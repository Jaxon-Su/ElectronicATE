#pragma once
#include "oscilloscope.h"
#include <QRegularExpression>

// Test-only bridge for the existing fault-injection fixtures and their SCPI traces.
// Production strategies cannot access these legacy hooks through IScopeMeasurement.
class MeasurementScopeFixture : public Oscilloscope {
public:
    using Oscilloscope::Oscilloscope;
    ScopeAcquisitionState acquisitionState() override {
        const auto value=getAcquisitionState().simplified().toUpper().section(' ', -1);
        if(value=="0" || value=="STOP" || value=="OFF") return ScopeAcquisitionState::Completed;
        if(value=="1" || value=="RUN" || value=="ON") return ScopeAcquisitionState::Running;
        return ScopeAcquisitionState::Invalid;
    }
    ScopeTriggerState triggerState() override {
        if(model()!="MSOSeries456") return ScopeTriggerState::Ready;
        const auto value=getTriggerState().simplified().toUpper().section(' ', -1);
        if(value=="READY")return ScopeTriggerState::Ready;
        if(value=="ARMED")return ScopeTriggerState::Arming;
        if(value=="TRIGGER")return ScopeTriggerState::Triggered;
        if(value=="AUTO")return ScopeTriggerState::Automatic;
        if(value=="SAVE")return ScopeTriggerState::Saving;
        return ScopeTriggerState::Invalid;
    }
    int triggerChannel() override {
        const auto match=QRegularExpression("(?:^|[ :])CH(\\d+)$", QRegularExpression::CaseInsensitiveOption)
                             .match(getTriggerSource().trimmed());
        return match.hasMatch()?match.captured(1).toInt():0;
    }
    double readMeasurement(int channel,ScopeMeasurement value) override {
        switch(value) {
        case ScopeMeasurement::Maximum:return measureSignalPeak(channel,"MAXimum");
        case ScopeMeasurement::Minimum:return measureSignalPeak(channel,"MINimum");
        case ScopeMeasurement::Rms:return measureSignalPeak(channel,"RMS");
        case ScopeMeasurement::Mean:return measureSignalPeak(channel,"MEAN");
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
    bool selectEdgeTrigger(QString& e) override {return writeConfigurationCommand("TRIGger:A:TYPe EDGE",e);}
    bool setEdgeSlope(ScopeTriggerSlope slope,QString& e) override {
        return writeConfigurationCommand(slope==ScopeTriggerSlope::Rise?"TRIGger:A:EDGE:SLOpe RISE":"TRIGger:A:EDGE:SLOpe FALL",e);
    }
    bool setTriggerMode(ScopeTriggerMode mode,QString& e) override {
        return writeConfigurationCommand(mode==ScopeTriggerMode::Auto?"TRIGger:A:MODe AUTO":"TRIGger:A:MODe NORMAL",e);
    }
    bool setChannelTriggerLevel(int channel,double value,QString& e) override {
        const auto prefix=model()=="MSOSeries456"?QString("TRIGger:A:LEVel:CH%1 ").arg(channel):QString("TRIGger:A:LEVel ");
        return writeConfigurationCommand(prefix+QString::number(value,'g',16),e);
    }
    bool setVerticalScale(int channel,double value,QString& e) override {
        return writeConfigurationCommand(QString("CH%1:SCAle %2").arg(channel).arg(value,0,'g',16),e);
    }
    bool stopAcquisition(QString& e) override {return writeConfigurationCommand("ACQuire:STATE STOP",e);}
};
