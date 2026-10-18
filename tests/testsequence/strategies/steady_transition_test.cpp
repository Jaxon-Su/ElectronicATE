#define main original_audit_main
#include "audit_output_test.cpp"
#undef main
#include "transientrelayconditions.h"

static void transition(const QString &mode, bool grouped, bool dynamic, bool sync)
{
    auto c = context(); Scope scope; c.scope = &scope; c.loadSync = {sync, true};
    // Start from unknown externally live outputs; the first condition must also establish OFF.
    bool ac = true, dc = true, loaded = true, configured = false;
    int measurements = 0, sourceOns = 0, loadOns = 0, loadOffs = 0, inputOffs = 0, complete = 0;
    QStringList events; QVector<TableKind> uncertain; QVector<TestSequenceResultRecord> records;
    TestRunDependencies d; d.validateMeasurement = validateMeasurementConditions;
    TestRunService *service = nullptr;
    auto input = [&](bool &state, InputAction action) {
        if (action == InputAction::PowerOff) {
            require(!loaded || mode == "off_failure" || mode == "cleanup_failure", "input switched off under load");
            ++inputOffs; state = false; events << "input off";
        } else if (action == InputAction::Change) {
            require(!loaded && !ac && !dc, "source reprogrammed while live"); events << "input config";
            if (mode == "config_failure") return InstrumentOperationResult{false,"configuration failed"};
        } else {
            require(!loaded && configured, "source enabled before offline load setup");
            state = true; ++sourceOns; events << "input on";
        }
        return InstrumentOperationResult{};
    };
    d.instruments.input = [&](const auto &, const auto &, InputAction a) { return input(ac,a); };
    d.instruments.dcGroup = [&](const auto &, const auto &, InputAction a) { return input(dc,a); };
    auto load = [&](int action, bool dirty) {
        if (action == 1) { // OFF
            ++loadOffs; events << "load off";
            if (mode == "off_failure" || (mode == "cleanup_failure" && measurements))
                return InstrumentOperationResult{false,"load OFF failed"};
            loaded = false;
        } else if (action == 2) { // Change
            require(!loaded && !ac && !dc, "load reprogrammed while live");
            configured = true; events << "load config";
        } else { // ON
            require(ac && dc && !loaded && configured && events.last() == "startup wait", "load enabled before startup wait");
            if (dynamic && loadOns == 0) require(dirty, "preparation consumed pending sync update");
            if (dynamic && loadOns > 0) require(!dirty, "successful ON failed to consume sync update");
            loaded = true; ++loadOns; events << "load on";
        }
        return InstrumentOperationResult{};
    };
    d.instruments.load = [&](const auto &,const auto &,int,const auto &,LoadAction a,bool s) {
        require(s == sync, "sync setting lost"); return load(static_cast<int>(a),true);
    };
    d.instruments.dynamic = [&](const auto &,const auto &,int,const auto &,DynamicLoadAction a,bool s,bool dirty) {
        require(s == sync, "dynamic sync setting lost"); return load(static_cast<int>(a),dirty);
    };
    d.strategy = [&](const MeasurementStrategyRequest &) {
        return std::make_unique<Strategy>([&] {
            require(ac && dc && loaded && events.last() == "settled", "measured before load settling");
            ++measurements; events << "measure";
            if (mode == "stop_measure") service->stop();
            MeasurementResult r; r.success = true; return r;
        });
    };
    TestRunService runner(d); service = &runner;
    QObject::connect(&runner,&TestRunService::logMessage,[&](const QString &text) {
        if (text.contains(": power-off wait")) {
            require(!loaded && !ac && !dc, "off delay started before OFF"); events << "off wait";
            if (mode == "stop_off") runner.stop();
        }
        if (text.contains(": startup wait")) {
            require(ac && dc && !loaded,"startup occurred under load"); events << "startup wait";
            if (mode == "stop_startup") runner.stop();
        }
        if (text.contains(": settling")) { events << "settled"; if (mode == "stop_settle") runner.stop(); }
    });
    QObject::connect(&runner,&TestRunService::measurementReady,[&](int,const auto &record,bool done) {
        if (done) {
            require(!ac && !dc && !loaded,"completion published before OFF");
            if (record.finishedAt.isValid()) ++complete; // Count final attempts, not progress snapshots.
        }
    });
    QObject::connect(&runner,&TestRunService::outputsUncertain,[&](const auto &,auto kinds) { uncertain=kinds; });
    QObject::connect(&runner,&TestRunService::resultRecordsReady,[&](auto r) { records=r; });
    const QString name = dynamic ? (grouped ? "Dynamic Group Test" : "Dynamic Test") : (grouped ? "Static Group Test" : "Static Test");
    auto p=task(name); p.settings["power_off_ms"]=1; p.settings["startup_ms"]=1; p.settings["delay_ms"]=1;
    // Use multiple tasks and Group members to exercise every handoff.
    auto second=p; if (!grouped) second.settings[dynamic ? "dyload_index" : "load_index"]=1;
    if (mode.endsWith("failure")) p.retry=1;
    runner.startTasks({p,second},c);
    if (mode == "success") {
        require(measurements==(grouped?4:2) && complete==(grouped?2:2) && records.size()==2,"conditions were skipped");
        require(records[0].status=="Pass" && records[1].status=="Pass" && uncertain.isEmpty(),"successful transition failed");
    } else if (mode == "off_failure" || mode == "cleanup_failure") {
        require(records.size()==2 && records[0].status=="Fail" && records[1].status=="Skipped" && complete==0,"OFF failure continued/retried or reported complete");
        require(uncertain.contains(dynamic?TableKind::DyLoad:TableKind::Load) && inputOffs>=2,"OFF failure lost UNKNOWN or skipped healthy source cleanup");
    } else if (mode.startsWith("stop_")) {
        require(records[0].status=="Cancelled" && records[1].status=="Skipped" && complete==0,"Stop completed or continued");
        if (mode=="stop_off") require(sourceOns==0 && loadOns==0,"OFF wait cancellation still enabled output");
        if (mode=="stop_startup") require(loadOns==0,"startup cancellation still enabled load");
    } else {
        require(measurements==0 && sourceOns==0 && loadOns==0 && complete==0,"configuration failure enabled output");
    }
    require(!ac && !dc && (mode=="off_failure" || mode=="cleanup_failure" || !loaded),"task left outputs ON");
}

