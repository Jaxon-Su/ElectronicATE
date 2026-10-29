#include "../../support/measurement_scope_fixture.h"
#include "../../../src/service/measurement/measurementformat.h"
#include "msosteadystartup.h"
#include "testrunservicefactory.h"
#include "testsequencescopepolicy.h"
#include "instrumentexecutor.h"
#include "dcinputoperation.h"
#include "communicationfactory.h"
#include "oscilloscopestrategyfactory.h"
#include "oscilloscopeconfiguration.h"
#include "oscilloscopeautoperiod.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <stdexcept>
#include <iostream>

namespace {
struct Fixture {
    QString scenario; QString expectedTarget; QStringList trace; TestRunService* worker=nullptr;
    int inputCalls=0,loadCalls=0,measures=0; bool sync=true,dirty=true;
} g;
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
InstrumentOperationResult operation(const QString& label) {
    g.trace<<label;
    if(label=="INPUT ON") {
        ++g.inputCalls;
        if(g.scenario=="cancel_input")g.worker->stop();
        if(g.scenario=="input_failure")return {false,"injected input failure"};
        if(g.scenario=="input_exception")throw std::runtime_error("input throw");
        if(g.scenario=="input_unknown")throw 1;
    }
    if(label=="LOAD ON") {
        ++g.loadCalls;
        if(g.scenario=="cancel_load")g.worker->stop();
        if(g.scenario=="load_failure" || (g.scenario=="retry"&&g.loadCalls==1))return {false,"injected load failure"};
        if(g.scenario=="load_exception")throw std::runtime_error("load throw");
    }
    if(label=="INPUT OFF" && g.inputCalls) {
        if(g.scenario=="cleanup_input")return {false,"input off failed"};
        if(g.scenario=="cleanup_exception")throw std::runtime_error("off throw");
        if(g.scenario=="cleanup_unknown")throw 2;
    }
    if(label=="LOAD OFF"&&g.inputCalls&&g.scenario=="cleanup_load")return {false,"load off failed"};
    return {};
}
class Scope : public MeasurementScopeFixture {
public:
    bool prepareSteadyAcquisition(QAtomicInt &stop, QString &error, bool startContinuous) override { return prepareMsoSteadyAcquisition(*this, stop, error, startContinuous); }
    QString getTriggerSource() override { return "CH1"; }
    int getTotalChannel() override { return 4; }
    bool isChannelEnabled(int channel) override { return channel == 1; }
    double getChannelScale(int) override { return 10; }
    QString model()const override{return g.scenario.startsWith("dpo7000") ? "DPO7000" : g.scenario.startsWith("dpo4000") ? "DPO4000" : "MSOSeries456";}
    QString vendor()const override{return "test";}
    QByteArray captureScreenshot(const QString&,const QString&)override{return {};}
    bool applySettings(const OscilloscopeSettings& settings,QAtomicInt&,QString& error, OscilloscopeConfigurationResult *result = nullptr)override {
        if (result) {
            *result = {};
            if (g.scenario != "write_missing_readback")
                result->horizontal = OscilloscopeConfigurationResult::Horizontal{"AUTO", {}, g.scenario == "write_adjusted" ? .002 : .001, 1e6, g.scenario == "write_adjusted" ? 20000.0 : 10000.0, 10};
        }
        require(settings.horizontal.secondsPerDivision==std::optional<double>(.001),"typed settings lost");
        return writeConfigurationCommand("WRITE CONFIG",error);
    }
    bool writeConfigurationCommand(const QString& cmd,QString& error)override {
        g.trace<<cmd;
        if(cmd=="WRITE CONFIG") {
            if(g.scenario=="write_success" || g.scenario=="write_adjusted" || g.scenario=="write_missing_readback" || g.scenario.startsWith("auto_")) return true;
            error="injected write failure"; return false;
        }
        if ((g.scenario=="auto_run_failure" && cmd=="ACQuire:STATE RUN")
            || (g.scenario=="auto_level_failure" && cmd.startsWith("TRIGger:A:LEVel:CH"))) {
            error="startup rejected"; return false;
        }
        if(cmd=="TRIGger:A:MODe AUTO") {
            require(g.trace.contains("SETTLING"),"AUTO before settling");
            if(g.scenario=="auto_trigger_cancel")g.worker->stop();
            if(g.scenario=="auto_trigger_failure"){error="AUTO rejected";return false;}
        }
        if(g.scenario=="cleanup_scope") {error="scope stop failed";return false;}
        return true;
    }
};
class Strategy : public IOscilloscopeMeasureStrategy {
public:
    QString name()const override{return "fault-injected strategy";}
    OscMeasureResult execute(IScopeMeasurement*,QAtomicInt&)override {
        ++g.measures;g.trace<<"MEASURE";
        if(g.scenario=="cancel_measure")g.worker->stop();
        if(g.scenario=="measure_exception")throw std::runtime_error("measure throw");
        if(g.scenario=="measure_unknown")throw 3;
        OscMeasureResult r;
        r.success=g.scenario.startsWith("auto_")||g.scenario=="success"||g.scenario=="retry"||g.scenario=="cancel_measure"||g.scenario=="cancel_result"||g.scenario=="dc_success"||g.scenario=="sequence_stop";
        r.errorMessage="injected acquisition failure";
        if(g.scenario=="transient_cleanup")r.cleanupFailed=true;
        OscChannelMeasure ch;ch.maxVal=100;ch.minVal=-100;r.channels.append(ch);
        r.trials.append({1, g.expectedTarget == "MIN" ? MeasurementDirection::Fall : MeasurementDirection::Rise,
                         1, 1, 0, {{ch.channel, {}, ch.maxVal, ch.minVal, ch.rms, ch.mean}}});
        return r;
    }
};
}
ICommunication* CommunicationFactory::create(const QString&){return nullptr;}
InstrumentOperationResult runDcGroup(const InstrumentSetupConfig&,const DcGroup& rows,InputAction a,const std::function<ICommunication*(const QString&)>&){
    require(rows[0].vin=="48","DC group snapshot lost");g.trace<<"DC GROUP";
    return operation(a==InputAction::PowerOn?"INPUT ON":a==InputAction::PowerOff?"INPUT OFF":"INPUT CONFIG");
}
InstrumentExecutor::Result InstrumentExecutor::runInput(const InstrumentSetupConfig&,const InputRow&,InputAction a){return operation(a==InputAction::PowerOn?"INPUT ON":a==InputAction::PowerOff?"INPUT OFF":"INPUT CONFIG");}
InstrumentExecutor::Result InstrumentExecutor::runLoad(const InstrumentSetupConfig&,const QVector<LoadDataRow>&,int,const LoadMetaRow&,LoadAction a,bool sync){g.sync=sync;return operation(a==LoadAction::LoadOn?"LOAD ON":a==LoadAction::LoadOff?"LOAD OFF":"LOAD CONFIG");}
InstrumentExecutor::Result InstrumentExecutor::runDynamicLoad(const InstrumentSetupConfig&,const QVector<DynamicDataRow>&,int,const DynamicMetaRow&,DynamicLoadAction a,bool sync,bool dirty){g.sync=sync;g.dirty=dirty;return operation(a==DynamicLoadAction::LoadOn?"LOAD ON":a==DynamicLoadAction::LoadOff?"LOAD OFF":"LOAD CONFIG");}
InstrumentExecutor::Result InstrumentExecutor::runRelay(const InstrumentSetupConfig&,const QVector<RelayDataRow>&,int,RelayAction action){
    g.trace << (action==RelayAction::RelayOn ? "RELAY ON" : "RELAY OFF");
    if(g.scenario=="relay_failure"&&action==RelayAction::RelayOn)return {false,"relay on failed"};
    if(g.scenario=="relay_cleanup"&&action==RelayAction::RelayOff)return {false,"relay off failed"};
    return {};
}
IOscilloscopeMeasureStrategy* OscilloscopeStrategyFactory::create(const QString& task,const TransientContext* ctx){ require(ctx && ctx->target==g.expectedTarget, "worker lost measure target"); return g.scenario=="missing_strategy"?nullptr:new Strategy;}
bool prepareOscilloscopeSettings(const QVariantMap&,const QString&,int,OscilloscopeSettings& settings,QString&) {
    settings.horizontal.secondsPerDivision=.001; return true;
}

