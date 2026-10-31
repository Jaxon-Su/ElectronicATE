#include "../support/measurement_scope_fixture.h"
#include "oscilloscopeacquisition.h"
#include "verticalscalecontrol.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace OscilloscopeAcquisition;
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
class Scope : public MeasurementScopeFixture {
public:
    QString model() const override { return "MSOSeries456"; }
    QString vendor() const override { return "offline"; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return {}; }
    int getTotalChannel() override { return 1; }
    bool isChannelEnabled(int) override { return true; }
    bool isClipping(int) override { return clipped; }
    double getChannelScale(int) override { return scale; }
    bool readVerticalRange(int, ScopeVerticalRange &range, QString &error) override {
        if (badQuery) { error="invalid range"; return false; }
        range={scale,offset,10,position,-8,8}; return true;
    }
    double measureSignalPeak(int, const QString& type) override { ++reads; require(!clipped,"read clipped data"); return type=="MAXimum"?high:low; }
    bool queryConfiguration(const QString& cmd, QString& r, QString&) override {
        if (badQuery) { r="invalid";return true; }
        r=QString::number(cmd.contains("OFFSet")?offset:cmd.contains("POSition")?position:inverted); return true;
    }
    bool writeConfigurationCommand(const QString& cmd, QString&) override {
        ++writes;
        if (reject) return false;
        if (!clamp) {
            const double value=cmd.section(' ',-1).toDouble();
            if(cmd.contains(":OFFSet ")) offset=value;
            else if(cmd.contains(":POSition ")) position=value;
            else scale=value;
        }
        return true;
    }
    double scale=10,high=20,low=-20,offset=0,position=0,inverted=0;
    bool clipped=false,badQuery=false,reject=false,clamp=false;
    int reads=0,writes=0;
};
class ReferenceScope : public Scope {
public:
    QString getTriggerSource() override { return "CH1"; }
    bool beginSingleAcquisition() override { ++captures;high=captures==1?100:90;low=captures==1?-30:-20;return true; }
    QString getAcquisitionState() override { return "0"; }
    bool writeConfigurationCommand(const QString& cmd,QString& error) override {
        return cmd.startsWith("CH")?Scope::writeConfigurationCommand(cmd,error):true;
    }
    int captures=0;
};
class DeadlineScope : public ReferenceScope {
public:
    int getTotalChannel() override { return channels; }
    bool beginSingleAcquisition() override { ++captures; readyTimer.start(); return true; }
    double measureSignalPeak(int channel, const QString &type) override {
        measuredChannels.append(channel);
        if (type == "MAXimum") ++maxReads;
        if (type == "MINimum") ++minReads;
        if (cancel && maxReads == 2) cancel->storeRelease(1);
        if (lateReadMs) QThread::msleep(lateReadMs);
        if (channel < stalledChannel) return type == "MAXimum" ? 19 : -5;
        if (type == "MAXimum" && maxReadyMs >= 0 && readyTimer.elapsed() >= maxReadyMs) return 19;
        if (type == "MINimum" && minReadyMs >= 0 && readyTimer.elapsed() >= minReadyMs) return -5;
        return std::numeric_limits<double>::quiet_NaN();
    }
    bool stopAcquisition(QString &) override { ++stops; return true; }
    bool setTriggerMode(ScopeTriggerMode mode, QString &) override {
        if (mode == ScopeTriggerMode::Normal) ++normalModes;
        return true;
    }
    int maxReads=0, minReads=0, stops=0, normalModes=0;
    int channels=1, stalledChannel=1, maxReadyMs=-1, minReadyMs=-1, lateReadMs=0;
    QElapsedTimer readyTimer;
    QVector<int> measuredChannels;
    QAtomicInt *cancel=nullptr;
};

