#define main original_audit_main
#include "audit_output_test.cpp"
#undef main
#include "transientrelayconditions.h"

static void taskTriggerSource()
{
    for (const auto& descriptor : TestSequenceTaskKind::descriptors) {
        const auto kind = descriptor.kind;
        if (!TestSequenceTaskKind::isSteady(kind) &&
            !(TestSequenceTaskKind::isTransient(kind) && !TestSequenceTaskKind::isDescent(kind))) continue;
        for (const QVariant value : {QVariant(0),QVariant(9),QVariant(1.5),QVariant(true),QVariant("1"),QVariant{}}) {
            auto invalid=task(descriptor.name); invalid.settings["trigger_channel"]=value;
            bool rejected=false;
            try { TaskSettings::decode(descriptor.name,invalid.settings); }
            catch (const std::invalid_argument&) { rejected=true; }
            require(rejected,"runtime accepted invalid Trigger source");
        }
        for (const QString mode : {"explicit","legacy","retry","prior-enable","stop","stop-read","stop-enabled","write-error",
                                   "stop-error","edge-error","echo","read-error","disabled","enabled-error","unavailable"}) {
            class SourceScope : public Scope {
            public:
                int source = 4, writes = 0;
                bool enabled = true;
                QString mode, error;
                QStringList calls;
                std::function<void()> cancel;
                bool applySettings(const OscilloscopeSettings& settings, QAtomicInt&, QString&,
                                   OscilloscopeConfigurationResult*) override {
                    calls << "WRITE";
                    for (const auto& channel : settings.channels) if (channel.index==1) enabled=channel.enabled;
                    return true;
                }
                QString lastError() const override { return error; }
                bool stopAcquisition(QString& out) override {
                    calls << "STOP"; if (mode=="stop-error") { out="stop failed"; return false; } return true;
                }
                bool selectEdgeTrigger(QString& out) override {
                    calls << "EDGE"; if (mode=="edge-error") { out="edge failed"; return false; } return true;
                }
                bool setEdgeTriggerChannel(int ch, QString& out) override {
                    calls << "SET"; ++writes;
                    if (mode=="stop") cancel();
                    if (mode=="write-error") { out="source write failed"; return false; }
                    if (mode!="echo") source=ch;
                    return true;
                }
                int triggerChannel() override {
                    calls << "READ"; error=mode=="read-error" ? "source read failed" : "";
                    if (mode=="stop-read") cancel();
                    return source;
                }
                bool isChannelEnabled(int ch) override {
                    calls << "ENABLED"; error=mode=="enabled-error" ? "channel state failed" : "";
                    if (mode=="stop-enabled") cancel();
                    return ch==1 && enabled && mode!="disabled";
                }
            } scope;
            scope.mode=mode;
            scope.enabled=mode!="prior-enable";
            auto c=context(); c.scope=&scope;
            auto p=task(descriptor.name); p.settings["trigger_channel"]=mode=="unavailable"?8:1;
            if (mode=="legacy") p.settings.remove("trigger_channel");
            if (mode=="retry") p.retry=1;
            const bool steady=TestSequenceTaskKind::isSteady(kind);
            if (steady) p.settings["auto_period"]=true;
            const bool pass=mode=="explicit" || mode=="legacy" || mode=="retry" || mode=="prior-enable";
            int on=0, executed=0, created=0, periods=0, relayIO=0;
            auto checkSource=[&] { require(scope.source==(mode=="legacy"?4:1),"test used stale trigger source"); };
            TestRunDependencies deps; deps.validateMeasurement=validateMeasurementConditions;
            deps.prepareConfiguration=[](const auto&,int,const QVariantMap& values,OscilloscopeSettings& out,QString&) {
                OscilloscopeSettings::Channel channel; channel.index=1; channel.enabled=values.value("ch1_enabled").toBool();
                out.channels.append(channel); return true;
            };
            deps.instruments.input=[&](const auto&,const auto&,InputAction action) { if(action==InputAction::PowerOn){++on;checkSource();} return InstrumentOperationResult{}; };
            deps.instruments.dcGroup=[&](const auto&,const auto&,InputAction action) { if(action==InputAction::PowerOn){++on;checkSource();} return InstrumentOperationResult{}; };
            deps.instruments.load=[&](const auto&,const auto&,int,const auto&,LoadAction action,bool) { if(action==LoadAction::LoadOn){++on;checkSource();} return InstrumentOperationResult{}; };
            deps.instruments.dynamic=[&](const auto&,const auto&,int,const auto&,DynamicLoadAction action,bool,bool) { if(action==DynamicLoadAction::LoadOn){++on;checkSource();} return InstrumentOperationResult{}; };
            deps.instruments.relay=[&](const auto&,const auto&,int,RelayAction) { ++relayIO; return InstrumentOperationResult{}; };
            deps.autoPeriod=[&](IScopeMeasurement*,QAtomicInt&,QString&,const OscCaptureObserver&,const VerticalScaleOptions&,const std::function<void(const QString&)>&) {++periods;checkSource();return true;};
            deps.strategy=[&](const MeasurementStrategyRequest& request) {
                ++created; checkSource();
                const auto ctx=std::get<ScopeMeasurementRequest>(request).context;
                return std::make_unique<Strategy>([&,ctx] {
                    ++executed; checkSource();
                    if (ctx.power) {
                        QString error; require(ctx.power(true,error),error); require(ctx.power(false,error),error);
                        require(ctx.relays(false,false,error),error); require(ctx.load(false,error),error);
                    }
                    MeasurementResult result; result.success=mode!="retry" || executed>1;
                    if(!result.success) result.error="injected retry";
                    scope.source=4; // The next Group member/retry must apply its own saved source again.
                    return result;
                });
            };
            TestRunService runner(deps); scope.cancel=[&] {runner.stop();};
            QVector<TestSequenceResultRecord> records;
            QObject::connect(&runner,&TestRunService::resultRecordsReady,[&](auto r){records=r;});
            TaskPayload first; first.task.name="Relay"; first.settings["relay_index"]=0;
            if(mode=="prior-enable") { first.task.name="Write Oscilloscope";first.settings={{"ch1_enabled",true}}; }
            runner.startTasks(mode=="unavailable"||mode=="prior-enable"?QVector<TaskPayload>{first,p}:QVector<TaskPayload>{p},c);
            require(!records.isEmpty(),"trigger source result missing");
            const auto label=QString(descriptor.name)+" / "+mode+": "+records.last().error;
            if(pass) {
                const int expected=(TestSequenceTaskKind::isGroup(kind)?2:1)+(mode=="retry"?1:0);
                require(records.last().status=="Pass" && executed==expected,label);
                require(scope.writes==(mode=="legacy"?0:expected),"source was not reapplied per member/attempt");
                require(periods==(steady?expected:0),"Auto Period did not execute after source preparation");
                if(mode!="legacy") require(scope.calls.mid(mode=="prior-enable"?1:0,5)==QStringList({"STOP","EDGE","SET","READ","ENABLED"}),"source preparation order");
                if(mode=="prior-enable") require(records.first().status=="Pass" && scope.enabled,"prior Write Scope did not enable source");
            } else {
                require(on==0 && executed==0 && created==0 && periods==0,"source failure/Stop entered output or strategy");
                require(records.last().status==(mode=="stop"||mode=="stop-read"||mode=="stop-enabled"?"Cancelled":"Fail"),label);
                if(mode=="unavailable") require(scope.calls.isEmpty() && records.last().attempt==0 &&
                    relayIO==0 && records.size()==2 && records.first().status=="Skipped","bad later channel touched IO");
            }
        }
    }
}

