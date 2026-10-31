#include "iscopemeasurement.h"
#include "steadystatesearchstrategy.h"
#include "transientstrategy.h"
#include "oscilloscopeacquisition.h"
#include "measurementstrategyfactory.h"
#include "oscilloscopestrategyfactory.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSet>
#include <QThread>
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }

// Deliberately has no model name, transport, command strings or Oscilloscope base.
class SemanticScope : public IScopeMeasurement {
public:
    bool beginAutoReference(int &budget, QString &error) override {
        budget=10000;
        if (beginSingleAcquisition()) return true;
        error="reference arm failed";return false;
    }
    ScopeReferenceStatus pollAutoReference(QString &state, QString &error) override {
        const auto current=acquisitionState();state="mode=AUTO";
        if(current==ScopeAcquisitionState::Invalid){error="reference state invalid";return ScopeReferenceStatus::Error;}
        return current==ScopeAcquisitionState::Completed?ScopeReferenceStatus::Completed:ScopeReferenceStatus::Pending;
    }
    QString lastError() const override { return lastFailure; }
    int triggerChannel() override { return 2; }
    int getTotalChannel() override { return 2; }
    bool isChannelEnabled(int channel) override { lastFailure.clear(); return channel == 2; }
    bool isClipping(int) override { return clipped; }
    double getChannelScale(int) override { return scale; }
    void stop() override { armed = false; ++stops; }
    bool stopAcquisition(QString &error) override {
        stop(); if (failStop) { error = "stop failed"; return false; } return true;
    }
    bool selectEdgeTrigger(QString &) override {
        if (cancelOnConfigure) cancelOnConfigure->storeRelease(1);
        return true;
    }
    bool setEdgeSlope(ScopeTriggerSlope value, QString &error) override {
        if (rejectSlope) { error = "slope rejected"; return false; }
        slope = value; return true;
    }
    bool setTriggerMode(ScopeTriggerMode value, QString &) override {
        mode = value; ++modes; return true;
    }
    bool setChannelTriggerLevel(int channel, double value, QString &) override {
        requireArchived();
        require(channel == 2, "strategy guessed CH1"); level = value; return true;
    }
    bool setVerticalScale(int channel, double value, QString &) override {
        requireArchived();
        require(channel == 2, "rescaled wrong channel"); scale = value; clipped = false; return true;
    }
    bool beginSingleAcquisition() override {
        requireArchived(); armed = !failArm; event = false; ++records;
        lastFailure = failArm ? "injected Single start failure" : QString{};
        return armed;
    }
    ScopeAcquisitionState acquisitionState() override {
        require(armed, "strategy polled a stopped scope without starting a fresh acquisition");
        lastFailure = invalidState ? "injected acquisition state failure" : QString{};
        if (invalidState) return ScopeAcquisitionState::Invalid;
        const bool completed = mode == ScopeTriggerMode::Auto || ((!transient || event) && level >= -3 && level <= 4);
        if (completed) completedRecords.insert(records);
        return completed ? ScopeAcquisitionState::Completed : ScopeAcquisitionState::Running;
    }
    void requireArchived() const {
        require(!checkArchives || completedRecords == archivedRecords, "new settings/acquisition overwrote an unarchived record");
    }
    void archive() {
        require(completedRecords.contains(records) && !archivedRecords.contains(records), "stale or duplicate Capture");
        archivedRecords.insert(records);
    }
    ScopeTriggerState triggerState() override {
        require(armed, "strategy waited for trigger readiness while still stopped");
        return reportedTrigger;
    }
    double readMeasurement(int channel, ScopeMeasurement metric) override {
        require(!transient || (event && mode != ScopeTriggerMode::Auto) ||
                    (referenceQuartet && mode == ScopeTriggerMode::Auto) ||
                    (metric != ScopeMeasurement::Rms && metric != ScopeMeasurement::Mean),
                "transient reference/Baseline queried RMS/Mean");
        require(channel == 2, "read disabled channel");
        require(acquisitionState() == ScopeAcquisitionState::Completed && !clipped, "read invalid record");
        ++reads;
        switch (metric) {
        case ScopeMeasurement::Maximum: return mode == ScopeTriggerMode::Auto ? 2 : 4;
        case ScopeMeasurement::Minimum: return mode == ScopeTriggerMode::Auto ? -1 : -3;
        case ScopeMeasurement::Rms: return 1.5;
        case ScopeMeasurement::Mean: return .5;
        }
        return NAN;
    }
    ScopeTriggerSlope slope = ScopeTriggerSlope::Rise;
    ScopeTriggerMode mode = ScopeTriggerMode::Normal;
    ScopeTriggerState reportedTrigger = ScopeTriggerState::Ready;
    double scale = 1, level = 0;
    bool transient = false, armed = false, event = false, clipped = false;
    bool referenceQuartet = false;
    bool invalidState = false, rejectSlope = false, failStop = false, failArm = false;
    QString lastFailure;
    int records = 0, stops = 0, modes = 0, reads = 0;
    QAtomicInt *cancelOnConfigure = nullptr;
    bool checkArchives = false;
    QSet<int> completedRecords, archivedRecords;
};