void checkDeadlineDiagnostics(QAtomicInt &stop)
{
    for (int captureMode : {0, 1, 2}) {
        DeadlineScope scope; scope.maxReadyMs=120;
        VerticalScaleState state; state.options.margin=true; state.includeRmsMean=false;
        int saved=0;
        if (captureMode) state.captureRecord=[&](const ScopeCaptureRecord &record) {
            ++saved;
            require(record.channels.size()==1 && record.channels[0].maxVal==19 &&
                    std::isnan(record.channels[0].minVal),"timeout diagnostic lost available Max/NA Min");
            if (captureMode==2) throw std::runtime_error("diagnostic save failed");
        };
        QElapsedTimer timer; timer.start();
        const auto result=captureAutoReference(&scope,stop,240,&state);
        require(!result.success && result.errorMessage.contains("timeout") && timer.elapsed()<900 &&
                scope.minReads>0 && saved==(captureMode?1:0) && scope.captures==1 && scope.writes==0 &&
                scope.stops==1 && scope.normalModes==1,"Min restarted budget or timeout lost Capture/cleanup");
        require(result.errorMessage.contains("diagnostic save failed")==bool(captureMode==2),
                "Capture failure lost original timeout or save error");
    }
    // CH1 needs expansion, CH2 stalls: retain metadata, but do not write a partial expansion or query CH3.
    DeadlineScope multiple; multiple.channels=3; multiple.stalledChannel=2; multiple.scale=4;
    VerticalScaleState state; state.options.margin=true; state.includeRmsMean=false;
    int saved=0;
    state.captureRecord=[&](const ScopeCaptureRecord &record) {
        ++saved; require(record.channels.size()==3 && record.channels[0].maxVal==19 &&
                         std::isnan(record.channels[1].maxVal),"cross-channel diagnostic incomplete");
    };
    QElapsedTimer timer; timer.start();
    auto result=captureAutoReference(&multiple,stop,160,&state);
    require(!result.success && result.errorMessage.contains("timeout") && timer.elapsed()<900 && saved==1 &&
            multiple.writes==0 && multiple.captures==1 && !multiple.measuredChannels.contains(3),
            "deadline reset per channel or partial expansion applied");
    // A single synchronous I/O can return late; it must not start a Min query after returning.
    DeadlineScope late; late.maxReadyMs=0; late.lateReadMs=80; saved=0;
    state.captureRecord=[&](const ScopeCaptureRecord &record) {
        ++saved; require(record.channels[0].maxVal==19,"late valid Max lost from diagnostic");
    };
    result=captureAutoReference(&late,stop,30,&state);
    require(!result.success && result.errorMessage.contains("timeout") && saved==1 && late.minReads==0 &&
            late.writes==0 && late.stops==1 && late.normalModes==1,"late read continued measurement or lost cleanup");
    // Stop beats diagnostics and still runs cleanup independently of the stop flag.
    DeadlineScope cancelled; cancelled.cancel=&stop; saved=0;
    state.captureRecord=[&](const auto &) { ++saved; };
    timer.restart(); result=captureAutoReference(&cancelled,stop,1000,&state);
    require(!result.success && result.errorMessage.contains("Stopped by user") && saved==0 &&
            timer.elapsed()<900 && cancelled.minReads==0 && cancelled.stops==1 && cancelled.normalModes==1,
            "Stop waited for deadline, saved diagnostics, or bypassed cleanup");
    stop.storeRelease(0);
    // Saving a valid completed record can cross the phase deadline, without permitting a scale write.
    DeadlineScope slowSave; slowSave.maxReadyMs=slowSave.minReadyMs=0; slowSave.scale=4; saved=0;
    state.captureRecord=[&](const auto &) { ++saved; QThread::msleep(80); };
    result=captureAutoReference(&slowSave,stop,40,&state);
    require(!result.success && result.errorMessage.contains("timeout") && saved==1 && slowSave.writes==0 &&
            slowSave.captures==1 && slowSave.stops==1 && slowSave.normalModes==1,
            "Capture deadline allowed rescale/reacquisition or skipped cleanup");
    // Entering the helper at expiry or with a late completed record still preserves the diagnostic.
    for (bool completedLate : {false,true}) {
        Scope scope; int adjustments=0; saved=0;
        state.captureRecord=[&](const auto &record) { ++saved; require(record.channels.size()==1,"channel metadata lost"); };
        QString error;
        try { expandVerticalRange(&scope,stop,adjustments,&state,"Measurement",completedLate,QDeadlineTimer(0)); }
        catch (const std::exception &e) { error=QString::fromUtf8(e.what()); }
        require(error.contains("timeout") && saved==1 && scope.reads==0 && scope.writes==0,
                "already expired or late record lost diagnosis or read measurements");
        require(completedLate || error.contains("time limit"),
                "phase expiry changed the existing time-limit error category");
    }
}
void positionCases(QAtomicInt &stop)
{
    VerticalScaleState state; state.options.margin=true;
    // Independent coordinates: DC at the upper/lower edge; zero and AC at center.
    for (double polarity : {-1.,1.}) {
        Scope scope; scope.scale=4; scope.high=scope.low=20*polarity; int n=0;
        require(expandVerticalRange(&scope,stop,n,&state) && scope.scale==4 &&
                std::abs(scope.position+1.05*polarity)<1e-9 && scope.offset==0,"DC edge placement");
        require(!expandVerticalRange(&scope,stop,n,&state) && n==1,"DC repeated adjustment/jitter");
    }
    Scope zero; zero.high=zero.low=0; zero.position=6; int n=0;
    require(expandVerticalRange(&zero,stop,n,&state) && zero.position==0 && zero.scale==10,"zero line center");
    Scope fit; fit.scale=4; fit.high=19; fit.low=-5; n=0;
    require(expandVerticalRange(&fit,stop,n,&state) && fit.scale==4 && fit.position==-1.75,"fitting AC span center");
    Scope wide; wide.scale=4; wide.high=19; wide.low=-19; n=0;
    require(expandVerticalRange(&wide,stop,n,&state) && std::abs(wide.scale-4.810126582278481)<1e-9 &&
            wide.position==0,"wide symmetric span expansion");
    // Synthetic geometry reproducing the issue's requested 106.609375 scale, not a measured hardware Offset.
    Scope issue; issue.scale=100; issue.offset=417.14062; issue.high=8.875; issue.low=-9.29688; n=0;
    require(expandVerticalRange(&issue,stop,n,&state) && issue.scale==100 &&
            std::abs(issue.position-4.1735156)<1e-9 && issue.offset==417.14062,"Issue9 Offset/Position conflation");
    Scope inside; inside.position=1; inside.high=10; inside.low=-10; n=0;
    require(!expandVerticalRange(&inside,stop,n,&state) && inside.position==1 && inside.writes==0,"within margin moved");
    Scope bounded; bounded.scale=1; bounded.offset=100.8; bounded.high=bounded.low=96; n=0;
    bounded.position=-1;
    require(expandVerticalRange(&bounded,stop,n,&state) && bounded.position<=8 && bounded.position>=-8 &&
            bounded.scale>1 && bounded.offset==100.8,"Position limit changed Offset or failed to expand Scale");
    class Rounded : public Scope {
    public:
        bool setVerticalScale(int,double,QString &) override { scale=8;return true; }
    } rounded;
    rounded.scale=4; rounded.high=30; rounded.low=-20; n=0;
    require(expandVerticalRange(&rounded,stop,n,&state) && rounded.scale==8 && rounded.position==-0.625,
            "Position calculated from requested instead of actual Scale");
    // A frozen diagnostic precedes position writes, even when the device ignores them.
    Scope clamp; clamp.scale=4; clamp.high=19; clamp.low=-5; clamp.clamp=true; n=0;
    int saved=0, snapshots=0; QStringList logs;
    state.captureRecord=[&](const auto &) {require(clamp.writes==0,"diagnostic after write");++saved;};
    state.options.beforePositionChange=[&](int,double old,double target) {
        require(old==0 && target==-1.75,"Position snapshot wrong");++snapshots;
    };
    state.options.log=[&](const auto &line) {logs<<line;};
    QString error; QElapsedTimer timer; timer.start();
    try {expandVerticalRange(&clamp,stop,n,&state,"Test",false,QDeadlineTimer(80));}
    catch(const std::exception &e) {error=QString::fromUtf8(e.what());}
    require(error.contains("timeout") && error.contains("position") && error.contains("requested=-1.75 actual=0") &&
            saved==1 && snapshots==1 && !logs.isEmpty() && timer.elapsed()<700,
            "Position clamp exceeded phase, lost diagnostics or snapshot");
    Scope scaleClamp;scaleClamp.scale=4;scaleClamp.high=19;scaleClamp.low=-19;scaleClamp.clamp=true;n=0;
    VerticalScaleState scaleState;scaleState.options.margin=true;error.clear();timer.restart();
    try {expandVerticalRange(&scaleClamp,stop,n,&scaleState,"Test",false,QDeadlineTimer(80));}
    catch(const std::exception &e) {error=QString::fromUtf8(e.what());}
    require(error.contains("timeout") && error.contains("scale") && error.contains("requested=") &&
            error.contains("actual=4") && timer.elapsed()<700,"Scale timeout lost requested/actual diagnostic or exceeded Phase");
    for(int bad=0;bad<4;++bad) {
        class Bad : public Scope {
        public:
            int bad=0;
            bool readVerticalRange(int ch,ScopeVerticalRange &r,QString &e) override {
                Scope::readVerticalRange(ch,r,e);
                if(bad==0)r.position=NAN;
                if(bad==1)r.minimumPosition=NAN;
                if(bad==2)r.maximumPosition=0;
                if(bad==3)r.position=9;
                return true;
            }
        } invalid;
        invalid.bad=bad; invalid.scale=4;invalid.high=19;invalid.low=-5; n=0;
        VerticalScaleState clean;clean.options.margin=true;bool failed=false;
        try {expandVerticalRange(&invalid,stop,n,&clean);}catch(...){failed=true;}
        require(failed && invalid.writes==0,"invalid Position geometry accepted");
    }
}

