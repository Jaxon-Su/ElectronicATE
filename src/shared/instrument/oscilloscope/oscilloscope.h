#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#pragma once
#include "../InstrumentWithCommBase.h"
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
class Oscilloscope : public InstrumentWithCommBase {

public:
    explicit Oscilloscope(ICommunication* comm = nullptr)
        : InstrumentWithCommBase(comm) {}

    virtual ~Oscilloscope() = default;

    // === 必須實作的純虛擬函數 ===

    // 基本設定功能

    // === 預設實作（可被override） ===

    // 基本擷取功能
    virtual QByteArray captureScreenshot(const QString& format) { return QByteArray(); }
    virtual void setTimebase(double timePerDiv) {}
    virtual void setChannelScale(int channel, double Div) {}
    virtual void setTriggerLevel(double level) {}

    // 通道設定
    virtual void setChannelPosition(int channel, double position) {}
    virtual void setChannelCoupling(int channel, const QString& coupling) {}
    virtual void setChannelBandwidth(int channel, double bandwidth) {}
    virtual void setProbeRatio(int channel, double ratio) {}
    virtual void enableChannel(int channel, bool enabled) {}

    // 水平軸設定
    virtual void setHorizontalPosition(double position) {}

    virtual void setRecordLength(int length) {}
    virtual void setSampleRate(double rate) {}

    // 觸發設定
    virtual void setTriggerSource(const QString& source) {}
    virtual void setTriggerType(const QString& type) {}
    virtual void setTriggerSlope(const QString& slope) {}
    virtual void setTriggerCoupling(const QString& coupling) {}

    // 控制功能
    virtual void autoSetup() {}
    virtual void automode() {}
    virtual void run() {}
    virtual void stop() {}
    virtual void single() {}
    virtual void normal() {}
    virtual void force() {}
    virtual void continuous() {}

    // 半自動测量功能暫時加入
    virtual double measureSignalPeak(int channel, const QString& measureType = "MAXimum") { return 0.0; }
    virtual bool waitForOperationComplete(int timeoutMs = 5000) { return true; }
    virtual QString getSystemError() { return ""; }
    virtual void clearErrors() {}

    // 相關查詢
    virtual QString getTriggerSlope() { return ""; }
    virtual QString getTriggerSource() { return ""; }
    virtual QString getTriggerType() { return ""; }
    virtual double getTriggerLevel() { return 0.0; }
    virtual QString getTriggerMode() { return ""; }
    virtual double getChannelPosition(int channel) {return 0.0;}
    virtual double getHorizontalPosition() {return 0.0;}
    virtual double getChannelScale(int channel) { return 0.0; }
    virtual bool isChannelEnabled(int channel) { return false; }
    virtual double getTimebase() { return 0.0; }
    virtual QString getAcquisitionState() { return ""; }
    virtual QString getTriggerState() { return ""; }
    virtual QString getStopAfterMode() { return ""; }
    virtual bool isRunning(){ return false; }




protected:
    // 成員變數
    int m_currentChannel = 1;

};

#endif // OSCILLOSCOPE_H