class ContractAlgorithm : public IOscilloscopeMeasureStrategy {
public:
    ContractAlgorithm(IScopeMeasurement &scope,QAtomicInt &stop,bool &destroyed)
        : expectedScope(scope),expectedStop(stop),destroyed(destroyed) {}
    ~ContractAlgorithm() override {destroyed=true;}
    QString name() const override {return "contract";}
    OscMeasureResult execute(IScopeMeasurement *scope,QAtomicInt &stop) override {
        require(scope==&expectedScope&&&stop==&expectedStop,"capability or cancellation flag changed");
        require(stop.loadAcquire()==1,"stop state lost");
        OscMeasureResult result;
        result.success=false;result.cleanupFailed=true;result.errorMessage="cleanup detail";
        result.channels={{4,3.141592653589793,-2.718281828459045,1.4142135623730951,0.},
                         {2,NAN,-0.125,0.,NAN}};
        if(m_captureObserver)m_captureObserver({result.channels});
        return result;
    }
    IScopeMeasurement &expectedScope;QAtomicInt &expectedStop;bool &destroyed;
};

class DiagnosticScope : public SemanticScope {
public:
    QString fault;
    int failedRecord=0;
    ScopeAcquisitionState acquisitionState() override {
        const auto state=SemanticScope::acquisitionState();
        if(fault=="late" && mode==ScopeTriggerMode::Normal && state==ScopeAcquisitionState::Completed && !failedRecord) {
            failedRecord=records;QThread::msleep(40);
        }
        return state;
    }
    bool isClipping(int channel) override {
        if(fault=="clipping" && mode==ScopeTriggerMode::Normal) {
            failedRecord=records;throw std::runtime_error("clipping read failed");
        }
        return SemanticScope::isClipping(channel);
    }
    double readMeasurement(int channel,ScopeMeasurement metric) override {
        if(fault=="measurement" && mode==ScopeTriggerMode::Normal) {
            failedRecord=records;throw std::runtime_error("measurement read failed");
        }
        return SemanticScope::readMeasurement(channel,metric);
    }
};

