#pragma once
#include "oscilloscope.h"

class DPO7000 : public Oscilloscope {
public:
    explicit DPO7000(ICommunication* comm = nullptr) : Oscilloscope(comm) {}

    ~DPO7000() override;

    // 實作基本資訊
    QString model() const override { return "DPO7000"; }
    QString vendor() const override { return "Tektronix"; }

    // 設定 PC 端掛載示波器資料夾的網路磁碟機路徑 (預設為 Z:/)
    void setMappedDrivePath(const QString& path) { m_mappedDrivePath = path; }
    QString getMappedDrivePath() const { return m_mappedDrivePath; }

    // === 必須實作的函數 ===
    QByteArray captureScreenshot(const QString& format,
                                 const QString& savePath = QString()) override;
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
    bool isMathChannelEnabled(int channel) override;
    double getTimebase() override;
    bool isRunning() override;  // 查詢是否運行
    int getTotalChannel() override;

    // === 擷取模式（Peak Detect / Sample / HIRes / Average / Envelope）===
    void setAcquisitionMode(const QString& mode) override;
    QString getAcquisitionMode() override;

    // === 狀態查詢方法（半自動觸發必需） ===
    QString getAcquisitionState() override;
    QString getTriggerState() override;
    QString getStopAfterMode() override;

    // === 系統操作方法 ===
    bool waitForOperationComplete(int timeoutMs = 5000) override;
    QString getSystemError() override;
    void clearErrors() override;
    double measureSignalPeak(int channel, const QString& measureType = "MAXimum") override;

    // === DPO7000 專用方法 ===
    // 抓取完整波形檔（CSV/WFM/ISF）為位元流
    QByteArray captureWaveformFile(int channel,
                                   const QString& format = "CSV",
                                   const QString& scopePath = "C:\\TekScope\\Waveforms\\wave.csv",
                                   int startPoint = -1,
                                   int stopPoint = -1) override;

    bool captureWaveformFileToHost(int channel,
                                   const QString& hostFilePath,
                                   const QString& format = "CSV",
                                   const QString& scopePath = "C:\\TekScope\\Waveforms\\wave.csv",
                                   int startPoint = -1,
                                   int stopPoint = -1) override;

private:
    // 共用查詢輔助：發送 SCPI query，回傳「1」或「ON」則視為啟用
    // logTag 範例：「[DPO7000] isChannelEnabled failed for CH3:」
    bool querySelectState(const QString& query, const QString& logTag);

    QString m_triggerType = "EDGE";
    int m_totalChannel = 4;

    // 預設網路磁碟機掛載路徑，對應示波器的 C:\TekScope\Waveforms 資料夾
    QString m_mappedDrivePath = "Z:/";
};