static void verticalMarginLifecycle()
{
    for (bool margin : {false, true}) for (bool group : {false, true}) for (bool autoPeriod : {false,true}) {
        class ScalingScope : public Scope {
        public:
            double scale=4;
            int oldScaleWrites=0;
            double position=-2;
            int zeroWrites=0;
            bool isChannelEnabled(int ch) override { return ch==1; }
            bool readVerticalRange(int ch,ScopeVerticalRange &range,QString &) override {
                require(ch==1,"queried disabled channel range");
                range={scale,0,10,position,-8,8}; return true;
            }
            bool setVerticalPosition(int,double value,QString &) override {
                if(value==0) ++zeroWrites;
                position=value; return true;
            }
            double getChannelScale(int) override { return scale; }
            bool setVerticalScale(int, double value, QString &) override {
                if (value==4) ++oldScaleWrites;
                scale=value; return true;
            }
        } scope;
        auto c=context(); c.scope=&scope;
        TestRunDependencies deps; deps.validateMeasurement=validateMeasurementConditions;
        deps.instruments.input=[](const auto &,const auto &,auto) { return InstrumentOperationResult{}; };
        deps.instruments.dcGroup=[](const auto &,const auto &,auto) { return InstrumentOperationResult{}; };
        deps.instruments.load=[](const auto &,const auto &,int,const auto &,auto,bool) { return InstrumentOperationResult{}; };
        int measurements=0;
        deps.autoPeriod=[&](IScopeMeasurement *,QAtomicInt &,QString &,const OscCaptureObserver &,const VerticalScaleOptions &options,const std::function<void(const QString&)>&) {
            require(options.margin==margin && bool(options.beforeChange),"Auto Period option missing");
            const bool firstMember=!group || measurements%2==0;
            require(scope.scale==(measurements==0?4:12),"Auto Period lost retained scope scale");
            const double target=measurements==0?8:12;
            if (margin) {
                require(bool(options.beforePositionChange),"Position snapshot missing from Auto Period");
                require(options.history && options.history->value(1).clippedScale==(firstMember?0:3),
                        "Auto Period did not receive the current attempt's fit history");
                (*options.history)[1].clippedScale=3;
                require(scope.position==(firstMember?0:2),"Auto Period inherited wrong Position");
                options.beforePositionChange(1,scope.position,1); scope.position=1;
            }
            options.beforeChange(1,scope.scale,target); scope.scale=target; return true;
        };
        deps.strategy=[&](const MeasurementStrategyRequest &request) {
            const auto options=std::get<ScopeMeasurementRequest>(request).context.verticalScale;
            require(options.margin==margin && bool(options.beforeChange),"vertical option missing from runtime request");
            return std::make_unique<Strategy>([&,options] {
                const bool firstMember=!group || measurements%2==0;
                require(scope.scale==(measurements==0?(autoPeriod?8:4):12),"new task lost previous scope scale");
                options.beforeChange(1,scope.scale,12);
                scope.scale=12; ++measurements;
                if (margin) {
                    require(bool(options.beforePositionChange),"Position snapshot missing from strategy");
                    require(options.history && options.history->value(1).clippedScale==(firstMember&&!autoPeriod?0:3),
                            "strategy did not share fit history with Auto Period / Group member");
                    (*options.history)[1].clippedScale=3;
                    require(scope.position==(autoPeriod?1:firstMember?0:2),"Group position restored early or task not restored");
                    options.beforePositionChange(1,scope.position,2); scope.position=2;
                }
                MeasurementResult result; result.success=true; return result;
            });
        };
        TestRunService runner(deps);
        const auto p=task(group?"Static Group Test":"Static Test");
        auto configured=p; configured.settings["vertical_margin"]=margin;
        configured.settings["auto_period"]=autoPeriod;
        for (int run=0;run<2;++run) {
            runner.prepareRun(); runner.startTasks({configured,configured},c);
            require(scope.scale==12 && scope.oldScaleWrites==0,"task or Run rewrote the original scale");
            require(scope.position==(margin?2:-2) && scope.zeroWrites==(margin?2*(run+1):0),
                    "Position was lost on exit or initialized more than once per attempt");
        }
        require(measurements==(group?8:4),"lifecycle test skipped measurements");
    }
}