void phaseDeadlineContract()
{
    // Exercise real AUTO -> search/event/baseline callers, not only the shared helper.
    class StalledScope : public SemanticScope {
    public:
        bool baseline=false;
        int stalledReads=0;
        bool readVerticalRange(int,ScopeVerticalRange &range,QString &) override { range={scale,0,10,0,-8,8}; return true; }
        ScopeAcquisitionState acquisitionState() override {
            if (baseline && mode==ScopeTriggerMode::Normal) {
                completedRecords.insert(records); return ScopeAcquisitionState::Completed;
            }
            return SemanticScope::acquisitionState();
        }
        double readMeasurement(int channel,ScopeMeasurement metric) override {
            if (mode==ScopeTriggerMode::Normal) { ++stalledReads; return NAN; }
            return SemanticScope::readMeasurement(channel,metric);
        }
    };
    for (const QString name : {"Static Test","Dynamic Test","Turn on","Turn off","Turn on then short","Short then turn on"})
    for (bool baseline : {false,true}) {
        if (baseline && name!="Turn off" && name!="Turn on then short") continue;
        StalledScope scope; scope.baseline=baseline; scope.checkArchives=true;
        scope.transient=name!="Static Test" && name!="Dynamic Test";
        scope.referenceQuartet=name=="Turn off" || name=="Turn on then short";
        QAtomicInt stop(0);
        TransientContext context; context.timeoutMs=3; context.phaseTimeoutMs=1000; context.target="MAX";
        context.settleMs=context.powerOffMs=context.dischargeMs=0; context.trialsPerLevel=1;
        context.singleProcessingMs=0; // Timing has its own real-time regression.
        context.verticalScale.margin=true;
        bool powered=false, loaded=false, shorted=false;
        context.power=[&](bool on,QString &) { powered=on; if(scope.armed)scope.event=true; return true; };
        context.load=[&](bool on,QString &) { loaded=on; return true; };
        context.relays=[&](bool sh,bool,QString &) { shorted=sh; if(scope.armed&&sh)scope.event=true; return true; };
        QStringList phases;
        context.captureRecord=[&](const ScopeCaptureRecord &record) { scope.archive(); phases<<record.phase; };
        QElapsedTimer timer; timer.start();
        OscMeasureResult result;
        if (!scope.transient) {
            SteadyStateSearchStrategy strategy(3,1000,100,"MAX",context.verticalScale);
            strategy.setCaptureObserver(context.captureRecord);
            result=strategy.execute(&scope,stop);
        } else {
            const auto kind=name=="Turn on"?TransientKind::TurnOn:name=="Turn off"?TransientKind::TurnOff:
                name=="Turn on then short"?TransientKind::TurnOnThenShort:TransientKind::ShortThenTurnOn;
            result=runTransientStrategy(kind,context,&scope,stop);
        }
        require(!result.success && result.errorMessage.contains("timeout") && timer.elapsed()<2400 &&
                scope.stalledReads>0 && scope.stops>0 && !powered && !loaded && !shorted &&
                phases.contains(baseline?"Baseline":"Measurement"),
                qPrintable(name+" phase timeout/diagnostic/cleanup contract: "+result.errorMessage));
        scope.requireArchived();
    }
}

