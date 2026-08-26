#pragma once
#include "oscilloscope.h"

// ─────────────────────────────────────────────────────────────────────────────
//  MSOSeries456
//  支援機型: MSO44(B) / MSO46(B) / MSO54(B) / MSO56(B) / MSO58(B) /
//            MSO58LP / MSO64(B) / MSO66B / MSO68B / LPD64
//
//  與 DPO7000 的主要 SCPI 差異：
//  ┌───────────────────┬──────────────────────────┬───────────────────────────┐
//  │ 功能              │ DPO7000                  │ MSO 4/5/6 Series          │
//  ├───────────────────┼──────────────────────────┼───────────────────────────┤
//  │ 通道開關          │ SELect:CH<x> {ON|OFF}    │ DISplay:GLObal:CH<x>:STATE│
//  │ Math 通道開關     │ SELect:MATH<x>?          │ DISplay:GLObal:MATH<x>    │
//  │                   │                          │ :STATE                    │
//  │ 觸發電平          │ TRIGger:A:LEVel <V>      │ TRIGger:A:LEVel:CH<x> <V>│
//  │                   │ (全域)                   │ (per-channel)             │
//  │ 垂直位移          │ CH<x>:POSITION (div)     │ CH<x>:OFFSet (V)          │
//  │ 截圖              │ EXPort STARt             │ SAVe:IMAGe "<path>"       │
//  │ 波形存檔          │ SAVe:WAVEform:FILEFormat │ 副檔名決定格式            │
//  │                   │ SPREADSHEETCSV 後再存    │ (.csv / .wfm / .mat)      │
//  └───────────────────┴──────────────────────────┴───────────────────────────┘
//
//  檔案傳輸策略（與 DPO7000 相同）：
//    示波器先將檔案存至自身硬碟 (m_scopeSaveDir，例如 C:/Temp)，
//    PC 端透過 SMB 網路磁碟機 (m_mappedDrivePath，例如 Z:/) 直接讀取，
//    避免 VISA / VXI-11 在大檔案傳輸時的效能瓶頸。
// ─────────────────────────────────────────────────────────────────────────────

class MSOSeries456 : public Oscilloscope {
public:
    explicit MSOSeries456(ICommunication* comm = nullptr) : Oscilloscope(comm) {}
    ~MSOSeries456() override;

    // 基本資訊
    QString model()  const override { return "MSOSeries456"; }
    QString vendor() const override { return "Tektronix"; }

    // ── 檔案傳輸路徑設定 ─────────────────────────────────────────────────────
    // PC 端掛載的網路磁碟機路徑，須對應示波器的 m_scopeSaveDir
    //   e.g. 示波器 C:/Temp  ↔  PC 端 Z:/
    void    setMappedDrivePath(const QString& path) { m_mappedDrivePath = path; }
    QString getMappedDrivePath() const              { return m_mappedDrivePath; }

    // 示波器端的暫存儲存資料夾（絕對路徑，正斜線）
    void    setScopeSaveDir(const QString& dir) { m_scopeSaveDir = dir; }
    QString getScopeSaveDir() const             { return m_scopeSaveDir; }

    // ── 通道數設定（依機型不同：4 / 6 / 8）────────────────────────────────
    void setTotalChannel(int n) { m_totalChannel = n; }

    // ═══ 必須實作的函數 ═══════════════════════════════════════════════════════
    QByteArray captureScreenshot(const QString& format,
                                 const QString& savePath = QString()) override;
    void setTimebase(double timePerDiv) override;
    void setChannelScale(int channel, double voltsPerDiv) override;
    void setTriggerLevel(double level) override;

    // ═══ 選擇性實作的函數 ══════════════════════════════════════════════════════
    // 注意：MSO 4/5/6 的垂直位移使用 CH<x>:OFFSet（單位：V），
    //       與 DPO7000 的 CH<x>:POSITION（單位：div）不同
    void   setChannelPosition(int channel, double offsetVolts) override;
    void   setHorizontalPosition(double position) override;
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