static void relayPreflight(const QString &mode, bool grouped)
{
    auto c=context(); Scope scope; c.scope=&scope; int io=0;
    auto p=task(grouped ? "Turn on then short Group Test" : "Turn on then short");
    if (mode=="all_off") c.relays[1].values={"OFF","OFF"};
    if (mode=="overlap") p.settings["discharge_index"]=0;
    if (mode=="unmapped") c.instrumentSetup.instruments.last().channels[1].index=0;
    if (mode=="invalid_value") c.relays[1].values[1]="INVALID";
    if (mode=="size") c.relays[1].values={"ON"};
    if (mode=="index") p.settings["relay_index"]=100;
    TestRunDependencies d; d.validateMeasurement=validateMeasurementConditions;
    d.instruments.input=[&](const auto &,const auto &,auto) { ++io; return InstrumentOperationResult{}; };
    d.instruments.dcGroup=[&](const auto &,const auto &,auto) { ++io; return InstrumentOperationResult{}; };
    d.instruments.load=[&](const auto &,const auto &,int,const auto &,auto,bool) { ++io; return InstrumentOperationResult{}; };
    d.instruments.relay=[&](const auto &,const auto &,int,auto) { ++io; return InstrumentOperationResult{}; };
    d.strategy=[&](const MeasurementStrategyRequest &) { ++io; return std::make_unique<Strategy>([] { return MeasurementResult{}; }); };
    TestRunService runner(d); QVector<TestSequenceResultRecord> records;
    QObject::connect(&runner,&TestRunService::resultRecordsReady,[&](auto r) { records=r; });
    runner.startTasks({task("Static Test"),p},c);
    require(io==0 && records.size()==2 && records[0].status=="Skipped" && records[1].status=="Fail" && records[1].attempt==0,"invalid relay condition reached hardware before whole-batch rejection");
}

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    try {
        const QString mode=app.arguments().value(1);
        if (mode=="settings") {
            const TaskSettings::Steady defaults({});
            require(defaults.powerOffMs==5000 && defaults.startupMs==5000,"missing fields bypass transition waits");
            for (const QString key : {"power_off_ms","startup_ms"}) {
                for (QVariant value : {QVariant(-1),QVariant(3600001),QVariant(0.5),QVariant(true),QVariant("100")}) {
                    bool rejected=false;
                    try { TaskSettings::Steady invalid(QVariantMap{{key,value}}); }
                    catch (const std::invalid_argument &) { rejected=true; }
                    require(rejected,"invalid wait accepted at execution boundary");
                }
            }
            std::cout<<"PASS settings"; return 0;
        }
        for (bool group : {false,true}) {
            if (mode.startsWith("relay_")) relayPreflight(mode.mid(6),group);
            else for (bool dynamic : {false,true}) for (bool sync : {false,true}) transition(mode,group,dynamic,sync);
        }
        std::cout << "PASS " << mode.toStdString();
    } catch (const std::exception &e) { std::cerr<<e.what(); return 1; }
}