void commonContract() {
    SemanticScope scope;QAtomicInt stop{1};bool destroyed=false;int captures=0;
    auto factory=makeMeasurementStrategyFactory([&](const QString &name,const TransientContext *context){
        require(name=="test"&&context&&context->target=="MIN","typed settings lost");
        return std::make_unique<ContractAlgorithm>(scope,stop,destroyed);
    });
    TransientContext context;context.target="MIN";
    auto strategy=factory(ScopeMeasurementRequest{"test",scope,context,[&](const auto &record){
        ++captures;require(record.channels.size()==2&&record.channels[0].maxVal==3.141592653589793,"capture record changed");
    }});
    require(strategy&&strategy->name()=="contract"&&!destroyed&&captures==0,"factory executed algorithm");
    const auto result=strategy->execute(stop);
    require(!result.success&&result.cleanupFailed&&result.error=="cleanup detail"&&result.summary.isEmpty(),"failure mapping changed");
    require(result.channels.size()==2&&captures==1,"channels or capture lost");
    const auto &first=result.channels[0];const auto &second=result.channels[1];
    require(first.channel==4&&first.unit.isEmpty()&&first.maximum==3.141592653589793&&
        first.minimum==-2.718281828459045&&first.rms==1.4142135623730951&&first.mean==0.,"measurement precision/order changed");
    require(second.channel==2&&std::isnan(second.maximum)&&second.minimum==-0.125&&second.rms==0.&&std::isnan(second.mean),"NA/zero changed");
    strategy.reset();require(destroyed,"algorithm ownership lost");
    require(!makeMeasurementStrategyFactory({})(ScopeMeasurementRequest{"test",scope,{}, {}}),"missing algorithm factory accepted");
    auto absent=makeMeasurementStrategyFactory([](const auto&,const auto*)->std::unique_ptr<IOscilloscopeMeasureStrategy>{return {};});
    require(!absent(ScopeMeasurementRequest{"test",scope,{}, {}}),"null algorithm wrapped");
    require(scope.records==0&&scope.reads==0&&scope.stops==0,"adapter added hardware operations");
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        commonContract();
        phaseDeadlineContract();
        auto factory=makeMeasurementStrategyFactory([](const QString &name,const TransientContext *context)
            ->std::unique_ptr<IOscilloscopeMeasureStrategy>{
            if(name=="Static Test"||name=="Dynamic Test")
                return std::make_unique<SteadyStateSearchStrategy>(3,10000,100,context->target);
            return std::unique_ptr<IOscilloscopeMeasureStrategy>(OscilloscopeStrategyFactory::create(name,context));
        });
        QAtomicInt stop{0};
        for (QString name : {"Static Test", "Dynamic Test"})
        for (QString target : {"BOTH", "MAX", "MIN"}) {
            SemanticScope scope;scope.checkArchives=true;
            TransientContext context;context.target=target;
            int captures = 0;
            auto strategy=factory(ScopeMeasurementRequest{name,scope,context,[&](const ScopeCaptureRecord &record) {
                scope.archive();
                require(record.channels.size() == 1 && record.channels[0].channel == 2, "capture channel mismatch"); ++captures;
            }});
            auto result = strategy->execute(stop);
            scope.requireArchived();
            require(result.success && captures > 0 && scope.stops > 0, qPrintable(result.error));
            const auto &value = result.channels[0];
            require(target == "MIN" ? std::isnan(value.maximum) : value.maximum == 4, "maximum changed");
            require(target == "MAX" ? std::isnan(value.minimum) : value.minimum == -3, "minimum changed");
            require(value.rms == 1.5 && value.mean == .5, "record metrics changed");
        }
        for (QString name : {"Turn on", "Turn off", "Turn on then short", "Short then turn on"}) {
            SemanticScope scope; scope.transient = true;scope.checkArchives=true;
            scope.referenceQuartet = name == "Turn off" || name == "Turn on then short";
            TransientContext context;
            context.timeoutMs = 3; context.phaseTimeoutMs = 10000;
            context.settleMs = context.powerOffMs = context.dischargeMs = 0;
            context.singleProcessingMs = 0;
            context.trialsPerLevel = 1;
            context.power = [&](bool, QString &) { if (scope.armed) scope.event = true; return true; };
            context.load = [](bool, QString &) { return true; };
            context.relays = [&](bool shorted, bool, QString &) {
                if (scope.armed && shorted) scope.event = true; return true;
            };
            int archived=0;
            auto strategy=factory(ScopeMeasurementRequest{name,scope,context,[&](const ScopeCaptureRecord &record){
                scope.archive();
                require(!record.channels.isEmpty(),"transient capture empty");++archived;
                for (const auto &value : record.channels)
                    require(std::isnan(value.rms) && std::isnan(value.mean), "transient capture retained RMS/Mean");
            }});
            require(strategy&&strategy->name()==name,"wrong transient strategy selected");
            auto result = strategy->execute(stop);
            scope.requireArchived();
            require(result.success && scope.stops > 0, qPrintable(result.error));
            require(archived>0,"transient strategy did not archive its events");
            require(result.channels[0].maximum == 4 && result.channels[0].minimum == -3, "transient extrema changed");
            require(std::isnan(result.channels[0].rms) && std::isnan(result.channels[0].mean), "transient result retained RMS/Mean");
        }
        for (QString name : {"Static Test", "Dynamic Test", "Turn on", "Turn off", "Turn on then short", "Short then turn on"})
        for(QString fault : {"late","clipping","measurement"}) {
            DiagnosticScope scope;scope.fault=fault;scope.checkArchives=true;
            scope.transient=name!="Static Test"&&name!="Dynamic Test";
            scope.referenceQuartet = name == "Turn off" || name == "Turn on then short";
            TransientContext context;context.timeoutMs=3;context.phaseTimeoutMs=10000;context.trialsPerLevel=1;
            context.settleMs=context.powerOffMs=context.dischargeMs=0;
            context.singleProcessingMs=0;
            context.power=[&](bool,QString&){if(scope.armed)scope.event=true;return true;};
            context.relays=[&](bool sh,bool,QString&){if(scope.armed&&sh)scope.event=true;return true;};
            context.load=[](bool,QString&){return true;};
            auto strategy=factory(ScopeMeasurementRequest{name,scope,context,[&](const ScopeCaptureRecord &record){
                require(record.channels.size()==1&&record.channels[0].channel==2,"diagnostic channel lost");scope.archive();
            }});
            const auto result=strategy->execute(stop);
            require(!result.success&&scope.stops>0&&scope.failedRecord>0&&scope.archivedRecords.contains(scope.failedRecord),
                    qPrintable(name+" "+fault+": failed record lost or task falsely passed: "+result.error));
            require(result.error.contains(fault=="late"?"timeout":"read failed"),"original diagnostic failure lost");
            scope.requireArchived();
        }
        for (QString name : {"Static Test", "Dynamic Test", "Turn on", "Turn off", "Turn on then short", "Short then turn on"}) {
            SemanticScope failed; failed.failArm = true;
            TransientContext context; context.timeoutMs = 3;
            context.powerOffMs = context.dischargeMs = context.settleMs = 0;
            context.power = context.load = [](bool, QString&) { return true; };
            context.relays = [](bool, bool, QString&) { return true; };
            int captured = 0;
            auto strategy = factory(ScopeMeasurementRequest{name, failed, context, [&](const auto&) { ++captured; }});
            const auto result = strategy->execute(stop);
            require(!result.success && failed.reads == 0 && captured == 0 && failed.stops > 0,
                    "failed Single start consumed a stale record or skipped cleanup");
        }
        SemanticScope rejected; rejected.rejectSlope = true;
        require(!factory(ScopeMeasurementRequest{"Static Test",rejected,{},{}})->execute(stop).success && rejected.stops > 0,
                "failed semantic operation accepted or cleanup skipped");
        SemanticScope invalid; invalid.invalidState = true;
        require(!factory(ScopeMeasurementRequest{"Static Test",invalid,{},{}})->execute(stop).success && invalid.reads == 0,
                "invalid acquisition exposed measurements");
        SemanticScope cancelled; cancelled.cancelOnConfigure = &stop;
        require(!factory(ScopeMeasurementRequest{"Static Test",cancelled,{},{}})->execute(stop).success && cancelled.records == 0
                    && cancelled.stops > 0 && cancelled.modes == 1,
                "cancelled configuration continued or NORMAL cleanup skipped");
        stop.storeRelease(0);
        using OscilloscopeAcquisition::AcquisitionResult;
        for (auto state : {ScopeTriggerState::Ready, ScopeTriggerState::Arming,
                           ScopeTriggerState::Triggered, ScopeTriggerState::Invalid}) {
            SemanticScope waiting; waiting.level = 10; waiting.reportedTrigger = state;
            waiting.beginSingleAcquisition();
            const auto expected = state == ScopeTriggerState::Ready ? AcquisitionResult::NoTrigger
                : state == ScopeTriggerState::Invalid ? AcquisitionResult::CommError : AcquisitionResult::CaptureTimeout;
            require(OscilloscopeAcquisition::waitForCapture(&waiting, stop, 3) == expected,
                    "typed trigger state confused incomplete capture with MISS");
            require(waiting.reads == 0, "waiting read stale data");
        }
        SemanticScope cleanup; cleanup.failStop = true;
        require(!OscilloscopeAcquisition::captureAutoReference(&cleanup, stop, 1000).success
                    && cleanup.mode == ScopeTriggerMode::Normal,
                "failed stop skipped remaining cleanup");
        SemanticScope clipped; clipped.clipped = true;
        require(OscilloscopeAcquisition::captureAutoReference(&clipped, stop, 1000).success
                    && clipped.scale == 2 && clipped.records == 2,
                "clipping recovery did not reacquire through semantic API");
        std::cout << "PASS strategy capability contract\n";
    } catch (const std::exception &error) { std::cerr << error.what(); return 1; }
}
