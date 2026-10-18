#include "measurement_scope_fixture.h"
#pragma once
#include "transientstrategy.h"
#include "oscilloscopestrategyfactory.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <cmath>
#include <functional>
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
class Scope : public MeasurementScopeFixture {
public:
    QString model()const override{return modelName;}
    QString vendor()const override{return "offline";}
    QByteArray captureScreenshot(const QString&,const QString&)override{return {};}
    QString getTriggerSource()override{return source;}
    QString getTriggerState()override{return "READY";}
    int getTotalChannel()override{return totalChannels;}
    bool isChannelEnabled(int)override{return enabled;}
    bool isClipping(int)override{return clipped && (!clipEventOnly || event) && (scale == 0 || capturedScale < requiredScale);}
    double getChannelScale(int)override{return scale == 0 ? 10.0 : scale;}
    bool queryConfiguration(const QString& cmd, QString& response, QString&) override {
        if (cmd.endsWith(":OFFSet?") || cmd.endsWith(":POSition?") || cmd.endsWith(":INVert?")) {response=QString::number(cmd.endsWith(":OFFSet?")?offsets.value(cmd.section(':',0,0)):cmd.endsWith(":POSition?")?positions.value(cmd.section(':',0,0)):0);return true;}
        return false;
    }
    double getTriggerLevel()override{return initial;}
    bool writeConfigurationCommand(const QString& cmd,QString& error)override{
        trace<<cmd;
        if(cmd.contains(":POSition ")) positions[cmd.section(':',0,0)]=cmd.section(' ',-1).toDouble();
        if(cmd.contains(":OFFSet ")) offsets[cmd.section(':',0,0)]=cmd.section(' ',-1).toDouble();
        if(onWrite)onWrite(cmd);
        if(cmd.startsWith("CH")&&cmd.contains(":SCAle ")) {
            if(rejectScale){error="scale rejected";return false;}
            if(!clampScale && scale != 0)scale=cmd.section(' ',-1).toDouble();
        }
        if(cmd=="TRIGger:A:MODe AUTO")autoMode=true;
        if(cmd=="TRIGger:A:MODe NORMAL")autoMode=false;
        if(cmd==failWrite){error="rejected";return false;}
        if(cmd.startsWith("TRIGger:A:LEVel:CH1 ") || cmd.startsWith("TRIGger:A:LEVel ")){level=cmd.section(' ',-1).toDouble();levels<<level;}
        if(cmd.startsWith("TRIGger:A:EDGE:SLOpe "))negative=cmd.endsWith("FALL");
        if(cmd=="ACQuire:STATE STOP")armed=false;
        return true;
    }
    bool beginSingleAcquisition()override{trace<<"ARM";armed=true;event=false;capturedScale=scale;++arms;if(onArm)onArm();return !failArm;}
    QString getAcquisitionState()override{
        if(onState)onState();
        if(commError)return "";
        if(autoMode)return "0";
        if(event){
            const bool hit=!never && !(negative ? missMin : missMax) && (!intermittent || events%3==0) && (negative ? level>=negativePeak && level<=steady : level<=(changing && events>12?9:peak) && level>=0);
            return hit?"0":"1";
        }
        const bool baseline = !single && (negative ? level>=-.5 && level<=0 : level<=steady && level>=0);
        return baseline?"0":"1";
    }
    double measureSignalPeak(int ch,const QString& type)override{
        require(!forbidRmsMean || (event && !autoMode) || (autoMode && allowReferenceQuartet) ||
                    (type != "RMS" && type != "MEAN"), "reference/Baseline queried RMS/Mean");
        ++reads;
        if(event && isClipping(ch))++clippedEventReads;
        if(onRead)onRead();
        if(ch==2 && invalidSecondary && !autoMode)return std::numeric_limits<double>::quiet_NaN();
        if(invalid)return std::numeric_limits<double>::quiet_NaN();
        if(autoMode)return steady;
        if(!event){++baselineReads;return negative?-.5:steady;}
        if(ch==2)return type=="MAXimum"?(events==1?42:5):type=="MINimum"?(events==1?-9:-4):2;
        return type=="MAXimum"?(changing && events>12?9:peak):type=="MINimum"?negativePeak:type=="RMS"?2:1;
    }
    bool waitForOperationComplete(int)override{throw std::runtime_error("OPC used");}
    bool forbidRmsMean = false;
    bool allowReferenceQuartet = false;
    int clippedEventReads=0;
    int totalChannels=2;
    double scale=0,capturedScale=0,requiredScale=4;
    bool rejectScale=false,clampScale=false;
    QString modelName="MSOSeries456", source="CH1";
    bool enabled=true,clipped=false,clipEventOnly=false,invalidSecondary=false;
    std::function<void(const QString&)> onWrite;
    std::function<void()> onArm,onState,onRead;
    QStringList trace;QVector<double> levels;QString failWrite;
    bool missMax=false,missMin=false;
    bool autoMode=false,changing=false,intermittent=false;
    bool armed=false,event=false,single=false,negative=false,failArm=false,commError=false,invalid=false,never=false;
    QMap<QString,double> offsets,positions;
    double negativePeak=-3,steady=1,peak=7,level=0,initial=.5;int arms=0,events=0,reads=0,baselineReads=0;
};
TransientContext context(Scope& s){
    TransientContext c;c.target="BOTH";c.timeoutMs=1;c.settleMs=0;c.dischargeMs=0;c.powerOffMs=0;
    c.phaseTimeoutMs=15000;c.maxIterations=100;c.trialsPerLevel=3;
    c.power=[&](bool on,QString&){s.trace<<(on?"POWER ON":"POWER OFF");
        if(s.armed&&!s.event&&((s.single&&on)||(!s.single&&!on))){s.event=true;++s.events;}return true;};
    c.load=[&](bool on,QString&){s.trace<<(on?"LOAD ON":"LOAD OFF");return true;};
    c.relays=[&](bool sh,bool dis,QString&){s.trace<<QString("RELAY %1 %2").arg(sh).arg(dis);
        if(s.armed&&!s.event&&sh){s.event=true;++s.events;}return true;};return c;
}