    // ═══ 半自動觸發核心查詢方法 ═══════════════════════════════════════════════
    QString getTriggerSource() override;
    QString getTriggerType() override;
    QString getTriggerSlope() override;
    double  getTriggerLevel() override;
    QString getTriggerMode() override;
    double  getChannelScale(int channel) override;
    bool    isChannelEnabled(int channel) override;
    bool    isMathChannelEnabled(int channel) override;
    double  getTimebase() override;
    bool    isRunning() override;
    int     getTotalChannel() override;

    // ═══ 擷取模式（SAMple / PEAKdetect / HIRes / AVErage / ENVelope）════════
    // 注意：MSO 4/5/6 不支援 DPO7000 的 WFMDB 模式
    void    setAcquisitionMode(const QString& mode) override;
    QString getAcquisitionMode() override;

    // ═══ 狀態查詢方法（半自動觸發必需） ══════════════════════════════════════
    QString getAcquisitionState() override;
    QString getTriggerState() override;
    QString getStopAfterMode() override;

    // ═══ 系統操作方法 ════════════════════════════════════════════════════════
    bool    waitForOperationComplete(int timeoutMs = 5000) override;
    QString getSystemError() override;
    void    clearErrors() override;
    double  measureSignalPeak(int channel,
                             const QString& measureType = "MAXimum") override;
    bool    isClipping(int channel) override;

    // ═══ 波形檔案擷取（透過 SMB 網路磁碟機）═════════════════════════════════
    // format: "CSV"（.csv）/ "WFM"（.wfm）/ "MAT"（.mat）
    // scopePath 在 MSO 4/5/6 驅動中不使用（路徑由 m_scopeSaveDir 自動產生），
    // 保留此參數僅為符合基類介面。
    QByteArray captureWaveformFile(int channel,
                                   const QString& format    = "CSV",
                                   const QString& scopePath = "C:/wave.csv",
                                   int startPoint = -1,
                                   int stopPoint  = -1) override;

    bool captureWaveformFileToHost(int channel,
                                   const QString& hostFilePath,
                                   const QString& format    = "CSV",
                                   const QString& scopePath = "C:/wave.csv",
                                   int startPoint = -1,
                                   int stopPoint  = -1) override;

private:
    // 共用查詢輔助：查詢 DISplay:GLObal:<source>:STATE，回傳是否為 ON
    bool queryGlobalDisplayState(const QString& source, const QString& logTag);

    // 從觸發來源字串中萃取通道號 (e.g. "CH2" -> 2，解析失敗 -> 1)
    int parseTriggerChannel(const QString& source) const;

    // 繞過 FILESystem:READFile 的 VXI-11 buffer 限制：
    // 直接用 CURVe?（binary block）取波形資料，PC 端計算電壓後組 CSV。
    // Header 格式對齊 Tektronix 原生 CSV，但非完全相同（見實作說明）。
    // 25000 點 × 2 bytes = 50KB，單次 VXI-11 chunk 即可完成，無截斷問題。
    bool buildCsvViaCurve(int channel, QByteArray& csvOut);

    // 原生 CSV：SAVe:WAVEform → FILESystem:READFile（VXI-11，無需 SMB）
    // 讀取前執行 deviceClear 清除 output buffer，避免前次殘留資料干擾。
    bool captureNativeCsv(int channel, QByteArray& csvOut);

    // ── 內部狀態 ─────────────────────────────────────────────────────────────
    QString m_triggerType   = "EDGE";
    QString m_triggerSource = "CH1";   // 追蹤目前 EDGE trigger source（供 per-channel level 使用）

    int m_totalChannel = 4;            // 預設 4 通道；MSO46/56/66→6，MSO58/68→8

    // ── 檔案傳輸路徑 ─────────────────────────────────────────────────────────
    QString m_mappedDrivePath = "Z:/"; // PC 端掛載路徑
    QString m_scopeSaveDir    = "C:"; // 示波器端儲存根目錄（標準版 Linux MSO 只有 C: 根目錄；Windows 10 選配版可改為 "C:/Temp"）
};