void checkedReadbackDeadlineCases(QAtomicInt &stop)
{
    class LateValid : public Scope {
    public:
        int delayMs=0;
        double getChannelScale(int ch) override {
            QThread::msleep(delayMs); return Scope::getChannelScale(ch);
        }
        bool readVerticalRange(int ch,ScopeVerticalRange &range,QString &error) override {
            QThread::msleep(delayMs); return Scope::readVerticalRange(ch,range,error);
        }
    } scope;
    require(setScaleChecked(&scope,stop,1,12)==12 && setPositionChecked(&scope,stop,1,1)==1,
            "prompt valid checked readback rejected");
    scope.delayMs=2050;
    QStringList errors;
    try {setScaleChecked(&scope,stop,1,14);} catch(const std::exception &e) {errors<<e.what();}
    try {setPositionChecked(&scope,stop,1,2);} catch(const std::exception &e) {errors<<e.what();}
    require(errors.size()==2 && errors[0].contains("CH1 scale") && errors[0].contains("requested=14 actual=14") &&
            errors[1].contains("CH1 position") && errors[1].contains("requested=2 actual=2"),
            "late valid Scale/Position readback incorrectly accepted beyond 2 seconds");
}

int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    try {
        QAtomicInt stop(0);
        checkedReadbackDeadlineCases(stop);
        positionCases(stop);
        if (argc > 1 && QString::fromUtf8(argv[1]) == "position") {
            Scope scope; scope.scale=4; scope.high=19; scope.low=-5;
            VerticalScaleState state; state.options.margin=true; int adjustments=0;
            require(expandVerticalRange(&scope,stop,adjustments,&state) && scope.scale==4 &&
                    std::abs(scope.position+1.75)<1e-9 && scope.offset==0,
                    "Issue9: fitting span must center by Position without enlarging Scale");
            return 0;
        }
        class CompletedScope : public Scope {
        public:
            QString getAcquisitionState() override {
                ++queries;
                if(cancel && queries==1)cancel->storeRelease(1);
                return queries>1?finalState:"0";
            }
            int queries=0;QString finalState="0";QAtomicInt* cancel=nullptr;
        } completedScope;
        QElapsedTimer completionTimer;completionTimer.start();
        require(waitForCapture(&completedScope,stop,100)==AcquisitionResult::Completed && completionTimer.elapsed()<1000
                && completedScope.queries==2,"completed record delayed or frozen-state check missing");
        for(const QString response : {QString("1"),QString("invalid")}) {
            CompletedScope invalid;invalid.finalState=response;
            require(waitForCapture(&invalid,stop,100)==(response=="1"?AcquisitionResult::CaptureTimeout:AcquisitionResult::CommError),
                    "unfrozen or invalid record accepted");
        }
        CompletedScope stoppedScope;stoppedScope.cancel=&stop;
        require(waitForCapture(&stoppedScope,stop,100)==AcquisitionResult::Cancelled,"completion cancellation ignored");
        stop.storeRelease(0);
        CompletedScope cleanupScope;bool exited=false;
        require(waitForCapture(&cleanupScope,stop,100,100,[&] {
            require(cleanupScope.queries==1,"short exit did not start immediately after STOP");exited=true;
        })==AcquisitionResult::Completed && exited && cleanupScope.queries==2,"short exit skipped");
        CompletedScope cancelledExit;
        require(waitForCapture(&cancelledExit,stop,100,100,[&] {stop.storeRelease(1);})==AcquisitionResult::Cancelled
                && cancelledExit.queries==1,"Stop during short exit exposed record");
        stop.storeRelease(0);
        CompletedScope failedExit;bool exitFailed=false;
        try { waitForCapture(&failedExit,stop,100,100,[] {throw std::runtime_error("OFF failed");}); }
        catch(const std::runtime_error&) {exitFailed=true;}
        require(exitFailed && failedExit.queries==1,"failed short exit exposed record");
        // A late STOP reply still delivers the frozen record, with explicit timeout status.
        class LateReplyScope : public Scope {
        public:
            QString getAcquisitionState() override {QThread::msleep(50);return "0";}
        } lateReply;
        bool lateRecordDelivered=false;
        require(waitForCapture(&lateReply,stop,5,5,[&] {lateRecordDelivered=true;})==AcquisitionResult::CompletedAfterTimeout
                && lateRecordDelivered,"late STOP lost the diagnostic record or timeout status");
        class LastPollScope : public Scope {
        public:
            int queries=0;
            QString getAcquisitionState() override {return ++queries==1?"1":"0";}
            QString getTriggerState() override {return "READY";}
        } lastPoll;
        require(waitForCapture(&lastPoll,stop,3)==AcquisitionResult::CompletedAfterTimeout&&lastPoll.queries==3,
                "record completed during last sleep was discarded as MISS");
        Scope ignored;VerticalScaleState ignore;int count=0;
        require(!expandVerticalRange(&ignored,stop,count,&ignore)&&ignored.reads==0&&ignored.writes==0,
                "unclipped record auto adjusted");
        ignored.clipped=true;
        require(expandVerticalRange(&ignored,stop,count,&ignore)&&ignored.scale==20&&ignored.reads==0,
                "clipping did not double scale");
        ignored.clipped=false;ignored.high=60;ignored.low=-60;
        require(!expandVerticalRange(&ignored,stop,count,&ignore)&&ignored.scale==20,
                "removed margin fit still active after clipping");
        for(int failure=0;failure<4;++failure){
            Scope scope;scope.clipped=true;scope.reject=failure==0;scope.clamp=failure==1;
            if(failure==2)scope.scale=0;
            int n=failure==3?8:0;bool failed=false;
            try{expandVerticalRange(&scope,stop,n,&ignore);}catch(...){failed=true;}
            require(failed,"clipping failure accepted");
        }
        Scope margin; margin.scale=4; margin.high=19; margin.low=-5;
        VerticalScaleState marginState; marginState.options.margin=true;
        int marginCount=0;
        require(expandVerticalRange(&margin,stop,marginCount,&marginState) && margin.scale==4 && margin.position==-1.75,
                "10% margin did not center fitting waveform");
        margin.high=10;
        require(!expandVerticalRange(&margin,stop,marginCount,&marginState) && margin.scale==4,
                "smaller waveform shrank scale");
        margin.high=29;
        require(expandVerticalRange(&margin,stop,marginCount,&marginState) && std::abs(margin.scale-4.30379746835443)<1e-9,
                "next wider waveform did not retain 10% margin");
        // Independent numerical oracles: negative edge, offset, reported inverted offset.
        for (double polarity : {-1.0,1.0}) {
            Scope shifted; shifted.scale=4; shifted.offset=100*polarity;
            shifted.high=polarity>0?105:-81; shifted.low=polarity>0?81:-105;
            shifted.position=3; int n=0;
            // Force a display violation for both reported Offset polarities.
            shifted.position=6;
            require(expandVerticalRange(&shifted,stop,n,&marginState) && shifted.scale==4 &&
                    shifted.offset==100*polarity && shifted.position==1.75*polarity,"offset/inverted range centered incorrectly");
        }
        Scope negative; negative.scale=4; negative.high=5; negative.low=-19; int n=0;
        require(expandVerticalRange(&negative,stop,n,&marginState) && negative.scale==4 && negative.position==1.75,"negative edge ignored");
        Scope clippedMargin; clippedMargin.scale=4; clippedMargin.clipped=true; n=0;
        require(expandVerticalRange(&clippedMargin,stop,n,&marginState) && clippedMargin.scale==8 &&
                clippedMargin.reads==0,"clipping margin used truncated extrema instead of doubling");
        // Preserve diagnostic records before reporting bad geometry; no partial write.
        for (int bad=0;bad<6;++bad) {
            class InvalidRange : public Scope {
            public:
                int bad=0;
                bool readVerticalRange(int,ScopeVerticalRange &r,QString &e) override {
                    r={4,0,10,position,-8,8};
                    if (bad==0) {e="range IO failed";return false;}
                    if (bad==1) r.scale=0;
                    if (bad==2) r.center=std::numeric_limits<double>::quiet_NaN();
                    if (bad==3) r.divisions=0;
                    return true;
                }
            } invalid;
            invalid.bad=bad; invalid.high=bad==4?-30:19; invalid.low=-5;
            if (bad==5) invalid.reject=true;
            auto options=marginState; int saved=0; n=0;
            options.captureRecord=[&](const auto&) { ++saved; require(invalid.writes==0,"diagnostic saved after scale write"); };
            bool failed=false; try {expandVerticalRange(&invalid,stop,n,&options);} catch (...) {failed=true;}
            require(failed && saved==1 && invalid.writes==(bad==5?1:0),"invalid margin accepted or diagnostic lost");
        }
        class Channels : public Scope {
        public:
            QMap<int,double> scales{{1,4},{2,4},{3,4}};
            QMap<int,double> positions{{1,0},{2,0},{3,0}};
            QVector<int> measured,changed;
            int getTotalChannel() override {return 3;}
            bool isChannelEnabled(int ch) override {return ch!=2;}
            bool readVerticalRange(int ch,ScopeVerticalRange &r,QString &) override {r={scales[ch],0,10,positions[ch],-8,8};return true;}
            bool setVerticalPosition(int ch,double v,QString &) override {changed<<ch;positions[ch]=v;return true;}
            double getChannelScale(int ch) override {return scales[ch];}
            double measureSignalPeak(int ch,const QString &type) override {
                measured<<ch; return type=="MAXimum"?(ch==1?19:12):-5;
            }
            bool setVerticalScale(int ch,double v,QString &) override {changed<<ch;scales[ch]=v;return true;}
        } channels;
        n=0; require(expandVerticalRange(&channels,stop,n,&marginState) && channels.changed==QVector<int>{1}
                     && !channels.measured.contains(2) && channels.scales[1]==4 && channels.positions[1]==-1.75 && channels.scales[3]==4,
                     "margin touched disabled/within-range channels");
        // Actual AUTO loop must re-acquire and use the second record after expansion.
        class Fresh : public ReferenceScope {
        public:
            bool beginSingleAcquisition() override {++captures;high=captures==1?19:10;low=-5;return true;}
        } fresh;
        fresh.scale=4; auto freshState=marginState; freshState.includeRmsMean=false;
        QVector<double> captured;
        freshState.captureRecord=[&](const auto &record) {captured<<record.channels[0].maxVal;};
        const auto reference=captureAutoReference(&fresh,stop,2000,&freshState);
        require(reference.success && fresh.captures==2 && fresh.scale==4 && fresh.position==-1.75 && captured==QVector<double>({19,10}) &&
                reference.channels[0].maxVal==10,"expanded AUTO accepted old record or lost diagnostics");
        // Legal NA must consume the shared phase budget, not 5 s for each metric.
        DeadlineScope deadlineScope;
        auto deadlineState=marginState; deadlineState.includeRmsMean=false;
        QElapsedTimer deadlineTimer; deadlineTimer.start();
        const auto timedOut=captureAutoReference(&deadlineScope,stop,1000,&deadlineState);
        std::cout << "AUTO margin NA phase=1000 elapsed=" << deadlineTimer.elapsed()
                  << " max_reads=" << deadlineScope.maxReads << " min_reads=" << deadlineScope.minReads << '\n';
        require(!timedOut.success && timedOut.errorMessage.contains("timeout") && deadlineTimer.elapsed()<1800 &&
                deadlineScope.maxReads>0 && deadlineScope.minReads==0 && deadlineScope.captures==1 &&
                deadlineScope.writes==0 && deadlineScope.stops==1 && deadlineScope.normalModes==1,
                "margin NA exceeded phase budget, read Min after expiry, or skipped AUTO cleanup");
        checkDeadlineDiagnostics(stop);
        Scope limit; limit.scale=4; limit.high=19; limit.low=-5; n=8; bool limited=false;
        try {expandVerticalRange(&limit,stop,n,&marginState);} catch (...) {limited=true;}
        require(limited && limit.writes==0,"margin bypassed adjustment limit");
        Scope stoppedMargin; stoppedMargin.scale=4; stoppedMargin.high=19; stoppedMargin.low=-5;
        auto cancelledState=marginState;
        cancelledState.options.beforePositionChange=[&](int,double,double) {stop.storeRelease(1);};
        n=0; bool cancelledBeforeWrite=false;
        try {expandVerticalRange(&stoppedMargin,stop,n,&cancelledState);} catch (...) {cancelledBeforeWrite=true;}
        require(cancelledBeforeWrite && stoppedMargin.writes==0,"Stop before margin write ignored");
        stop.storeRelease(0);
        stop.storeRelease(1);bool cancelled=false;
        try{expandVerticalRange(&ignored,stop,count,&ignore);}catch(...){cancelled=true;}
        require(cancelled,"cancel ignored");
        std::cout<<"vertical scale tests passed\n";
    }catch(const std::exception& ex){std::cerr<<ex.what();return 1;}
}
