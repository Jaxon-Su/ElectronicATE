#pragma once
#include "iscopeperiodsession.h"
#include "scopetypes.h"
#include <QString>
#include <memory>

// Measurement strategies depend on capabilities, not transport or file-transfer
// ownership.
class IScopeAcquisition
{
  public:
    virtual ~IScopeAcquisition() = default;
    // Start a fresh single record; Completed must not refer to an earlier record.
    virtual bool beginSingleAcquisition() = 0;
    virtual ScopeAcquisitionState acquisitionState() = 0;
    virtual ScopeTriggerState triggerState() = 0;
    virtual bool stopAcquisition(QString &error) = 0;
    virtual int waveformSettleTimeMs() const = 0;
    virtual void stop() = 0;
};

class IScopeChannels
{
  public:
    virtual ~IScopeChannels() = default;
    virtual int getTotalChannel() = 0;
    virtual bool isChannelEnabled(int channel) = 0;
    virtual double getChannelScale(int channel) = 0;
    virtual bool isClipping(int channel) = 0;
    virtual double readMeasurement(int channel, ScopeMeasurement measurement) = 0;
    virtual bool setVerticalScale(int channel, double scale, QString &error) = 0;
};

class IScopeMeasurement : public IScopeAcquisition, public IScopeChannels
{
  public:
    virtual QString lastError() const = 0;
    // Zero means an unavailable or non-analog trigger source; never guess a
    // channel.
    virtual int triggerChannel() = 0;
    // Checked operations report device rejection as well as transport failure.
    virtual bool selectEdgeTrigger(QString &error) = 0;
    virtual bool setEdgeSlope(ScopeTriggerSlope slope, QString &error) = 0;
    virtual bool setTriggerMode(ScopeTriggerMode mode, QString &error) = 0;
    virtual bool setChannelTriggerLevel(int channel, double level, QString &error) = 0;
};

class IScopeAutoPeriod : public IScopeMeasurement
{
  public:
    virtual std::unique_ptr<IScopePeriodSession> beginPeriodSession(IScopePeriodSession::Continue ready,
                                                                    QString &error) = 0;
};
