#include "../../support/transient_test_support.h"
#include "oscilloscopeacquisition.h"
#include "steadystatesearchstrategy.h"
#include <QThread>

void transientGridContract(TransientKind kind)
{
    for (double scale : {4.0, 9.999}) {
        Scope scope;scope.totalChannels=1;scope.scale=scale;
        scope.single=kind==TransientKind::TurnOn || kind==TransientKind::ShortThenTurnOn;
        scope.steady=17.7563;scope.peak=21.555;scope.negativePeak=-21.555;
        auto cfg=context(scope);cfg.trialsPerLevel=1;
        QAtomicInt stop(0);
        const auto result=runTransientStrategy(kind,cfg,&scope,stop);
        require(result.success,"quantized transient execution failed");
        const double step=scale==4 ? .16 : .39996;
        for (double level : scope.levels) {
            if (std::abs(level/step-std::round(level/step))>1e-8)
                std::cerr<<"Transient kind="<<int(kind)<<" Level="<<level<<" Step="<<step<<'\n';
            require(std::abs(level/step-std::round(level/step))<1e-8,
                    "transient sent a fractional trigger grid point");
        }
        require(!result.trials.isEmpty() && result.channels[0].maxVal==21.555 &&
                result.channels[0].minVal==-21.555,"grid changed measured transient extrema");
    }
}
void transientCase(const QString& scenario, TransientKind kind) {
    if (scenario=="bipolar") transientGridContract(kind);
    Scope s;
    s.forbidRmsMean = true;
    s.allowReferenceQuartet = kind == TransientKind::TurnOff || kind == TransientKind::TurnOnThenShort;
    s.allowBaselineQuartet = kind == TransientKind::TurnOff;
    s.single=kind==TransientKind::TurnOn || kind==TransientKind::ShortThenTurnOn;
    auto c=context(s);
    c.trialsPerLevel=1;
    QAtomicInt stop(0);
    bool expectSuccess=false, expectCleanup=true, expectBoth=false;
    QString expectedError;
    QStringList logs;
    c.log=[&](const QString& line){logs<<line;};
    if(scenario.startsWith("scale_")) {
        s.totalChannels=1;s.clipped=true;s.clipEventOnly=true;s.scale=1;
        expectSuccess=scenario=="scale_recover";
        if(scenario=="scale_limit"){s.requiredScale=1e9;s.peak=1e12;s.negativePeak=-1e12;}
        if(scenario=="scale_clamp")s.clampScale=true;
        if(scenario=="scale_reject")s.rejectScale=true;
        if(!expectSuccess)expectedError=scenario=="scale_reject"?"scale rejected":"scale";
    } else if(scenario=="bipolar" || scenario=="positive" || scenario=="negative") {
        s.steady=10;s.peak=scenario=="negative"?10:100;s.negativePeak=scenario=="positive"?0:-100;
        s.missMax=scenario=="negative";s.missMin=scenario=="positive";
        expectSuccess=true;expectBoth=true;
    } else if(scenario=="no_event") {
        s.never=true;s.steady=100;expectBoth=true;
        expectSuccess=!s.single;
        if(!expectSuccess)expectedError="NA";
    } else if(scenario=="intermittent") {
        s.intermittent=true;c.trialsPerLevel=3;expectSuccess=true;expectBoth=true;
    } else if(scenario=="secondary_na") {
        s.invalidSecondary=true;expectSuccess=true;expectBoth=true;
    } else if(scenario=="dpo_command") {
        s.modelName="DPO7000";expectSuccess=true;expectBoth=true;
    } else if(scenario=="clipping") {
        s.clipped=true;s.clipEventOnly=true;expectedError="scale";
    } else if(scenario=="nan_event") {
        s.onRead=[&]{if(s.event)s.invalid=true;};expectedError="measurement";
    } else if(scenario=="bad_source") {
        s.source="MATH1";expectedError="Trigger source";expectCleanup=false;
    } else if(scenario=="disabled_source") {
        s.enabled=false;expectedError="Trigger source";expectCleanup=false;
    } else if(scenario=="arm_failure") {
        s.failArm=true;expectedError="start";
    } else if(scenario=="comm_error") {
        s.commError=true;expectedError="communication";
    } else if(scenario=="write_failure") {
        s.failWrite="TRIGger:A:TYPe EDGE";expectedError="rejected";
    } else if(scenario=="power_failure") {
        c.power=[&](bool,QString& e){s.trace<<"POWER FAILURE";e="power failed";return false;};expectedError="power failed";
    } else if(scenario=="relay_failure") {
        c.relays=[&](bool,bool,QString& e){s.trace<<"RELAY FAILURE";e="relay failed";return false;};expectedError="relay failed";
    } else if(scenario=="load_failure") {
        c.load=[&](bool on,QString& e){s.trace<<(on?"LOAD FAIL":"LOAD OFF");e="load failed";return !on;};expectedError="load failed";
    } else if(scenario=="unknown_exception") {
        c.load=[&](bool on,QString&){s.trace<<(on?"LOAD FAIL":"LOAD OFF");if(on)throw 42;return true;};expectedError="Unexpected";
    } else if(scenario=="cleanup_failure") {
        c.load=[&](bool on,QString& e){s.trace<<(on?"LOAD ON":"LOAD OFF");e="cleanup load rejected";return on;};expectedError="Load OFF";
    } else if(scenario=="cleanup_exception") {
        c.load=[&](bool on,QString&){s.trace<<(on?"LOAD ON":"LOAD OFF");if(!on)throw std::runtime_error("cleanup throw");return true;};expectedError="cleanup throw";
    } else if(scenario=="cleanup_unknown") {
        c.load=[&](bool on,QString&){s.trace<<(on?"LOAD ON":"LOAD OFF");if(!on)throw 42;return true;};expectedError="unexpected cleanup";
    } else if(scenario=="cancel_before") {
        stop.storeRelease(1);expectedError="Stopped";expectCleanup=false;
    } else if(scenario=="cancel_wait") {
        s.onState=[&]{stop.storeRelease(1);};expectedError="Stopped";
    } else if(scenario=="cancel_read") {
        s.onRead=[&]{stop.storeRelease(1);};expectedError="Stopped";
    } else if(scenario=="iteration_limit") {
        c.maxIterations=1;expectedError="iteration limit";
    } else if(scenario=="phase_limit") {
        c.settleMs=30;c.powerOffMs=30;c.phaseTimeoutMs=2;expectedError="time limit";
    } else if(scenario=="invalid_config") {
        c.timeoutMs=0;expectedError="Invalid";expectCleanup=false;
    } else if(scenario=="null_scope") {
        auto r=runTransientStrategy(kind,c,nullptr,stop);
        require(!r.success&&r.errorMessage.contains("No oscilloscope"),"null scope");return;
    } else throw std::runtime_error("unknown transient case");
    const QString task=QStringList{"Turn on","Turn off","Turn on then short","Short then turn on"}.at(static_cast<int>(kind));
    std::unique_ptr<IOscilloscopeMeasureStrategy> strategy(OscilloscopeStrategyFactory::create(task,&c));
    require(strategy && strategy->name()==task,"factory strategy mapping");
    auto r=strategy->execute(&s,stop);
    if(r.success!=expectSuccess) std::cerr<<r.errorMessage.toStdString()<<"\n";
    require(r.success==expectSuccess,"unexpected success/failure");
    if(!expectedError.isEmpty())require(r.errorMessage.contains(expectedError,Qt::CaseInsensitive),"wrong error category");
    if(expectCleanup)require(s.trace.last()=="LOAD OFF","cleanup must reach load OFF");
    if(expectBoth) {
        require(s.trace.contains("TRIGger:A:EDGE:SLOpe RISE")&&s.trace.contains("TRIGger:A:EDGE:SLOpe FALL"),"opposite polarity skipped");
        require(!s.trace.contains("TRIGger:A:EDGE:SLOpe EITHER"),"combined edge used");
    }
    if(scenario=="bipolar" || scenario=="positive" || scenario=="negative") {
        require(r.channels[0].maxVal==s.peak && r.channels[0].minVal==s.negativePeak,"measured extrema lost");
        require(r.errorMessage.isEmpty(),"missing edge remained a failure");
        require(std::isnan(r.channels[0].rms) && std::isnan(r.channels[0].mean),"transient retained RMS/Mean");
        if(scenario=="positive") require(!logs.filter("No falling event").isEmpty(),"missing falling-edge notice");
        if(scenario=="negative") require(!logs.filter("No rising event").isEmpty(),"missing rising-edge notice");
        if(kind==TransientKind::TurnOff)require(std::abs(s.levels[1]-10.4)<1e-9,"AUTO seed incorrect");
    }
    if(scenario=="scale_recover")require(s.scale>=4&&s.arms>2&&r.channels[0].maxVal==7,"rescaled event not reacquired");
    if(scenario.startsWith("scale_")&&scenario!="scale_recover")require(s.clippedEventReads==0,"clipped record measured");
    if(scenario=="no_event") {
        require(logs.filter("no event trigger bracket").size()==2,"both NA phases not recorded");
        if(s.single) {
            require(std::isnan(r.channels[0].maxVal)&&std::isnan(r.channels[0].minVal),"reference or stale event leaked");
        } else {
            for(const auto& ch:r.channels) {
                require(ch.maxVal==100 && ch.minVal==100,"steady fallback lost");
                require(std::isnan(ch.rms)&&std::isnan(ch.mean),"steady RMS/Mean leaked into event result");
            }
            require(!logs.filter("Steady fallback").isEmpty(),"fallback provenance missing");
        }
    }
    if(scenario=="secondary_na")require(std::isnan(r.channels[1].rms),"secondary NA fabricated");
    if(scenario=="cancel_before")require(s.arms==0,"cancelled run acquired");
}

