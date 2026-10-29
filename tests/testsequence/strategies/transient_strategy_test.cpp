#include "../../support/transient_test_support.h"
void turnOffInitialDiagnostics() {
    Scope scope;
    auto cfg=context(scope);
    cfg.target="MIN";cfg.phaseTimeoutMs=40;cfg.settleMs=80;cfg.trialsPerLevel=1;
    QStringList logs;
    cfg.log=[&](const QString& line){logs<<line;};
    QAtomicInt stop(0);
    const auto result=runTransientStrategy(TransientKind::TurnOff,cfg,&scope,stop);
    require(!result.success && result.errorMessage=="Transient phase time limit reached", "initial timeout category changed");
    require(scope.arms==0 && scope.trace.last()=="LOAD OFF", "initial timeout acquired or skipped cleanup");
    require(logs.join('\n').contains("Settings: direction=MIN; trigger timeout=1 ms; phase limit=40 ms"), "Turn off settings diagnostic missing");
    require(logs.join('\n').contains("power-off wait=0 ms; settle=80 ms; trials per level=1; acquisition limit=100; vertical margin=OFF; Capture=OFF"), "effective settings snapshot missing");
    require(logs.join('\n').contains("Trigger source=CH1; enabled channels=CH1,CH2"), "channel snapshot missing");
    require(logs.join('\n').contains("stage=Initial Settle; begin; requested=80 ms"), "initial Settle begin diagnostic missing");
    const auto failure=logs.filter("failed: Transient phase time limit reached");
    require(failure.size()==1 && failure[0].contains("stage=Initial Settle") && failure[0].contains("Phase=Fall")
            && failure[0].contains("remaining=0 ms"), "timeout location/time missing");
}
void turnOffReferenceDiagnostics() {
    Scope scope;
    auto cfg=context(scope); cfg.target="MIN"; cfg.trialsPerLevel=1;
    QStringList logs;
    cfg.log=[&](const QString& line){logs<<line;};
    scope.onRead=[&]{if(scope.autoMode) throw std::runtime_error("reference read failure");};
    QAtomicInt stop(0);
    const auto result=runTransientStrategy(TransientKind::TurnOff,cfg,&scope,stop);
    require(!result.success && result.errorMessage.contains("reference read failure"),"reference error changed");
    require(logs.join('\n').contains("AUTO reference: CH1 Maximum read begin; Phase=Fall"),"AUTO metric begin/time missing");
    require(logs.filter("AUTO reference: CH1 Maximum read complete").isEmpty(),"failed read falsely completed");
    require(logs.join('\n').contains("stage=AUTO reference; failed: reference read failure"),"AUTO failure stage missing");
    require(scope.trace.last()=="LOAD OFF", "reference failure cleanup missing");
}
void turnOffDiagnosticBoundaries() {
    for (const QString scenario : {"stop_settle","power_failure","capture_failure","reference_timeout","both","cleanup_failure","diagnostic_throw"}) {
        class DiagnosticScope : public Scope {
        public:
            bool pending=false;
            ScopeReferenceStatus pollAutoReference(QString& state,QString& error) override {
                if(pending){state="mode=AUTO acquisition=RUN count=0";return ScopeReferenceStatus::Pending;}
                return Scope::pollAutoReference(state,error);
            }
        } scope;
        auto cfg=context(scope);cfg.target=scenario=="both"?"BOTH":"MIN";cfg.trialsPerLevel=1;
        QStringList logs;
        QAtomicInt stop(0);
        cfg.log=[&](const QString& line){
            logs<<line;
            if(scenario=="stop_settle" && line.contains("stage=Initial Settle; begin")) stop.storeRelease(1);
            if(scenario=="diagnostic_throw" && line.startsWith("Diagnostic:")) throw std::runtime_error("diagnostic sink failure");
        };
        if(scenario=="power_failure") cfg.power=[](bool on,QString& error){error="input rejected";return !on;};
        if(scenario=="capture_failure") cfg.captureRecord=[](const ScopeCaptureRecord&){throw std::runtime_error("capture rejected");};
        if(scenario=="reference_timeout"){scope.pending=true;cfg.phaseTimeoutMs=80;}
        if(scenario=="cleanup_failure") cfg.load=[](bool on,QString& error){error="load cleanup rejected";return on;};
        const auto result=runTransientStrategy(TransientKind::TurnOff,cfg,&scope,stop);
        const QString text=logs.join('\n');
        if(scenario=="diagnostic_throw") {
            require(result.success && scope.trace.last()=="LOAD OFF", "diagnostic callback interrupted operation/cleanup");
        } else if(scenario=="both") {
            require(result.success && !result.trials.isEmpty(),"event result changed");
            require(logs.filter("stage=Phase initialization; begin").size()==2 && text.contains("Phase=Rise") && text.contains("Phase=Fall"),"directions not distinguished");
            require(logs.filter("phase complete;").size()==2,"phase completion missing");
            require(text.contains("AUTO reference: CH1 Mean read complete; Phase=Fall"),"quartet completion missing");
        } else {
            require(!result.success && result.trials.isEmpty() == (scenario!="cleanup_failure"),"failure became fallback success");
            if(scenario=="stop_settle") {
                require(result.errorMessage=="Stopped by user" && text.contains("stage=Initial Settle; failed: Stopped by user"),"Stop stage missing");
                require(!text.contains("stage=Initial Settle; complete") && scope.arms==0,"cancelled Settle completed/acquired");
            }
            if(scenario=="power_failure")
                require(result.errorMessage=="input rejected" && text.contains("stage=Input ON; failed: input rejected") && !text.contains("stage=Input ON; complete"),"Input failure boundary lost");
            if(scenario=="capture_failure")
                require(result.errorMessage.contains("capture rejected") && text.contains("AUTO reference: failed during record checks / Capture") && !text.contains("record checks / Capture complete"),"Capture failure boundary lost");
            if(scenario=="reference_timeout")
                require(result.errorMessage.contains("AUTO reference timeout") && text.contains("failed during acquisition") && text.contains("remaining=0 ms"),"reference timeout location missing");
            if(scenario=="cleanup_failure")
                require(result.cleanupFailed && text.contains("stage=Cleanup Load OFF; failed: load cleanup rejected"),"cleanup failure lost");
        }
        require(text.contains("cleanup finished; result="),"final cleanup status missing");
        const QRegularExpression time("Phase=([^;]+); elapsed=(\\d+) ms; remaining=(\\d+) ms; limit=(\\d+) ms");
        QString direction; qint64 previous=0; int samples=0;
        for(const auto& line:logs) {
            const auto match=time.match(line);if(!match.hasMatch())continue;
            const auto elapsed=match.captured(2).toLongLong(), remaining=match.captured(3).toLongLong(), limit=match.captured(4).toLongLong();
            require(remaining==qMax<qint64>(0,limit-elapsed),"invalid phase accounting");
            if(direction==match.captured(1)) require(elapsed>=previous,"phase clock restarted within direction");
            direction=match.captured(1);previous=elapsed;++samples;
        }
        require(samples>5,"phase progress absent");
    }
    // Other strategies retain their existing log surface and result behavior.
    for(auto kind:{TransientKind::TurnOn,TransientKind::ShortThenTurnOn,TransientKind::TurnOnThenShort}) {
        Scope scope;scope.single=kind!=TransientKind::TurnOnThenShort;
        auto cfg=context(scope);cfg.trialsPerLevel=1;QStringList logs;cfg.log=[&](const QString& line){logs<<line;};
        QAtomicInt stop(0);require(runTransientStrategy(kind,cfg,&scope,stop).success,"other strategy changed");
        require(logs.filter("Diagnostic:").isEmpty() && logs.filter("Maximum read begin").isEmpty(),"Turn off diagnostics leaked to other strategy");
    }
}
void turnOffNegativeFallbackUnchanged() {
    class NegativeScope : public Scope {
    public:
        NegativeScope(){scale=60;totalChannels=1;}
        QString getAcquisitionState() override {
            if(!armed || autoMode)return "0";
            if(event)return "1";
            return level>=-6.037500001?"0":"1";
        }
        double measureSignalPeak(int,const QString& metric) override {
            if(metric=="MAXimum")return 476.76563;
            if(metric=="MINimum")return autoMode?-5.8875:-6.1;
            return metric=="RMS"?275.44099:238.91049;
        }
    } scope;
    auto cfg=context(scope);cfg.target="MIN";cfg.trialsPerLevel=1;
    QStringList logs;cfg.log=[&](const QString& line){logs<<line;};
    QAtomicInt stop(0);
    const auto result=runTransientStrategy(TransientKind::TurnOff,cfg,&scope,stop);
    const double levels[]={2.4,-8.2875,-7.0875,-6.4875,-6.1875,-6.0375};
    require(scope.levels.size()==6,"fallback search level count changed");
    for(int i=0;i<6;++i)require(std::abs(scope.levels[i]-levels[i])<1e-8,"Fall thresholds changed");
    require(result.success && result.trials.isEmpty() && result.steadyReferences.size()==1,"fallback provenance changed");
    const auto& values=result.steadyReferences[0].channels[0];
    require(values.maximum==476.76563 && values.minimum==-5.8875 && values.rms==275.44099 && values.mean==238.91049,"fallback quartet changed");
    require(logs.count("Power off; wait for event")==4 && logs.count("Baseline trigger excluded from event results")==1,"event/baseline count changed");
}
void turnOffTrialStopFailure() {
    Scope scope;scope.never=true;
    auto cfg=context(scope);cfg.target="MIN";cfg.trialsPerLevel=1;
    QStringList logs;cfg.log=[&](const QString& line){logs<<line;};
    bool injected=false;
    scope.onWrite=[&](const QString& command){
        if(command=="ACQuire:STATE STOP" && !scope.autoMode && scope.event && !injected) {
            injected=true;throw std::runtime_error("trial STOP rejected");
        }
    };
    QAtomicInt stop(0);
    const auto result=runTransientStrategy(TransientKind::TurnOff,cfg,&scope,stop);
    require(!result.success && result.errorMessage=="trial STOP rejected" && scope.trace.last()=="LOAD OFF", "STOP error/cleanup changed");
    require(logs.join('\n').contains("stage=Trial STOP; failed: trial STOP rejected"),"STOP failure mislabeled as previous completed step");
    require(logs.filter("stage=Trial STOP; complete").isEmpty(),"failed STOP reported complete");
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{
    turnOffInitialDiagnostics();
    turnOffReferenceDiagnostics();
    turnOffDiagnosticBoundaries();
    turnOffNegativeFallbackUnchanged();
    turnOffTrialStopFailure();
    QAtomicInt stop(0);
    Scope rejected;rejected.failWrite="TRIGger:A:TYPe EDGE";auto rc=context(rejected);
    require(!runTransientStrategy(TransientKind::TurnOn,rc,&rejected,stop).success&&rejected.arms==0,"SCPI rejection ignored");
    for(const auto& name:{"Turn on","Turn off","Turn on then short","Short then turn on"}){
        std::unique_ptr<IOscilloscopeMeasureStrategy>s(OscilloscopeStrategyFactory::create(name,&rc));
        require(bool(s)&&OscilloscopeStrategyFactory::create(name)==nullptr,"factory gap");
    }
    std::cout<<"binary transient strategies passed\n";
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
