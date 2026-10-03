#pragma once
#include "scopetypes.h"
#include <QByteArray>
#include <QString>

struct ScopeCaptureSource {
    QString name;
    int channel = 0; // Zero denotes a non-analog or unavailable source.
};

// File capture and the metadata needed to identify the acquired record.
class IScopeCapture
{
  public:
    virtual ~IScopeCapture() = default;
    virtual QString model() const = 0;
    virtual QString lastError() const = 0;
    virtual bool isConnected() const = 0;
    virtual int getTotalChannel() = 0;
    virtual bool isChannelEnabled(int channel) = 0;
    virtual ScopeCaptureSource captureSource() = 0;
    virtual QString captureSlope() = 0;
    virtual double getTriggerLevel() = 0;
    virtual double getTimebase() = 0;
    virtual double getChannelScale(int channel) = 0;
    virtual double getChannelPosition(int channel) = 0;
    virtual ScopeAcquisitionState acquisitionState() = 0;
    // False reports a failed query; an unreadable numeric response yields NaN.
    virtual bool readChannelOffset(int channel, double &offset, QString &error) = 0;
    virtual bool waitForOperationComplete(int timeoutMs) = 0;
    virtual QByteArray captureScreenshot(const QString &format, const QString &path = {}) = 0;
    virtual bool captureWaveformFileToHost(int channel, const QString &path, const QString &format,
                                           const QString &instrumentPath, int startPoint = -1,
                                           int stopPoint = -1) = 0;
};
