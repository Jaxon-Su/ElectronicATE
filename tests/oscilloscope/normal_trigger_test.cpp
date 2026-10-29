#include "../support/measurement_scope_fixture.h"
#include "steadystatesearchstrategy.h"
#include "oscilloscopeacquisition.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <stdexcept>
#include <iostream>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class Scope : public MeasurementScopeFixture {
public:
    bool isClipping(int) override { return false; }
    QString model() const override { return "fake"; }
    QString vendor() const override { return "offline"; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return {}; }
    QString getTriggerSource() override { return "CH1"; }
    int getTotalChannel() override { return 1; }
    bool isChannelEnabled(int) override { return true; }
    bool writeConfigurationCommand(const QString& command, QString&) override {
        if (command=="TRIGger:A:MODe AUTO") autoMode=true;
        if (command=="TRIGger:A:MODe NORMAL") autoMode=false;
        if (command.endsWith("SLOpe RISE")) slope="RISING";
        if (command.endsWith("SLOpe FALL")) slope="FALLING";
        if (command.startsWith("TRIGger:A:LEVel ")) setTriggerLevel(command.section(' ', -1).toDouble());
        if (command=="ACQuire:STATE STOP") ++stops;
        return true;
    }
    double getTriggerLevel() override { return 0.5; }
    double getChannelScale(int channel) override { require(channel == 1, "wrong scale channel"); return scale; }
    void setTriggerLevel(double value) override { require(std::isfinite(value), "invalid trigger level"); level=value; if (slope == "FALLING") minLevels.append(value); }
    void setTriggerSlope(const QString& value) override { slope = value; }
    bool beginSingleAcquisition() override { ++starts; return !startFails; }
    QString getAcquisitionState() override {
        if (cancel) cancel->storeRelease(1);
        if (error) return "";
        if (autoMode && !never) return "0";
        if (never || (timeoutAfterCapture && starts % 2 == 0)) return "1";
        if (!alwaysCapture && ((slope == "FALLING" && level <= -3.0) || (slope == "RISING" && level >= 3.0))) return "1";
        return ":ACQUIRE:STATE 0";
    }
    double measureSignalPeak(int, const QString& type) override {
        ++reads;
        return invalid ? std::numeric_limits<double>::quiet_NaN()
                       : (type == "MINimum" ? -2.0 : 2.0);
    }
    bool waitForOperationComplete(int) override { throw std::runtime_error("OPC must not be used"); }
    void stop() override { ++stops; }
    QString slope;
    double level=0, scale=10;
    QVector<double> minLevels;
    bool autoMode=false,alwaysCapture=false;
    int starts=0, reads=0, stops=0;
    bool error=false, never=false, timeoutAfterCapture=false, invalid=false, startFails=false;
    QAtomicInt* cancel=nullptr;
};
class NegativeExcursionScope : public Scope {
public:
    QString getAcquisitionState() override { return autoMode || level > -100 ? "0" : "1"; }
    double measureSignalPeak(int ch,const QString& type) override {
        return !autoMode && type=="MINimum" ? -100 : Scope::measureSignalPeak(ch,type);
    }
};
class SecondaryScope : public Scope {
public:
    double bad = 9.91e37;
    bool triggerBad = false;
    int getTotalChannel() override { return 2; }
    double measureSignalPeak(int channel, const QString& type) override {
        if (!autoMode && (channel == 2 || triggerBad)) return bad;
        return Scope::measureSignalPeak(channel, type);
    }
};
void referenceFailureContract()
{
    using namespace OscilloscopeAcquisition;
    class Reference : public Scope {
    public:
        bool beginAutoReference(int &budget,QString &error) override {
            ++starts;budget=25;
            if(mode=="begin-error"){error="begin failure";return false;}
            return true;
        }
        ScopeReferenceStatus pollAutoReference(QString &state,QString &error) override {
            state="mode=AUTO count=0";
            if(mode=="error"){error="query failure";return ScopeReferenceStatus::Error;}
            if(mode=="throw")throw std::runtime_error("query threw");
            if(mode=="cancel")cancel->storeRelease(1);
            if(mode=="late"){QThread::msleep(35);return ScopeReferenceStatus::Completed;}
            return ScopeReferenceStatus::Pending;
        }
        QString mode;
    };
    for(const QString mode:{"begin-error","error","throw","cancel","timeout","phase","late"}) {
        Reference scope;scope.mode=mode;QAtomicInt stop{0};scope.cancel=&stop;
        QString error;QElapsedTimer clock;clock.start();
        const auto result=acquireAutoRecord(&scope,stop,mode=="phase"?5:100,error);
        const auto expected=mode=="late"?AcquisitionResult::CompletedAfterTimeout:
            mode=="cancel"?AcquisitionResult::Cancelled:
            mode=="timeout"||mode=="phase"?AcquisitionResult::CaptureTimeout:AcquisitionResult::CommError;
        require(result==expected && scope.starts==1 && scope.reads==0,"reference failure classification or stale read");
        require(mode=="late"||scope.stops>0,"failed reference left acquisition running");
        require(clock.elapsed()<100,"reference ignored the record/phase budget");
        if(mode=="error")require(error.contains("query failure")&&error.contains("count=0"),"reference error lost cause/state");
    }
    Reference stopped;QAtomicInt stop{1};QString error;
    require(acquireAutoRecord(&stopped,stop,100,error)==AcquisitionResult::Cancelled && stopped.starts==0,
            "pre-cancelled reference started acquisition");
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        referenceFailureContract();
        QAtomicInt stop(0);
        // Flat DC has no crossing at 2.4 V. Only a fresh AUTO RUNSTOP reference
        // may seed the Normal search; a Single-only implementation fails here.
        class FlatReference : public Scope {
        public:
            bool beginSingleAcquisition() override {
                require(!autoMode,"AUTO DC reference incorrectly started Single");
                ++starts;normalLevel=level;return true;
            }
            bool beginAutoReference(int &budget,QString &) override { budget=30;referenceStarted=true;return true; }
            ScopeReferenceStatus pollAutoReference(QString &state,QString &) override {
                state="mode=AUTO stop-after=RUNSTOP acquisition=RUN count=0";
                return stall?ScopeReferenceStatus::Pending:ScopeReferenceStatus::Completed;
            }
            QString getAcquisitionState() override {return "1";}
            double measureSignalPeak(int,const QString &) override {++reads;require(referenceStarted,"stale reference read");return 20;}
            bool stall=false,referenceStarted=false;double normalLevel=0;
        } flat;
        flat.scale=60;QStringList log;
        SteadyStateSearchStrategy dc(2,300,5,"MAX");
        dc.setLogObserver([&](const QString &text){log<<text;});
        const auto dcResult=dc.execute(&flat,stop);
        require(dcResult.success&&flat.normalLevel==20&&dcResult.channels[0].maxVal==20,
                "flat DC did not seed Normal with the AUTO maximum");
        require(log.join(' ').contains("wait limit=30")&&log.join(' ').contains("entering Normal")&&
                log.join(' ').contains("MISS"),"reference/search log boundaries missing");
        FlatReference stalled;stalled.stall=true;
        QElapsedTimer boundedReference;boundedReference.start();
        const auto stalledResult=dc.execute(&stalled,stop);
        require(!stalledResult.success&&stalled.reads==0&&stalled.starts==0&&
                boundedReference.elapsed()<200&&stalledResult.errorMessage.contains("count=0"),
                "missing AUTO record waited for the whole phase or read stale data");
        SteadyStateSearchStrategy strategy(2, 1000, 5);
        for (const QString target : {QString("MAX"), QString("MIN")}) {
            Scope single;
            auto r=SteadyStateSearchStrategy(2,1000,20,target).execute(&single,stop);
            require(r.success && single.stops>0, "single-side normal failed");
            require(std::isnan(target=="MAX"?r.channels[0].minVal:r.channels[0].maxVal), "normal unselected peak not NA");
            require(std::isfinite(target=="MAX"?r.channels[0].maxVal:r.channels[0].minVal), "normal selected peak lost");
            require(target=="MIN" || single.minLevels.isEmpty(), "MAX executed Fall");
            Scope failed; failed.error=true;
            require(!SteadyStateSearchStrategy(2,1000,20,target).execute(&failed,stop).success, "single-side communication error passed");
        }
        NegativeExcursionScope excursion;excursion.scale=1;
        auto negative=SteadyStateSearchStrategy(2,1000,5,"MIN").execute(&excursion,stop);
        require(negative.success && negative.channels[0].minVal==-100 && excursion.starts==3,
                "Fall must advance past newly measured negative peak");
        class CrossEdgeScope : public Scope {
        public:
            int getTotalChannel() override { return 2; }
            double measureSignalPeak(int ch,const QString& type) override {
                if(!autoMode && slope=="RISING" && type=="MINimum")return -50.0*ch;
                if(!autoMode && slope=="FALLING" && type=="MAXimum")return 80.0*ch;
                return Scope::measureSignalPeak(ch,type);
            }
        } cross;
        QMap<int,QPair<double,double>> captured;
        SteadyStateSearchStrategy both(2,1000,20,"BOTH");
        both.setCaptureObserver([&](const ScopeCaptureRecord& record){
            for(const auto& ch:record.channels){
                auto previous=captured.value(ch.channel,qMakePair(ch.maxVal,ch.minVal));
                captured[ch.channel]=qMakePair(std::max(previous.first,ch.maxVal),std::min(previous.second,ch.minVal));
            }
        });
        const auto crossResult=both.execute(&cross,stop);
        require(crossResult.success,"cross-edge BOTH failed");
        for(const auto& ch:crossResult.channels){
            require(ch.maxVal==80.0*ch.channel && ch.minVal==-50.0*ch.channel,
                    "opposite-edge extrema lost");
            require(ch.maxVal==captured[ch.channel].first && ch.minVal==captured[ch.channel].second,
                    "capture candidates disagree with strategy extrema");
        }
        for(const QString target:{QString("MAX"),QString("MIN")}){
            CrossEdgeScope single;
            const auto result=SteadyStateSearchStrategy(2,1000,20,target).execute(&single,stop);
            require(result.success,"single direction failed after BOTH change");
            require(target=="MAX"?result.channels[0].maxVal==2:result.channels[0].minVal==-2,
                    "opposite edge changed single direction");
        }
        Scope ok;
        auto result = strategy.execute(&ok, stop);
        require(result.success && ok.starts == 9 && ok.stops > 0, "no-progress termination failed");
        require(ok.minLevels.size() == 4 && std::abs(ok.minLevels[1] + 2.4) < 1e-9, "Min must start at AUTO reference minimum");
        Scope scaled; scaled.scale=20;
        require(strategy.execute(&scaled,stop).success && scaled.minLevels.size()==3 && std::abs(scaled.minLevels[1]+2.8)<1e-9,
                "20 units/div must advance by 0.8 units");
        Scope fine; fine.scale=2;
        require(SteadyStateSearchStrategy(2,1000,20).execute(&fine,stop).success
                && fine.minLevels.size()>2 && std::abs(fine.minLevels[1]+2.08)<1e-9,
                "2 units/div must advance by 0.08 units");
        for(double bad : {0.0,-1.0,std::numeric_limits<double>::quiet_NaN()}) {
            Scope invalidScale; invalidScale.scale=bad;
            auto failed=strategy.execute(&invalidScale,stop);
            require(!failed.success && failed.errorMessage.contains("scale") && invalidScale.stops>0,
                    "invalid scale must fail and stop");
        }
        require(result.channels[0].maxVal == 2 && result.channels[0].minVal == -2, "wrong peaks");
        require(std::isfinite(result.channels[0].rms), "RMS not measured");
        class SlowReference : public Scope {
        public:
            QString getAcquisitionState() override {
                if(autoMode) QThread::msleep(80);
                return Scope::getAcquisitionState();
            }
        };
        SlowReference slowReference;
        SteadyStateSearchStrategy patient(20,1000,100);
        require(patient.execute(&slowReference,stop).success,"AUTO reference used short trigger timeout");
        Scope timeout; timeout.timeoutAfterCapture=true;
        result=strategy.execute(&timeout, stop);
        require(result.success && timeout.reads == 8, "timeout reread stale waveform");
        require(result.channels[0].maxVal == 2 && result.channels[0].minVal == -2, "last valid data lost");
        Scope never; never.never=true;
        result=strategy.execute(&never, stop);
        require(!result.success && never.reads == 0 && result.errorMessage.contains("NA"), "first timeout falsely passed");
        Scope error; error.error=true;
        result=strategy.execute(&error, stop);
        require(!result.success && result.errorMessage.contains("communication"), "communication error treated as convergence");
        Scope invalid; invalid.invalid=true;
        require(!strategy.execute(&invalid, stop).success, "invalid peak passed");
        Scope failedStart; failedStart.startFails=true;
        for (double bad : {9.91e37, -9.91e37, 1e30, -1e30,
                           std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
            SecondaryScope secondary; secondary.bad = bad;
            const auto measured = strategy.execute(&secondary, stop);
            require(measured.success && measured.channels.size() == 2, "secondary invalid value broke search");
            const auto& ch = measured.channels[1];
            require(ch.maxVal == 2 && ch.minVal == -2, "invalid value polluted retained peaks");
            require(std::isnan(ch.rms) && std::isnan(ch.mean), "invalid RMS/Mean not normalized to NA");
            SecondaryScope trigger; trigger.bad = bad; trigger.triggerBad = true;
            const auto failed = strategy.execute(&trigger, stop);
            require(!failed.success && failed.errorMessage.contains("Trigger measurement: NA"),
                    "invalid trigger value did not fail search");
        }
        require(!strategy.execute(&failedStart, stop).success && failedStart.reads==0, "failed start read stale data");
        Scope limited;
        require(!SteadyStateSearchStrategy(2,1000,1).execute(&limited,stop).success, "iteration bound missing");
        Scope bounded; bounded.alwaysCapture=true;
        result=SteadyStateSearchStrategy(2,1000,4).execute(&bounded,stop);
        require(!result.success && bounded.starts==5, "Min iteration bound missing");
        Scope total; total.never=true;
        result=SteadyStateSearchStrategy(100,1,5).execute(&total,stop);
        require(!result.success && result.errorMessage.contains("timeout"), "total deadline missing");
        Scope cancelled; cancelled.cancel=&stop;
        result=strategy.execute(&cancelled,stop);
        require(!result.success && result.errorMessage.contains("Stopped") && cancelled.stops>0, "cancellation cleanup missing");
        std::cout << "normal trigger tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
