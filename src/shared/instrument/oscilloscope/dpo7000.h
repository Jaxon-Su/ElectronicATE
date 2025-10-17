#pragma once
#include "Oscilloscope.h"

class DPO7000 : public Oscilloscope {
public:
    explicit DPO7000(ICommunication* comm = nullptr) : Oscilloscope(comm) {}

    ~DPO7000() override;

    // 實作基本資訊
    QString model() const override { return "DPO7000"; }
    QString vendor() const override { return "Tektronix"; }

    // === 必須實作的函數 ===
    QByteArray captureScreenshot(const QString& format) override;
    void setTimebase(double timePerDiv) override;
    void setChannelScale(int channel, double voltsPerDiv) override;
    void setTriggerLevel(double level) override;

    // === 選擇性實作的函數 ===
    void setChannelPosition(int channel, double position) override;
    void setHorizontalPosition(double position) override;
    double getChannelPosition(int channel) override;
    double getHorizontalPosition() override;
    void setChannelCoupling(int channel, const QString& coupling) override;
    void setTriggerSource(const QString& source) override;
    void setTriggerType(const QString& type) override;
    void setTriggerSlope(const QString& slope) override;
    void autoSetup() override;
    void run() override;
    void stop() override;
    void single() override;
    void normal() override;
    void automode() override;
    void force() override;
    void continuous() override; //連續模式
    void enableChannel(int channel, bool enabled) override;

    // === 半自動觸發核心查詢方法 ===
    QString getTriggerSource() override;
    QString getTriggerType() override;
    QString getTriggerSlope() override;
    double getTriggerLevel() override;
    QString getTriggerMode() override;
    double getChannelScale(int channel) override;
    bool isChannelEnabled(int channel) override;
    double getTimebase() override;
    bool isRunning() override;  // 查詢是否運行

    // === 狀態查詢方法（半自動觸發必需） ===
    QString getAcquisitionState() override;
    QString getTriggerState() override;
    QString getStopAfterMode()override;

    // === 系統操作方法 ===
    bool waitForOperationComplete(int timeoutMs = 5000) override;
    QString getSystemError() override;
    void clearErrors() override;
    double measureSignalPeak(int channel, const QString& measureType = "MAXimum") override;

    // === DPO7000 專用方法 ===
    // 抓取完整波形檔（CSV/WFM/ISF）為位元流
    QByteArray captureWaveformFile(int channel,
                                   const QString& format = "CSV",
                                   const QString& scopePath = "E:\\temp\\wave.csv",
                                   int startPoint = -1, int stopPoint = -1);

    // 直接存檔到主機檔案系統
    bool captureWaveformFileToHost(int channel,
                                   const QString& hostFilePath,
                                   const QString& format = "CSV",
                                   const QString& scopePath = "E:\\temp\\wave.csv",
                                   int startPoint = -1, int stopPoint = -1);


private:
    QString m_triggerType = "EDGE";
};