class ReferenceScope : public Scope {
public:
    QString response="0";
    int responseDelay=0, queries=0;
    bool noChannels=false,invalidRange=false,sentinel=false,unknownRead=false,throwCleanup=false;
    QString getAcquisitionState()override{++queries;if(onState)onState();if(responseDelay)QThread::msleep(responseDelay);return response;}
    bool isChannelEnabled(int ch)override{return !noChannels && ch==1;}
    double measureSignalPeak(int ch,const QString& type)override{
        if(unknownRead)throw 42;
        if(sentinel)return 9.9e37;
        if(invalidRange)return type=="MAXimum"?-1:1;
        return Scope::measureSignalPeak(ch,type);
    }
    bool writeConfigurationCommand(const QString& cmd,QString& e)override{
        if(throwCleanup&&cmd=="ACQuire:STATE STOP")throw 42;
        return Scope::writeConfigurationCommand(cmd,e);
    }
};
void acquisitionCase(const QString& scenario) {
    using namespace OscilloscopeAcquisition;
    ReferenceScope s;QAtomicInt stop(0);
    if(scenario.startsWith("state_")) {
        const QString token=scenario.mid(6);
        s.response=token=="header"?":ACQUIRE:STATE 0":token;
        const auto expected=QStringList{"0","STOP","OFF","header"}.contains(token)?AcquisitionResult::Completed
            :QStringList{"1","RUN","ON"}.contains(token)?AcquisitionResult::NoTrigger:AcquisitionResult::CommError;
        require(waitForCapture(&s,stop,3)==expected,"state decoding");return;
    }
    if(scenario=="late_completion") {s.responseDelay=10;require(waitForCapture(&s,stop,2)==AcquisitionResult::CompletedAfterTimeout,"late record lost or timeout ignored");return;}
    if(scenario=="late_running") {s.response="RUN";s.responseDelay=10;require(waitForCapture(&s,stop,2)==AcquisitionResult::NoTrigger,"late running");return;}
    if(scenario=="null_wait") {require(waitForCapture(nullptr,stop,2)==AcquisitionResult::CommError,"null wait");return;}
    if(scenario=="zero_wait") {require(waitForCapture(&s,stop,0)==AcquisitionResult::CaptureTimeout&&s.queries==0,"zero deadline must not count as a trigger MISS");return;}
    if(scenario=="cancel_wait") {stop.storeRelease(1);require(waitForCapture(&s,stop,2)==AcquisitionResult::Cancelled&&s.queries==0,"pre cancellation");return;}
    if(scenario=="cancel_query") {s.onState=[&]{stop.storeRelease(1);};require(waitForCapture(&s,stop,2)==AcquisitionResult::Cancelled,"query cancellation");return;}
    bool success=false;
    if(scenario=="reference_ok")success=true;
    else if(scenario=="reference_clipped")s.clipped=true;
    else if(scenario=="reference_empty")s.noChannels=true;
    else if(scenario=="reference_range")s.invalidRange=true;
    else if(scenario=="reference_sentinel")s.sentinel=true;
    else if(scenario=="reference_nan")s.invalid=true;
    else if(scenario=="reference_timeout")s.response="RUN";
    else if(scenario=="reference_comm")s.response="invalid";
    else if(scenario=="reference_arm")s.failArm=true;
    else if(scenario=="reference_write")s.failWrite="TRIGger:A:MODe AUTO";
    else if(scenario=="reference_cleanup")s.failWrite="TRIGger:A:MODe NORMAL";
    else if(scenario=="reference_throw_cleanup")s.throwCleanup=true;
    else if(scenario=="reference_throw")s.unknownRead=true;
    else if(scenario=="reference_cancel")s.onRead=[&]{stop.storeRelease(1);};
    else if(scenario=="reference_null") {require(!captureAutoReference(nullptr,stop,3).success,"null reference");return;}
    else if(scenario=="reference_zero") {require(!captureAutoReference(&s,stop,0).success,"zero reference");return;}
    else throw std::runtime_error("unknown acquisition case");
    auto r=captureAutoReference(&s,stop,3);
    require(r.success==success,"reference result");
    require(s.trace.contains("TRIGger:A:MODe NORMAL"),"reference did not restore NORMAL");
    if(success)require(r.channels.size()==1&&r.channels[0].maxVal==s.steady,"reference measurement");
}

