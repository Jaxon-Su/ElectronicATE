#pragma once
#include <QString>
#include <QVector>
#include <QAtomicInt>

class Oscilloscope;

// ── 單通道量測結果 ────────────────────────────────────
struct OscChannelMeasure {
    int    channel = 1;
    double maxVal  = 0.0;
    double minVal  = 0.0;
    double rms     = 0.0;
    double mean    = 0.0;
};

// ── 一次策略執行的完整結果 ────────────────────────────
struct OscMeasureResult {
    bool                       success = false;
    QVector<OscChannelMeasure> channels;
    QString                    errorMessage;
};

// ══════════════════════════════════════════════════════
//  IOscilloscopeMeasureStrategy — 示波器量測策略介面
//
//  職責：
//    封裝「如何觸發示波器 + 讀取量測值」的策略
//    不涉及儀器初始化（Write Oscilloscope 負責）
//
//  使用方式：
//    Page5TestWorker 持有一個 non-owning pointer，
//    由 Page5ViewModel 建立並注入。
//    execute() 在 worker thread 上呼叫。
//
//  新增策略：
//    繼承本介面，實作 execute() 與 name()，
//    於 Page5ViewModel 選擇注入哪個實作即可。
// ══════════════════════════════════════════════════════
class IOscilloscopeMeasureStrategy
{
public:
    virtual ~IOscilloscopeMeasureStrategy() = default;

    // 在 worker thread 執行觸發 + 量測；stopFlag 供中途中斷
    virtual OscMeasureResult execute(Oscilloscope* scope,
                                     QAtomicInt&   stopFlag) = 0;

    // 策略名稱（供 log 輸出使用）
    virtual QString name() const = 0;
};