bool adjustOscilloscopeAutoPeriod(IScopeAutoPeriod*,QAtomicInt&,QString& error,const OscCaptureObserver&,const VerticalScaleOptions&,const std::function<void(const QString&)>&) {
    g.trace<<"AUTO PERIOD";
    require(g.trace.contains("INPUT ON")&&g.trace.contains("LOAD ON"),"Auto Period ran before outputs");
    require(g.trace.contains("TRIGger:A:MODe AUTO"),"Auto Period started without AUTO mode");
    require(g.trace.contains("TRIGger:A:LEVel:CH1 0.4"),"Missing positive Scale-based startup level");
    require(g.trace.contains("TRIGger:A:EDGE:SLOpe RISE"),"Missing initial rising edge");
    require(g.trace.contains("ACQuire:STOPAfter RUNSTop") && g.trace.contains("ACQuire:STATE RUN"),"Auto Period started while stopped");
    if(g.scenario=="auto_cancel")g.worker->stop();
    if(g.scenario=="auto_failure"){error="period unavailable";return false;}
    return true;
}

int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    try {
        require(argc==3,"usage: worker_failure scenario dynamic");
        require(formatOscMeasurement(0.00001).toDouble() == 0.00001, "small value displayed as zero");
        require(formatOscMeasurement(-1e-12).toDouble() == -1e-12, "small negative value lost");
        require(formatOscMeasurement(1e20).toDouble() == 1e20, "large value lost");
        require(formatOscMeasurement(std::numeric_limits<double>::quiet_NaN()) == "NA", "NaN format");
        g.scenario=argv[1];const bool dynamic=QString(argv[2])=="1";g.expectedTarget=dynamic?"MIN":"MAX";
        if (g.scenario == "capture_removed") {
            auto ownedService = makeTestRunService();
            auto& worker = *ownedService;
            TaskPayload relay; relay.task.name = "Relay"; relay.settings["relay_index"] = 0;
            TaskPayload removed; removed.task.name = "Capture";
            QVector<TestSequenceResultRecord> records;
            QObject::connect(&worker, &TestRunService::resultRecordsReady, [&](const auto &value) { records = value; });
            TestSequenceExecutionContext context; context.relays.append(RelayDataRow{});
            worker.prepareRun(); worker.startTasks({relay, removed}, context);
            require(records.size() == 2 && records[0].status == "Skipped" && records[1].status == "Fail" &&
                    records[1].attempt == 0 && !g.trace.contains("RELAY ON"), "removed task allowed hardware operation");
            return 0;
        }
        for (const QString& name : {"Static Test", "Dynamic Test", "Turn on", "Turn off",
                 "Turn on then short", "Short then turn on", "Write Oscilloscope"}) {
            require(TestSequenceScopePolicy::requiresScope(name), "scope task omitted from policy");
        }
        require(!TestSequenceScopePolicy::requiresScope("Relay") && !TestSequenceScopePolicy::requiresScope("Delay"), "independent task requires scope");
        for (const auto& model : TriggerModelCatalog::mso456Models())
            require(TestSequenceScopePolicy::unavailableReason(model).isEmpty(), "MSO model blocked");
        for (const QString& model : {"DPO7000", "DPO4000", "MSO4000", "unknown", ""})
            require(!TestSequenceScopePolicy::unavailableReason(model).isEmpty(), "unsupported model allowed");
        Scope scope;TestSequenceExecutionContext context;context.scope=g.scenario=="no_scope"?nullptr:&scope;
        context.inputs.append(InputRow{});context.loads.append(LoadDataRow{});context.dynamics.append(DynamicDataRow{});
        context.loads[0].label = "Static snapshot";
        context.dynamics[0].label = "Dynamic snapshot";
        context.loadSync.enabled=false;context.loadSync.dirty=true;
        TaskPayload task;task.task.name=dynamic?"Dynamic Test":"Static Test";
        task.settings={{"ac_input_index",0},{"load_index",0},{"dyload_index",0},{"delay_ms",0},{"power_off_ms",0},{"startup_ms",0},{"measure_target",dynamic?"MIN":"MAX"}};
        if(g.scenario.startsWith("write_"))task.task.name="Write Oscilloscope";
        if(g.scenario=="bad_input")task.settings["ac_input_index"]=-1;
        if(g.scenario=="bad_load"){task.settings["load_index"]=-1;task.settings["dyload_index"]=-1;}
        if(g.scenario=="bad_delay")task.settings["delay_ms"]=-1;
        if(g.scenario=="cancel_settle")task.settings["delay_ms"]=20;
        if(g.scenario=="retry"||g.scenario.startsWith("cleanup_"))task.retry=2;
        if(g.scenario=="dc_success") {DcGroup group;group[0].vin="48";context.dcInputs.append(group);task.settings["ac_input_index"]=-1;task.settings["dc_input_index"]=0;}
        if(g.scenario=="transient_cleanup") {task.task.name="Turn off";task.retry=2;}
        if(g.scenario=="bad_retry")task.retry=-1;
        if(g.scenario=="large_retry")task.retry=101;
        if(g.scenario.startsWith("relay_")){task.task.name="Relay";task.settings["relay_index"]=0;context.relays.append(RelayDataRow{});}
        TaskPayload next;next.task.name="Delay";next.settings["delay_ms"]=(g.scenario=="sequence_stop"||g.scenario=="relay_stop")?50:0;
        QThread thread;auto* worker=makeTestRunService().release();g.worker=worker;worker->moveToThread(&thread);
        QObject::connect(&thread,&QThread::finished,worker,&QObject::deleteLater);
        QEventLoop loop;QVector<QPair<int,TaskStatus>> statuses;QStringList logs;int finished=0;
        QVector<TestSequenceResultRecord> records;
        QObject::connect(worker,&TestRunService::resultRecordsReady,&loop,[&](const QVector<TestSequenceResultRecord>& values){records = values;});
        QVector<TestSequenceResultRecord> measurements;
        QObject::connect(worker,&TestRunService::measurementReady,&loop,
            [&](int, const TestSequenceResultRecord &record, bool){ measurements.append(record); });
        QStringList results;
        QObject::connect(worker,&TestRunService::taskResultChanged,&loop,[&](int, const QString& text){results << text;});

        QObject::connect(worker,&TestRunService::taskStatusChanged,&loop,[&](int i,TaskStatus s){statuses.append({i,s});});
        QObject::connect(worker,&TestRunService::logMessage,&loop,[&](const QString& s){logs<<s;});
        // Direct fault injection at an execution boundary; Stop itself is atomic.
        QObject::connect(worker,&TestRunService::logMessage,worker,[&](const QString& s){
            if(s.contains("settling"))g.trace<<"SETTLING";
            if((g.scenario=="sequence_stop"||g.scenario=="relay_stop")&&s.contains("  Delay:"))worker->stop();
            if(g.scenario=="cancel_settle"&&s.contains("settling"))worker->stop();
            if(g.scenario=="cancel_result"&&s.contains("CH1"))worker->stop();
        },Qt::DirectConnection);
        QObject::connect(worker,&TestRunService::finished,&loop,[&]{++finished;loop.quit();});
        worker->prepareRun();if(g.scenario=="cancel_before")worker->stop();
        QVector<TaskPayload> tasks{task,next};
        if(g.scenario.startsWith("auto_")) {
            tasks[0].settings["auto_period"] = g.scenario != "auto_off";
            TaskPayload setup;setup.task.name="Write Oscilloscope";
            tasks.prepend(setup);
            if(g.scenario=="auto_off"||g.scenario=="auto_ignore") {
                setup.settings["ignore_horizontal"]=(g.scenario=="auto_ignore");
                tasks.insert(1,setup);
            }
        }
        if(g.scenario.endsWith("_batch")) {
            TaskPayload relay; relay.task.name="Relay"; relay.settings["relay_index"]=0;
            context.relays = {{"off", {"OFF"}}}; // Exercise unsupported-scope rejection after a valid Relay row.
            tasks.prepend(relay);
        }
        if(g.scenario.endsWith("_independent")) {
            task.task.name="Relay";task.settings["relay_index"]=0;
            context.relays.append(RelayDataRow{});tasks={task,next};
        }
        thread.start();QMetaObject::invokeMethod(worker,[=]{worker->startTasks(tasks,context);},Qt::QueuedConnection);
        QTimer deadline;deadline.setSingleShot(true);QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);deadline.start(5000);
        loop.exec();worker->stop();thread.quit();thread.wait();
        require(finished==1,"worker did not finish exactly once");
        for (int i = 0; i < tasks.size(); ++i) {
            bool found = false;
            for (const auto& record : records) if (record.taskIndex == i) {
                found = true;
                require(!record.runId.isEmpty() && record.finishedAt.isValid(), "invalid record identity");
                if (!TestSequenceTaskKind::isMeasurement(record.taskName) || record.attempt == 0)
                    require(record.loadCondition.index == -1, "unexecuted/non-measurement task invented a load");
                else if (g.scenario != "bad_load") {
                    const bool dynamicResult = record.taskName == "Dynamic Test";
                    require(record.loadCondition.index == 0 &&
                            record.loadCondition.label == (dynamicResult ? "Dynamic snapshot" : "Static snapshot") &&
                            record.loadCondition.kind == (dynamicResult ? LoadConditionKind::Dynamic : LoadConditionKind::Static),
                            "attempt lost its load snapshot");
                }
            }
            require(found, "task missing from Run history");
        }
        if (g.scenario == "cancel_before")
            for (const auto& record : records)
                require(record.status == "Skipped" && record.attempt == 0, "pre-stop history incorrect");
        if (g.scenario == "cancel_input")
            require(records[0].status == "Cancelled" && records[1].status == "Skipped", "cancelled/skipped history incorrect");
        if (g.scenario == "write_failure")
            require(records[0].error.contains("injected write failure"), "write error detail lost");
        if (g.scenario == "input_failure")
            require(records[0].error.contains("injected input failure"), "input error detail lost");
        if (g.scenario == "relay_cleanup")
            require(records[0].error.contains("relay off failed"), "deferred cleanup error lost");
        if (g.scenario == "success" || g.scenario == "retry") {
            require(!records.isEmpty(), "missing structured records");
            const auto& last = records[g.scenario == "retry" ? 1 : 0];
            require(last.status == "Pass" && last.attempt == (g.scenario == "retry" ? 2 : 1), "attempt history lost");
            require(last.channels.size() == 1 && last.channels[0].maximum == 100.0 && std::isnan(last.channels[0].rms), "raw measurement lost");
            require(last.startedAt.isValid() && last.finishedAt >= last.startedAt && !last.runId.isEmpty(), "record identity/time missing");
            if (g.scenario == "retry") require(records[0].status == "Fail", "retry overwrote failed attempt");
        }

        if (g.scenario == "write_success") require(results.contains("Oscilloscope settings applied"), "missing write summary");
        if (g.scenario == "write_success" || g.scenario == "write_adjusted") {
            require(logs.join("\n").contains("requested:") && logs.join("\n").contains("actual:")
                    && logs.join("\n").contains("Sample Rate=1000000 S/s") && logs.join("\n").contains("RL="), "horizontal readback log missing");
            require(logs.join("\n").contains("adjusted by instrument") == (g.scenario == "write_adjusted"), "adjustment log incorrect");
            if (g.scenario == "write_adjusted") require(results.contains("Oscilloscope settings applied (adjusted)"), "adjustment summary missing");
        }
        if (g.scenario == "write_missing_readback") require(!results.contains("Oscilloscope settings applied"), "missing readback reported success");
        if (g.scenario == "relay_success") require(results.join("\n").contains("Applied:"), "missing relay summary");
        if (g.scenario == "success") require(results.join("\n").contains("Waited "), "missing delay summary");
        require(!results.isEmpty() && results.first().isEmpty(), "previous run results not cleared");
        if (g.scenario == "success") {
            require(!measurements.isEmpty() && measurements[0].channels[0].maximum==100 && measurements[0].channels[0].minimum==-100 && std::isnan(measurements[0].channels[0].rms), "numeric measurement not published");
        }
        if (g.scenario == "input_failure") {
            require(results.contains("NA"), "failed measurement missing NA result");
            require(!results.join("\n").contains("CH1"), "failed input published stale measurement");
        }

        if(g.scenario.startsWith("auto_")) {
            const bool triggerFailed=g.scenario=="auto_trigger_failure"||g.scenario=="auto_trigger_cancel"
                ||g.scenario=="auto_run_failure"||g.scenario=="auto_level_failure";
            const bool fail=g.scenario=="auto_failure"||g.scenario=="auto_cancel"||triggerFailed;
            const int index=(g.scenario=="auto_off"||g.scenario=="auto_ignore")?2:1;
            require(statuses.contains({index,fail?TaskStatus::Fail:TaskStatus::Pass}),"deferred Auto Period result");
            require(g.trace.count("AUTO PERIOD")== (g.scenario=="auto_off"||triggerFailed?0:1),"Auto Period setting lost");
            if(g.trace.contains("AUTO PERIOD"))require(g.trace.indexOf("LOAD ON")<g.trace.indexOf("AUTO PERIOD"),"period before load");
            require(g.measures==(fail?0:1),"measurement after period failure/cancel");
            require(g.trace.contains("INPUT OFF")&&g.trace.contains("LOAD OFF"),"period cleanup missing");
            std::cout<<"PASS deferred period\n";return 0;
        }
        if(g.scenario.startsWith("dpo")) {
            if(g.scenario.endsWith("_independent"))
                require(statuses.contains({0,TaskStatus::Pass}) && statuses.contains({1,TaskStatus::Pass})
                        && g.trace.contains("RELAY OFF"), "DPO blocked independent tasks");
            else {
                require(g.trace.isEmpty(), "unsupported batch touched hardware");
                require(statuses.size()==1 && statuses[0].first==(g.scenario.endsWith("_batch")?1:0)
                        && statuses[0].second==TaskStatus::Fail, "unsupported batch was not rejected");
            }
            std::cout<<"PASS model gate\n";return 0;
        }
        const bool success=g.scenario=="write_success"||g.scenario=="write_adjusted"||g.scenario=="success"||g.scenario=="retry"||g.scenario=="dc_success"||g.scenario=="sequence_stop"||g.scenario=="relay_success"||g.scenario=="relay_stop";
        if(g.scenario=="cancel_before") {require(statuses.isEmpty()&&g.trace.isEmpty(),"pre-stop executed task");}
        else {
            require(statuses.contains({0,success?TaskStatus::Pass:TaskStatus::Fail}),"incorrect task result");
            if(!success&&g.scenario!="relay_cleanup")require(!statuses.contains({0,TaskStatus::Pass}),"failure reported Pass");
        }
        if(g.scenario.startsWith("write_"))require(g.trace==QStringList{"WRITE CONFIG"},"Write task dispatch changed");
        const bool touched=g.inputCalls>0;
        if(touched&&g.scenario!="transient_cleanup") {
            require(g.trace.contains("ACQuire:STATE STOP")&&g.trace.contains("INPUT OFF")&&g.trace.contains("LOAD OFF"),"failure skipped cleanup");
            require(g.trace.lastIndexOf("ACQuire:STATE STOP")<g.trace.lastIndexOf("LOAD OFF")&&g.trace.lastIndexOf("LOAD OFF")<g.trace.lastIndexOf("INPUT OFF"),"cleanup order");
        }
        if(g.scenario=="cancel_input")require(g.loadCalls==0&&g.measures==0,"Stop after input still enabled load");
        if(g.scenario=="cancel_load"||g.scenario=="cancel_settle")require(g.measures==0,"Stop still measured");
        if(g.scenario.startsWith("cleanup_")) {
            require(g.inputCalls==1&&!statuses.contains({1,TaskStatus::Running}),"cleanup failure retried or ran next task");
            require(logs.join('\n').contains("Cleanup failed"),"cleanup failure missing log");
        }
        if(g.scenario.startsWith("cancel_"))require(!statuses.contains({1,TaskStatus::Running}),"cancel ran next task");
        if(g.scenario=="no_scope"||g.scenario.startsWith("bad_")||g.scenario=="missing_strategy")require(!touched,"invalid prerequisites enabled output");
        if(g.scenario=="success"||g.scenario=="sequence_stop"||g.scenario=="dc_success")
            require(g.trace.contains("INPUT OFF")&&g.trace.contains("LOAD OFF"),"Run completion/Stop left output enabled");
        if(g.scenario=="transient_cleanup")require(g.measures==1&&!statuses.contains({1,TaskStatus::Running}),"transient cleanup failure retried");
        if(g.scenario=="dc_success")require(g.trace.count("DC GROUP")==4,"DC OFF/configure/ON/OFF not routed");
        if(g.scenario=="bad_retry"||g.scenario=="large_retry")require(!touched,"invalid retry executed");
        if(g.scenario=="retry")require(g.inputCalls==2&&g.loadCalls==2&&g.trace.indexOf("LOAD OFF")<g.trace.lastIndexOf("INPUT ON"),"retry before cleanup");
        if(g.scenario.startsWith("relay_"))require(g.trace.contains("RELAY ON")&&g.trace.contains("RELAY OFF"),"relay outputs left active");
        if(g.loadCalls)require(!g.sync,"sync parameter changed");
        std::cout<<"PASS "<<task.task.name.toStdString()<<" "<<g.scenario.toStdString()<<"\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}


// These fault fixtures isolate orchestration; measurement_preflight_test uses the real validator.
void validateMeasurementConditions(const TestSequenceExecutionContext &, InputSelection, int, bool) {}
