#include "../support/measurement_scope_fixture.h"
#include "oscilloscopeacquisition.h"
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
int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    try {
        QAtomicInt stop(0);
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
        require(!expandClippedChannels(&ignored,stop,count,&ignore)&&ignored.reads==0&&ignored.writes==0,
                "unclipped record auto adjusted");
        ignored.clipped=true;
        require(expandClippedChannels(&ignored,stop,count,&ignore)&&ignored.scale==20&&ignored.reads==0,
                "clipping did not double scale");
        ignored.clipped=false;ignored.high=60;ignored.low=-60;
        require(!expandClippedChannels(&ignored,stop,count,&ignore)&&ignored.scale==20,
                "removed margin fit still active after clipping");
        for(int failure=0;failure<4;++failure){
            Scope scope;scope.clipped=true;scope.reject=failure==0;scope.clamp=failure==1;
            if(failure==2)scope.scale=0;
            int n=failure==3?8:0;bool failed=false;
            try{expandClippedChannels(&scope,stop,n,&ignore);}catch(...){failed=true;}
            require(failed,"clipping failure accepted");
        }
        stop.storeRelease(1);bool cancelled=false;
        try{expandClippedChannels(&ignored,stop,count,&ignore);}catch(...){cancelled=true;}
        require(cancelled,"cancel ignored");
        std::cout<<"vertical scale tests passed\n";
    }catch(const std::exception& ex){std::cerr<<ex.what();return 1;}
}
