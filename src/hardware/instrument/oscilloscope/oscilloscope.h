#pragma once
#include <limits>
#include <QAtomicInt>
#include "instrumentwithcommbase.h"
#include "iscopemeasurement.h"
#include "iscopecapture.h"
#include "iscopemanualcontrol.h"
#include "iscopeconfiguration.h"
#include <stdexcept>
#include <QString>
#include <QList>
#include <QVector>
#include <QByteArray>
#include <QDateTime>
#include <QFile>

// 波形數據結構
struct WaveformData {
    QVector<double> timePoints;
    QVector<double> voltagePoints;
    int channel;
    double sampleRate;
    double timeBase;
    double voltageScale;
    QString timestamp;
    int recordLength;

    WaveformData() : channel(1), sampleRate(0), timeBase(0), voltageScale(0), recordLength(0) {}
};

// 可擴充的示波器抽象父類
class Oscilloscope : public InstrumentWithCommBase, public IScopeAutoPeriod, public IScopeCapture,
                     public IScopeManualControl, public IScopeConfiguration
{

  public:
    explicit Oscilloscope(ICommunication *comm = nullptr) : InstrumentWithCommBase(comm) {}

    virtual ~Oscilloscope() = default;
    QString model() const override = 0;
    void disconnect() override { InstrumentWithCommBase::disconnect(); }
    bool applySettings(const OscilloscopeSettings &, QAtomicInt &, QString &error) override
    { return unsupportedSetting("applySettings", error); }
    ScopeCaptureSource captureSource() override { return unavailable("captureSource", ScopeCaptureSource{}); }
    QString captureSlope() override { return unavailable("captureSlope", QString{}); }
    bool readChannelOffset(int, double &, QString &error) override
    { return unsupportedSetting("readChannelOffset", error); }
    bool isConnected() const override { return InstrumentWithCommBase::isConnected(); }
    QString lastError() const override { return InstrumentWithCommBase::lastError(); }

    ScopeAcquisitionState acquisitionState() override
    { return unavailable("acquisitionState", ScopeAcquisitionState::Invalid); }
    ScopeTriggerState triggerState() override
    { return unavailable("triggerState", ScopeTriggerState::Invalid); }
    int triggerChannel() override { return unavailable("triggerChannel", 0); }
    double readMeasurement(int, ScopeMeasurement) override
    { return unavailable("readMeasurement", std::numeric_limits<double>::quiet_NaN()); }
    bool stopAcquisition(QString &error) override { return unsupportedSetting("stopAcquisition", error); }
    bool selectEdgeTrigger(QString &error) override { return unsupportedSetting("selectEdgeTrigger", error); }
    bool setEdgeSlope(ScopeTriggerSlope, QString &error) override
    { return unsupportedSetting("setEdgeSlope", error); }
    bool setTriggerMode(ScopeTriggerMode, QString &error) override
    { return unsupportedSetting("setTriggerMode", error); }
    bool setChannelTriggerLevel(int, double, QString &error) override
    { return unsupportedSetting("setChannelTriggerLevel", error); }
    bool setVerticalScale(int, double, QString &error) override
    { return unsupportedSetting("setVerticalScale", error); }

    // 必須實作的純虛擬函數

    // 基本設定功能

    // 預設實作（可被override）

    // 基本擷取功能
    virtual QByteArray captureScreenshot(const QString &format, const QString &savePath = QString()) = 0;
    virtual void setTimebase(double timePerDiv) { unsupported("setTimebase"); }
    virtual void setChannelScale(int channel, double Div) { unsupported("setChannelScale"); }
    virtual void setTriggerLevel(double level) { unsupported("setTriggerLevel"); }

    // 通道設定
    virtual void setChannelPosition(int channel, double position) { unsupported("setChannelPosition"); }
    virtual void setChannelCoupling(int channel, const QString &coupling)
    {
        unsupported("setChannelCoupling");
    }
    virtual void setChannelBandwidth(int channel, double bandwidth) { unsupported("setChannelBandwidth"); }
    virtual void setProbeRatio(int channel, double ratio) { unsupported("setProbeRatio"); }
    virtual void enableChannel(int channel, bool enabled) { unsupported("enableChannel"); }

    // 水平軸設定
    virtual void setHorizontalPosition(double position) { unsupported("setHorizontalPosition"); }

    virtual void setRecordLength(int length) { unsupported("setRecordLength"); }
    virtual void setSampleRate(double rate) { unsupported("setSampleRate"); }

    // 觸發設定
    virtual void setTriggerSource(const QString &source) { unsupported("setTriggerSource"); }
    virtual void setTriggerType(const QString &type) { unsupported("setTriggerType"); }
    virtual void setTriggerSlope(const QString &slope) { unsupported("setTriggerSlope"); }
    virtual void setTriggerCoupling(const QString &coupling) { unsupported("setTriggerCoupling"); }

    // 控制功能
    virtual void autoSetup() { unsupported("autoSetup"); }
    virtual void automode() { unsupported("automode"); }
    virtual void run() { unsupported("run"); }
    virtual void stop() { unsupported("stop"); }
    virtual bool queryConfiguration(const QString &, QString &, QString &error)
    {
        error = "Configuration queries are not supported";
        return false;
    }
    virtual bool writeConfigurationCommand(const QString &, QString &error)
    {
        error = "Checked configuration is unsupported for this oscilloscope";
        return false;
    }
    virtual void single() { unsupported("single"); }
    std::unique_ptr<IScopePeriodSession> beginPeriodSession(IScopePeriodSession::Continue,
                                                           QString &error) override
    {
        error = "Auto Period is unsupported for this oscilloscope";
        return {};
    }
    virtual bool prepareSteadyAcquisition(QAtomicInt &, QString &error)
    {
        error = "Steady acquisition startup is unsupported for this oscilloscope";
        return false;
    }
    virtual bool beginSingleAcquisition() { return unavailable("beginSingleAcquisition", false); }
    // Driver-specific display/measurement settling guard after a completed acquisition.
    virtual int waveformSettleTimeMs() const { return 0; }
    virtual void normal() { unsupported("normal"); }
    virtual void force() { unsupported("force"); }
    virtual void continuous() { unsupported("continuous"); }

    // 半自動测量功能暫時加入
    virtual double measureSignalPeak(int channel, const QString &measureType = "MAXimum")
    {
        return unavailable("measureSignalPeak", std::numeric_limits<double>::quiet_NaN());
    }
    virtual bool isClipping(int channel)
    {
        Q_UNUSED(channel);
        unsupported("isClipping");
    }
    virtual bool waitForOperationComplete(int timeoutMs = 5000) { return unavailable("waitForOperationComplete", false); }
    virtual QString getSystemError() { return unavailable("getSystemError", QString{}); }
    virtual void clearErrors() { unsupported("clearErrors"); }

    // 相關查詢
    virtual QString getTriggerSlope() { return unavailable("getTriggerSlope", QString{}); }
    virtual QString getTriggerSource() { return unavailable("getTriggerSource", QString{}); }
    virtual QString getTriggerType() { return unavailable("getTriggerType", QString{}); }
    virtual double getTriggerLevel() { return unavailable("getTriggerLevel", std::numeric_limits<double>::quiet_NaN()); }
    virtual QString getTriggerMode() { return unavailable("getTriggerMode", QString{}); }
    virtual double getChannelPosition(int channel) { return unavailable("getChannelPosition", std::numeric_limits<double>::quiet_NaN()); }
    virtual double getHorizontalPosition() { return unavailable("getHorizontalPosition", std::numeric_limits<double>::quiet_NaN()); }
    virtual double getChannelScale(int channel) { return unavailable("getChannelScale", std::numeric_limits<double>::quiet_NaN()); }
    virtual bool isChannelEnabled(int channel) { return unavailable("isChannelEnabled", false); }
    virtual bool isMathChannelEnabled(int channel) { return unavailable("isMathChannelEnabled", false); }
    virtual double getTimebase() { return unavailable("getTimebase", std::numeric_limits<double>::quiet_NaN()); }
    virtual QString getAcquisitionState() { return unavailable("getAcquisitionState", QString{}); }
    virtual QString getTriggerState() { return unavailable("getTriggerState", QString{}); }
    virtual QString getStopAfterMode() { return unavailable("getStopAfterMode", QString{}); }
    virtual bool isRunning() { return unavailable("isRunning", false); }
    virtual int getTotalChannel() { return unavailable("getTotalChannel", 0); }

    // 擷取模式
    // mode: "SAMple" | "PEAKdetect" | "HIRes" | "AVErage" | "ENVelope" | "WFMDB"
    virtual void setAcquisitionMode(const QString &mode) { unsupported("setAcquisitionMode"); }
    virtual QString getAcquisitionMode() { return unavailable("getAcquisitionMode", QString{}); }

    // Waveform File Capture (Polymorphism Support)
    virtual QByteArray captureWaveformFile(int channel, const QString &format = "CSV",
                                           const QString &scopePath = "E:\\temp\\wave.csv",
                                           int startPoint = -1, int stopPoint = -1)
    {
        Q_UNUSED(channel);
        Q_UNUSED(format);
        Q_UNUSED(scopePath);
        Q_UNUSED(startPoint);
        Q_UNUSED(stopPoint);
        qWarning() << "[Oscilloscope] captureWaveformFile() not implemented for model:" << model();
        return QByteArray();
    }

    virtual bool captureWaveformFileToHost(int channel, const QString &hostFilePath,
                                           const QString &format = "CSV",
                                           const QString &scopePath = "E:\\temp\\wave.csv",
                                           int startPoint = -1, int stopPoint = -1)
    {
        Q_UNUSED(channel);
        Q_UNUSED(hostFilePath);
        Q_UNUSED(format);
        Q_UNUSED(scopePath);
        Q_UNUSED(startPoint);
        Q_UNUSED(stopPoint);
        qWarning() << "[Oscilloscope] captureWaveformFileToHost() not implemented for model:" << model();
        return false;
    }

  protected:
    bool unsupportedSetting(const char *operation, QString &error)
    {
        error = QString("Unsupported oscilloscope operation: %1").arg(operation);
        m_lastError = error;
        return false;
    }
    template <class Value> Value unavailable(const char *operation, Value invalid)
    {
        m_lastError = QString("Unsupported oscilloscope operation: %1").arg(operation);
        return invalid;
    }
    [[noreturn]] static void unsupported(const char *operation)
    {
        throw std::logic_error(std::string("Unsupported oscilloscope operation: ") + operation);
    }
    // 成員變數
    int m_currentChannel = 1;
};
