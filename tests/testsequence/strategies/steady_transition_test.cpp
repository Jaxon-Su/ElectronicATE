#define main original_audit_main
#include "audit_output_test.cpp"
#undef main
#include "transientrelayconditions.h"

static void verticalMarginLifecycle()
{
    for (bool margin : {false, true}) for (bool group : {false, true}) for (bool autoPeriod : {false,true}) {
        class ScalingScope : public Scope {
        public:
            double scale=4;
            int restores=0;
            double getChannelScale(int) override { return scale; }
            bool setVerticalScale(int, double value, QString &) override {
                if (value==4) ++restores;
                scale=value; return true;
            }
        } scope;
        auto c=context(); c.scope=&scope;
        TestRunDependencies deps; deps.validateMeasurement=validateMeasurementConditions;
        deps.instruments.input=[](const auto &,const auto &,auto) { return InstrumentOperationResult{}; };
        deps.instruments.dcGroup=[](const auto &,const auto &,auto) { return InstrumentOperationResult{}; };
        deps.instruments.load=[](const auto &,const auto &,int,const auto &,auto,bool) { return InstrumentOperationResult{}; };
        int measurements=0;
        deps.autoPeriod=[&](IScopeMeasurement *,QAtomicInt &,QString &,const OscCaptureObserver &,const VerticalScaleOptions &options) {
            require(options.margin==margin && bool(options.beforeChange),"Auto Period option missing");
            const bool firstMember=!group || measurements%2==0;
            require(scope.scale==(firstMember?4:12),"Auto Period inherited wrong original scale");
            const double target=firstMember?8:12;
            options.beforeChange(1,scope.scale,target); scope.scale=target; return true;
        };
        deps.strategy=[&](const MeasurementStrategyRequest &request) {
            const auto options=std::get<ScopeMeasurementRequest>(request).context.verticalScale;
            require(options.margin==margin && bool(options.beforeChange),"vertical option missing from runtime request");
            return std::make_unique<Strategy>([&,options] {
                const bool firstMember=!group || measurements%2==0;
                require(scope.scale==(firstMember?(autoPeriod?8:4):12),"new task inherited expanded scale, or Group shrank early");
                options.beforeChange(1,scope.scale,12);
                scope.scale=12; ++measurements;
                MeasurementResult result; result.success=true; return result;
            });
        };
        TestRunService runner(deps);
        const auto p=task(group?"Static Group Test":"Static Test");
        auto configured=p; configured.settings["vertical_margin"]=margin;
        configured.settings["auto_period"]=autoPeriod;
        for (int run=0;run<2;++run) {
            runner.prepareRun(); runner.startTasks({configured,configured},c);
            require(scope.scale==4 && scope.restores==2*(run+1),"task/Run did not restore original scale");
        }
        require(measurements==(group?8:4),"lifecycle test skipped measurements");
    }
}

static void verticalMarginFailures()
{
    for (const QString mode : {"failure","stop","restore","stop_restore","auto_period_failure"}) {
        class ScalingScope : public Scope {
        public:
            QMap<int,double> scales{{1,4},{2,2}};
            QVector<int> restores;
            bool reject=false;
            double getChannelScale(int ch) override {return scales[ch];}
            bool setVerticalScale(int ch,double value,QString &error) override {
                restores<<ch;
                if (reject && ch==1) {error="injected restore failure";return false;}
                scales[ch]=value;return true;
            }
        } scope;
        auto c=context();c.scope=&scope;
        TestRunDependencies deps; deps.validateMeasurement=validateMeasurementConditions;
        deps.instruments.input=[](const auto &,const auto &,auto) {return InstrumentOperationResult{};};
        deps.instruments.dcGroup=[](const auto &,const auto &,auto) {return InstrumentOperationResult{};};
        deps.instruments.load=[](const auto &,const auto &,int,const auto &,auto,bool) {return InstrumentOperationResult{};};
        int calls=0,periodCalls=0;
        auto expand=[&](const VerticalScaleOptions &options) {
            require(scope.scales[1]==4 && scope.scales[2]==2,"retry inherited expansion");
            options.beforeChange(1,4,8);scope.scales[1]=8;
            options.beforeChange(2,2,4);scope.scales[2]=4;
        };
        TestRunService *service=nullptr;
        deps.autoPeriod=[&](IScopeMeasurement *,QAtomicInt &,QString &error,const OscCaptureObserver &,const VerticalScaleOptions &options) {
            if (++periodCalls==1) {expand(options);error="injected Auto Period failure";return false;}
            return true;
        };
        deps.strategy=[&](const MeasurementStrategyRequest &request) {
            const auto options=std::get<ScopeMeasurementRequest>(request).context.verticalScale;
            return std::make_unique<Strategy>([&,options] {
                ++calls;expand(options);
                if (mode.contains("restore")) scope.reject=true;
                if (mode.startsWith("stop")) service->stop();
                if (mode=="failure" && calls==1) throw std::runtime_error("injected measurement failure after scale write");
                MeasurementResult result;result.success=true;return result;
            });
        };
        TestRunService runner(deps);service=&runner;
        QVector<TestSequenceResultRecord> records;
        QObject::connect(&runner,&TestRunService::resultRecordsReady,[&](auto r) {records=r;});
        auto configured=task("Static Test");configured.settings["vertical_margin"]=true;configured.retry=1;
        configured.settings["auto_period"]=mode=="auto_period_failure";
        runner.startTasks({configured,configured},c);
        if (mode.contains("restore")) {
            require(calls==1 && scope.restores==QVector<int>({1,2}) && scope.scales[2]==2 && records.size()==2 &&
                    records[0].error.contains("restore failed") && records[1].status=="Skipped",
                    "restore failure skipped healthy channel or continued/retried");
        } else if (mode=="stop") {
            require(calls==1 && scope.scales[1]==4 && scope.scales[2]==2 && records[0].status=="Cancelled" &&
                    records[1].status=="Skipped","Stop skipped scale cleanup or continued");
        } else {
            require(records.size()==3 && records[0].status=="Fail" && records[1].status=="Pass" && records[2].status=="Pass" &&
                    scope.scales[1]==4 && scope.scales[2]==2 && scope.restores.size()==6,
                    "failed attempt/Auto Period lost restore before retry");
        }
    }
}

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
            verticalMarginLifecycle();
            verticalMarginFailures();
            require(!TaskSettings::Steady({}).verticalMargin && !TaskSettings::Transient({}).verticalMargin,
                    "legacy task enabled margin");
            for (QVariant value : {QVariant(1),QVariant("true"),QVariant()}) {
                bool rejected=false;
                try {TaskSettings::Steady invalid(QVariantMap{{"vertical_margin",value}});}
                catch (const std::invalid_argument &) {rejected=true;}
                require(rejected,"runtime accepted nonboolean margin");
            }
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
