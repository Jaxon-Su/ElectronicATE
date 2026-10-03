#pragma once
#include "scopetypes.h"
#include <QString>

// Manual operations only. Ownership and polling leases stay with the caller.
class IScopeManualControl
{
  public:
    virtual ~IScopeManualControl() = default;
    virtual bool isConnected() const = 0;
    virtual void disconnect() = 0;
    virtual QString lastError() const = 0;
    virtual bool isRunning() = 0;
    virtual bool beginSingleAcquisition() = 0;
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void automode() = 0;
    virtual void normal() = 0;
    virtual void setTriggerLevel(double level) = 0;
    virtual void setTriggerType(const QString &type) = 0;
    virtual void setTriggerSource(const QString &source) = 0;
    virtual void setTriggerSlope(const QString &slope) = 0;
    virtual int getTotalChannel() = 0;
    virtual double getChannelScale(int channel) = 0;
    virtual double readMeasurement(int channel, ScopeMeasurement measurement) = 0;
};