class NormalScope : public Scope {
public:
    bool always=false,miss=false,nanNormal=false;
    bool isClipping(int ch)override{return !autoMode && Scope::isClipping(ch);}
    QString getAcquisitionState()override {
        if(onState)onState();
        if(commError)return "invalid";
        if(autoMode)return "0";
        return !miss && (always || (negative?level>=-2.4:level<=2.4))?"0":"1";
    }
    double measureSignalPeak(int ch,const QString& type)override {
        ++reads;if(onRead)onRead();
        if(nanNormal&&!autoMode)return std::numeric_limits<double>::quiet_NaN();
        return type=="MINimum"?-2:2;
    }
};
void normalCase(const QString& scenario) {
    NormalScope s;QAtomicInt stop(0);
    bool success=false;
    QString error;
    if(scenario=="scale_recover"){s.totalChannels=1;s.scale=1;s.clipped=true;success=true;}
    else if(scenario=="scale_clamp"){s.totalChannels=1;s.scale=1;s.clipped=true;s.clampScale=true;error="scale";}
    else if(scenario=="bipolar")success=true;
    else if(scenario=="flat") {s.miss=true;success=true;}
    else if(scenario=="null") {require(!SteadyStateSearchStrategy(2,1000,4).execute(nullptr,stop).success,"null normal");return;}
    else if(scenario=="cancel_before") {stop.storeRelease(1);error="Stopped";}
    else if(scenario=="bad_source") {s.source="MATH1";error="Trigger source";}
    else if(scenario=="disabled_source") {s.enabled=false;error="Trigger source";}
    else if(scenario=="write_edge") {s.failWrite="TRIGger:A:TYPe EDGE";error="rejected";}
    else if(scenario=="write_level") {s.failWrite="TRIGger:A:LEVel:CH1 2.4";error="rejected";}
    else if(scenario=="arm_normal") {s.onArm=[&]{if(!s.autoMode)s.failArm=true;};error="Failed to start";}
    else if(scenario=="comm_normal") {s.onState=[&]{if(!s.autoMode)s.commError=true;};error="communication";}
    else if(scenario=="nan_normal") {s.nanNormal=true;error="measurement";}
    else if(scenario=="cancel_normal_wait") {s.onState=[&]{if(!s.autoMode)stop.storeRelease(1);};error="Stopped";}
    else if(scenario=="cancel_normal_read") {s.onRead=[&]{if(!s.autoMode)stop.storeRelease(1);};error="Stopped";}
    else if(scenario=="cancel_after_level") {s.onWrite=[&](const QString& cmd){if(cmd.startsWith("TRIGger:A:LEVel"))stop.storeRelease(1);};error="Stopped";}
    else if(scenario=="finish_stop_failure") {s.onWrite=[&](const QString& cmd){if(s.arms>1&&cmd=="ACQuire:STATE STOP")s.failWrite=cmd;};error="Scope STOP";}
    else if(scenario=="iteration_limit") {s.always=true;error="limit";}
    else throw std::runtime_error("unknown normal scenario");
    auto r=SteadyStateSearchStrategy(2,1000,8).execute(&s,stop);
    require(r.success==success,"normal result");
    if(!error.isEmpty())require(r.errorMessage.contains(error,Qt::CaseInsensitive),"normal error category");
    if(success) {
        require(s.trace.contains("TRIGger:A:EDGE:SLOpe RISE")&&s.trace.contains("TRIGger:A:EDGE:SLOpe FALL"),"normal missed polarity");
        require(r.channels[0].maxVal==2&&r.channels[0].minVal==-2,"normal extrema");
    }
    if(scenario=="bipolar") {
        for(const auto& name:{"Static Test","Dynamic Test"}) {
            std::unique_ptr<IOscilloscopeMeasureStrategy> strategy(OscilloscopeStrategyFactory::create(name));
            require(strategy && strategy->name()=="NormalTrigger(BOTH)","Static/Dynamic mapping");
        }
        TransientContext c;
        require(OscilloscopeStrategyFactory::create("unsupported",&c)==nullptr,"unsupported factory");
    }
    if(scenario=="scale_recover") {
        require(s.scale==4 && s.levels.contains(2.08) && s.levels.contains(-2.08),
                "normal search did not refresh step after scale expansion");
    }
}

int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    try {
        require(argc>=3,"usage: matrix domain scenario [kind]");
        if(QString(argv[1])=="acquisition") acquisitionCase(argv[2]);
        else if(QString(argv[1])=="normal")normalCase(argv[2]);
        else {require(argc==4,"missing kind");transientCase(argv[2],static_cast<TransientKind>(QString(argv[3]).toInt()));}
        std::cout<<"PASS "<<argv[1]<<" / "<<argv[2]<<"\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
    return 0;
}
