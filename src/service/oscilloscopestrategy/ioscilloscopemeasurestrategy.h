#pragma once
#include <QAtomicInt>
#include <functional>
#include <utility>
#include "oscmeasurementresult.h"
#include "osccaptureobserver.h"

#include "iscopemeasurement.h"

// Worker 建立並持有策略，於背景執行 execute；初始化由 Write Oscilloscope 負責。
// 策略由 Factory 建立；Turn on 的額外設定透過 context 傳入。
class IOscilloscopeMeasureStrategy
{
  public:
    void setCaptureObserver(OscCaptureObserver observer) { m_captureObserver = std::move(observer); }
    void setLogObserver(std::function<void(const QString &)> observer) { m_logObserver = std::move(observer); }
    virtual ~IOscilloscopeMeasureStrategy() = default;

    // 在 worker thread 執行觸發 + 量測；stopFlag 供中途中斷
    virtual OscMeasureResult execute(IScopeMeasurement *scope, QAtomicInt &stopFlag) = 0;

    // 策略名稱（供 log 輸出使用）
    virtual QString name() const = 0;

  protected:
    OscCaptureObserver m_captureObserver;
    std::function<void(const QString &)> m_logObserver;
};
