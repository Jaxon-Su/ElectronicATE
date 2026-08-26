#pragma once
#include "oscilloscope.h"

// ─────────────────────────────────────────────────────────────────────────────
// DPO4000 / MSO4000 系列示波器驅動
//
// 與 DPO7000 的主要 SCPI 差異：
//   1. 截圖：EXPort  →  SAVe:IMAGe (格式先用 SAVe:IMAGe:FILEFormat 設定)
//   2. 擷取模式：移除 WFMDB（4000 系列不支援）
//   3. 觸發位準：TRIGger:A:LEVel 為通用命令，可搭配
//                TRIGger:A:LEVel:CH<x> 對各通道個別設定
//   4. 示波器端預設存檔路徑改為 E:\（USB 隨身碟）
//   5. 數學通道查詢：SELect:MATH（與 DPO7000 相同）
// ─────────────────────────────────────────────────────────────────────────────

class DPO4000 : public Oscilloscope {
public:
    explicit DPO4000(ICommunication* comm = nullptr) : Oscilloscope(comm) {}

    ~DPO4000() override;

    // 基本資訊
    QString model()  const override { return "DPO4000"; }
    QString vendor() const override { return "Tektronix"; }

    // 設定 PC 端掛載示波器資料夾的網路磁碟機路徑（預設 Z:/）
    void    setMappedDrivePath(const QString& path) { m_mappedDrivePath = path; }
    QString getMappedDrivePath() const              { return m_mappedDrivePath; }

    // ── 必須實作的函數 ────────────────────────────────────────────────────────
    QByteArray captureScreenshot(const QString& format,
                                 const QString& savePath = QString()) override;
    void setTimebase(double timePerDiv) override;
    void setChannelScale(int channel, double voltsPerDiv) override;
    void setTriggerLevel(double level) override;

    // ── 選擇性實作的函數 ──────────────────────────────────────────────────────
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
    void continuous() override;
    void enableChannel(int channel, bool enabled) override;

    // ── 半自動觸發核心查詢方法 ───────────────────────────────────────────────
    QString getTriggerSource() override;
    QString getTriggerType()   override;
    QString getTriggerSlope()  override;
    double  getTriggerLevel()  override;
    QString getTriggerMode()   override;
    double  getChannelScale(int channel) override;
    bool    isChannelEnabled(int channel) override;
    bool    isMathChannelEnabled(int channel) override;
    double  getTimebase() override;
    bool    isRunning()   override;
    int     getTotalChannel() override;

    // ── 擷取模式（4000 系列：SAMple / PEAKdetect / HIRes / AVErage / ENVelope）
    void    setAcquisitionMode(const QString& mode) override;
    QString getAcquisitionMode() override;

    // ── 狀態查詢（半自動觸發必需）────────────────────────────────────────────
    QString getAcquisitionState() override;
    QString getTriggerState()     override;
    QString getStopAfterMode()    override;

    // ── 系統操作方法 ─────────────────────────────────────────────────────────
    bool    waitForOperationComplete(int timeoutMs = 5000) override;
    QString getSystemError() override;
    void    clearErrors()    override;
    double  measureSignalPeak(int channel,
                             const QString& measureType = "MAXimum") override;

    // ── DPO4000 專用：逐通道觸發位準設定 ─────────────────────────────────────
    // DPO4000 支援對每個通道各自設定觸發位準
    // setTriggerLevel() 呼叫的是通用版本（TRIGger:A:LEVel）
    void setTriggerLevelByChannel(int channel, double level);

    // ── 波形檔擷取（CSV / WFM）────────────────────────────────────────────────
    QByteArray captureWaveformFile(int channel,
                                   const QString& format   = "CSV",
                                   const QString& scopePath = "E:\\wave.csv",
                                   int startPoint = -1,
                                   int stopPoint  = -1) override;

    bool captureWaveformFileToHost(int channel,
                                   const QString& hostFilePath,
                                   const QString& format    = "CSV",
                                   const QString& scopePath = "E:\\wave.csv",
                                   int startPoint = -1,
                                   int stopPoint  = -1) override;

private:
    // 共用查詢輔助：回傳「1」或「ON」視為啟用
    bool querySelectState(const QString& query, const QString& logTag);

    QString m_triggerType   = "EDGE";
    int     m_totalChannel  = 4;

    // PC 端網路磁碟機掛載路徑，對應示波器的 E:\ (USB 隨身碟)
    QString m_mappedDrivePath = "Z:/";
};