static void verticalMarginFailures()
{
    for (bool stopDuringWrite : {false,true}) {
        class PartialInit : public Scope {
        public:
            QVector<double> positions{0,-2,3};
            QVector<int> zeroed;
            std::function<void()> cancel;
            int getTotalChannel() override { return 2; }
            bool readVerticalRange(int ch,ScopeVerticalRange &range,QString &) override {
                range={4,0,10,positions[ch],-8,8}; return true;
            }
            bool setVerticalPosition(int ch,double value,QString &e) override {
                positions[ch]=value; // Device can apply a write even if the response is lost.
                if (value==0) zeroed << ch;
                if (ch==2 && value==0) {
                    if (cancel) cancel();
                    else { e="initial Position rejected"; return false; }
                }
                return true;
            }
        } scope;
        TestRunDependencies deps; deps.validateMeasurement=validateMeasurementConditions;
        int starts=0;
        deps.instruments.input=[&](const auto &,const auto &,InputAction a) {
            if (a==InputAction::PowerOn) ++starts;
            return InstrumentOperationResult{};
        };
        deps.instruments.dcGroup=[&](const auto &,const auto &,InputAction a) {
            if (a==InputAction::PowerOn) ++starts;
            return InstrumentOperationResult{};
        };
        deps.instruments.load=[](const auto &,const auto &,int,const auto &,auto,bool) {return InstrumentOperationResult{};};
        deps.instruments.relay=[](const auto &,const auto &,int,RelayAction) {return InstrumentOperationResult{};};
        deps.strategy=[&](const MeasurementStrategyRequest &) { ++starts; return std::unique_ptr<IMeasurementStrategy>{}; };
        TestRunService runner(deps);
        if (stopDuringWrite) scope.cancel=[&] {runner.stop();};
        auto c=context(); c.scope=&scope;
        QVector<TestSequenceResultRecord> records;
        QObject::connect(&runner,&TestRunService::resultRecordsReady,[&](auto r){records=r;});
        auto p=task("Turn off"); p.settings["vertical_margin"]=true;
        runner.startTasks({p},c);
        require(starts==0 && scope.zeroed==QVector<int>({1,2}) && records.size()==1 &&
                records[0].status==(stopDuringWrite?"Cancelled":"Fail") &&
                scope.positions==QVector<double>({0,0,0}),"partial initialization: "+records.value(0).error);
    }
    for (const QString mode : {"failure","stop","auto_period_failure","position_only_failure","stop_error"}) {
        class ScalingScope : public Scope {
        public:
            double scale=4, position=-2;
            int scaleWrites=0, resets=0, failedStops=0;
            int stopsAfterMeasurement=0;
            bool rejectStop=false;
            bool isChannelEnabled(int ch) override { return ch==1; }
            bool readVerticalRange(int,ScopeVerticalRange &r,QString &) override {
                r={scale,0,10,position,-8,8}; return true;
            }
            bool setVerticalPosition(int,double v,QString &) override {
                if (v==0) ++resets;
                position=v; return true;
            }
            bool setVerticalScale(int,double v,QString &) override { ++scaleWrites; scale=v; return true; }
            bool stopAcquisition(QString &e) override {
                if (!rejectStop || ++stopsAfterMeasurement==1) return true;
                ++failedStops; e="injected STOP failure"; return false;
            }
        } scope;
        auto c=context(); c.scope=&scope;
        TestRunDependencies deps; deps.validateMeasurement=validateMeasurementConditions;
        deps.instruments.input=[](const auto &,const auto &,auto) {return InstrumentOperationResult{};};
        deps.instruments.dcGroup=[](const auto &,const auto &,auto) {return InstrumentOperationResult{};};
        deps.instruments.load=[](const auto &,const auto &,int,const auto &,auto,bool) {return InstrumentOperationResult{};};
        int changes=0, periods=0;
        const bool positionOnly=mode=="position_only_failure";
        auto adjust=[&](const VerticalScaleOptions &options) {
            require(scope.scale==(changes && !positionOnly ? 8 : 4) && scope.position==0,
                    "attempt did not start from retained scale / initial Position zero");
            if (!positionOnly) {
                options.beforeChange(1,scope.scale,8);
                scope.scale=8;
            }
            options.beforePositionChange(1,scope.position,1);
            scope.position=1;
            ++changes;
        };
        deps.autoPeriod=[&](IScopeMeasurement *,QAtomicInt &,QString &e,const OscCaptureObserver &,
                           const VerticalScaleOptions &options,const std::function<void(const QString&)>&) {
            if (++periods==1) { adjust(options); e="injected Auto Period failure"; return false; }
            return true;
        };
        TestRunService *service=nullptr;
        deps.strategy=[&](const MeasurementStrategyRequest &request) {
            const auto options=std::get<ScopeMeasurementRequest>(request).context.verticalScale;
            return std::make_unique<Strategy>([&,options] {
                adjust(options);
                if (mode=="stop") service->stop();
                if (mode=="stop_error") scope.rejectStop=true;
                if ((mode=="failure" || positionOnly) && changes==1)
                    throw std::runtime_error("injected failure after vertical changes");
                MeasurementResult result; result.success=true; return result;
            });
        };
        TestRunService runner(deps); service=&runner;
        QVector<TestSequenceResultRecord> records;
        QObject::connect(&runner,&TestRunService::resultRecordsReady,[&](auto r) {records=r;});
        auto p=task("Static Test"); p.settings["vertical_margin"]=true; p.retry=1;
        p.settings["auto_period"]=mode=="auto_period_failure";
        runner.startTasks({p,p},c);
        require(scope.position==1 && scope.scale==(positionOnly?4:8) && scope.scaleWrites==0 &&
                scope.resets==changes,"exit rewrote vertical settings");
        if (mode=="stop" || mode=="stop_error") {
            require(records.size()==2 && records[1].status=="Skipped" && changes==1 &&
                    records[0].status==(mode=="stop"?"Cancelled":"Fail"),"Stop / STOP failure allowed retry or next row");
            if (mode=="stop_error") require(scope.failedStops==1 && records[0].error.contains("Scope STOP after vertical adjustment"),
                                            "hidden final STOP failure");
        } else {
            require(records.size()==3 && records[0].status=="Fail" && records[1].status=="Pass" &&
                    records[2].status=="Pass" && changes==3,"failed attempt did not retain settings through retry");
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
            taskTriggerSource();
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
