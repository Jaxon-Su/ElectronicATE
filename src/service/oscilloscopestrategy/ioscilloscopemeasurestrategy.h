#pragma once
#include <QString>
#include <functional>
#include <limits>
#include <cmath>
#include <QVector>
#include <QAtomicInt>

#include "iscopemeasurement.h"

inline QString formatOscMeasurement(double value)
{
    return std::isfinite(value) ? QString::number(value, 'g', 8) : QStringLiteral("NA");
}

// 單通道量測結果
struct OscChannelMeasure {
    int channel = 1;
    double maxVal = std::numeric_limits<double>::quiet_NaN();
    double minVal = std::numeric_limits<double>::quiet_NaN();
    double rms = std::numeric_limits<double>::quiet_NaN();
    double mean = std::numeric_limits<double>::quiet_NaN();
};

using OscCaptureObserver = std::function<void(const QVector<OscChannelMeasure> &)>;

// 一次策略執行的完整結果
struct OscMeasureResult {
    bool success = false;
    bool cleanupFailed = false;
    QVector<OscChannelMeasure> channels;
    QString errorMessage;
};

// Worker 建立並持有策略，於背景執行 execute；初始化由 Write Oscilloscope 負責。
// 策略由 Factory 建立；Turn on 的額外設定透過 context 傳入。
class IOscilloscopeMeasureStrategy
{
  public:
    void setCaptureObserver(OscCaptureObserver observer) { m_captureObserver = std::move(observer); }
    virtual ~IOscilloscopeMeasureStrategy() = default;

    // 在 worker thread 執行觸發 + 量測；stopFlag 供中途中斷
    virtual OscMeasureResult execute(IScopeMeasurement *scope, QAtomicInt &stopFlag) = 0;

    // 策略名稱（供 log 輸出使用）
    virtual QString name() const = 0;

  protected:
    OscCaptureObserver m_captureObserver;
};
