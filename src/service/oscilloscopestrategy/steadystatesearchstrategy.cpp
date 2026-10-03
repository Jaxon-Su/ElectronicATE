#include "steadystatesearchstrategy.h"
#include "iscopemeasurement.h"
#include "oscilloscopeacquisition.h"

#include <QElapsedTimer>
#include <QMap>
#include <QScopeGuard>

#include <algorithm>
#include <cmath>
#include <limits>

SteadyStateSearchStrategy::SteadyStateSearchStrategy(int timeoutMs, int phaseTimeoutMs, int maxIterations,
                                             const QString &target)
    : m_target(target.trimmed().toUpper()), m_timeoutMs(qMax(1, timeoutMs)),
      m_phaseTimeoutMs(qMax(1, phaseTimeoutMs)), m_maxIterations(qMax(1, maxIterations))
{
}

QString SteadyStateSearchStrategy::name() const { return "NormalTrigger(" + m_target + ")"; }

OscMeasureResult SteadyStateSearchStrategy::execute(IScopeMeasurement *scope, QAtomicInt &stopFlag)
{
    using OscilloscopeAcquisition::AcquisitionResult;
    const auto measurementOrNaN = [](double value) {
        return std::isfinite(value) && std::abs(value) < 1e30 ? value
                                                              : std::numeric_limits<double>::quiet_NaN();
    };

    // 1. 驗證輸入；success 預設 false，只有所選階段都完成才設為 true。
    OscMeasureResult result;
    if (m_target != "BOTH" && m_target != "MAX" && m_target != "MIN") {
        result.errorMessage = "Invalid measurement target";
        return result;
    }
    if (!scope) {
        result.errorMessage = "No oscilloscope available";
        return result;
    }

    // 所有 return／例外離開都嘗試 STOP；清理例外不可覆蓋原始錯誤。
    auto cleanup = qScopeGuard([scope] {
        try {
            scope->stop();
        } catch (...) {

            // 清理不能拋出第二個例外。
        }
    });
    QMap<int, OscChannelMeasure> channels;

    // 錯誤時仍回傳已取得的部分結果；尚未量測欄位保持 NaN（顯示 NA）。
    auto finish = [&]() {
        QString stopError;
        if (!scope->stopAcquisition(stopError)) {
            result.success = false;
            result.errorMessage +=
                (result.errorMessage.isEmpty() ? "" : "; ") + QString("Scope STOP: ") + stopError;
        }
        result.channels.clear();
        for (auto value : channels) {
            if (m_target == "MAX")
                value.minVal = std::numeric_limits<double>::quiet_NaN();
            if (m_target == "MIN")
                value.maxVal = std::numeric_limits<double>::quiet_NaN();
            result.channels.append(value);
        }
        return result;
    };
    if (stopFlag.loadAcquire()) {
        result.errorMessage = "Stopped by user";
        return finish();
    }

    // 2. 僅接受已啟用的類比觸發通道；避免無法更新門檻而持續等待。
    const int triggerChannel = scope->triggerChannel();
    // 每個已啟用的示波器通道，建立量測結果欄位。
    for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
        if (scope->isChannelEnabled(ch)) {
            OscChannelMeasure value;
            value.channel = ch;
            channels.insert(ch, value);
        }
    }
    if (!channels.contains(triggerChannel)) {
        result.errorMessage = "Trigger source must be an enabled analog channel";
        return finish();
    }
    OscilloscopeAcquisition::VerticalScaleState vertical;
    vertical.captureRecord = m_captureObserver;
    // AUTO obtains a fresh steady reference; V/div expands only on clipping, preserving timebase.
    const auto reference =
        OscilloscopeAcquisition::captureAutoReference(scope, stopFlag, m_phaseTimeoutMs, &vertical);
    if (!reference.success) {
        result.errorMessage = reference.errorMessage;
        return finish();
    }
    for (const auto &value : reference.channels)
        channels[value.channel] = value;
    const double initialMax = channels[triggerChannel].maxVal;
    const double initialMin = channels[triggerChannel].minVal;

    // 每個階段都有獨立的總時間與迭代上限，避免無限迴圈。
    for (bool maximum : {true, false}) {
        if ((maximum && m_target == "MIN") || (!maximum && m_target == "MAX"))
            continue;
        QElapsedTimer phase;
        phase.start();
        double level = maximum ? initialMax : initialMin;
        bool haveCapture = true; // The AUTO reference is valid measurement data for Static/Dynamic.
        bool finished = false;
        int scaleAdjustments = 0;
        if (!scope->selectEdgeTrigger(result.errorMessage) ||
            !scope->setEdgeSlope(maximum ? ScopeTriggerSlope::Rise : ScopeTriggerSlope::Fall,
                                 result.errorMessage) ||
            !scope->setTriggerMode(ScopeTriggerMode::Normal, result.errorMessage))
            return finish();
        for (int iteration = 0; iteration < m_maxIterations; ++iteration) {
            if (stopFlag.loadAcquire()) {
                result.errorMessage = "Stopped by user";
                return finish();
            }
            if (phase.elapsed() >= m_phaseTimeoutMs) {
                break;
            }

            // Read each round so clipping recovery also updates the step (V or A).
            const double scale = measurementOrNaN(scope->getChannelScale(triggerChannel));
            const double step = scale * 0.04;
            if (stopFlag.loadAcquire()) {
                result.errorMessage = "Stopped by user";
                return finish();
            }
            if (!std::isfinite(step) || step <= 0) {
                result.errorMessage = "Invalid trigger channel scale for search step";
                return finish();
            }
            if (phase.elapsed() >= m_phaseTimeoutMs)
                break;

            if (maximum && iteration == 0)
                level = std::max(level, step);

            // 每輪重新啟動 Single；啟動失敗不能讀取上一筆波形當成新資料。
            if (!scope->setChannelTriggerLevel(triggerChannel, level, result.errorMessage))
                return finish();
            if (!scope->beginSingleAcquisition()) {
                result.errorMessage = "Failed to start single acquisition";
                return finish();
            }

            // 每輪等待時間不能超過該階段剩餘時間。
            const int remaining = int(m_phaseTimeoutMs - phase.elapsed());
            const auto status = OscilloscopeAcquisition::waitForCapture(
                scope, stopFlag, qMin(m_timeoutMs, qMax(0, remaining)), qMax(0, remaining));

            // 停止與通訊錯誤直接判失敗，不能當成搜尋完成。
            if (status == AcquisitionResult::Cancelled) {
                result.errorMessage = "Stopped by user";
                return finish();
            }
            if (status == AcquisitionResult::CommError) {
                result.errorMessage = "Acquisition state communication error";
                return finish();
            }
            if (status == AcquisitionResult::CaptureTimeout) {
                result.errorMessage = "Acquisition incomplete or scope not ready (not a trigger MISS)";
                return finish();
            }
            if (phase.elapsed() >= m_phaseTimeoutMs) {
                break;
            }

            // 期限內未完成擷取：首次沒有資料就回 NA；已有資料則保留最後結果。

            // 不重新讀波形，也不宣稱找到訊號的絕對極值。
            if (status == AcquisitionResult::NoTrigger) {
                QString stopError;
                if (!scope->stopAcquisition(stopError)) {
                    result.cleanupFailed = true;
                    result.errorMessage = "Scope STOP: " + stopError;
                    return finish();
                }
                if (!haveCapture) {
                    result.errorMessage = "No completed acquisition: NA";
                    return finish();
                }
                finished = true;
                break;
            }

            // Scale changes require a fresh record; clipped data is never retained.
            try {
                if (OscilloscopeAcquisition::expandClippedChannels(scope, stopFlag, scaleAdjustments,
                                                                   &vertical)) {
                    continue;
                }
            } catch (const std::exception &ex) {
                result.errorMessage = QString::fromUtf8(ex.what());
                return finish();
            }
            // 4. 只有 Completed 才讀本輪峰值與 RMS／Mean。
            double peak = std::numeric_limits<double>::quiet_NaN();
            for (auto it = channels.begin(); it != channels.end(); ++it) {
                if (stopFlag.loadAcquire()) {
                    result.errorMessage = "Stopped by user";
                    return finish();
                }
                auto readPeak = [&](ScopeMeasurement type) {
                    const double value = measurementOrNaN(scope->readMeasurement(it.key(), type));
                    return value;
                };
                // BOTH retains both extrema from every record; slope only selects the search threshold.
                const double high = (m_target == "BOTH" || maximum)
                                        ? readPeak(ScopeMeasurement::Maximum)
                                        : std::numeric_limits<double>::quiet_NaN();
                if (stopFlag.loadAcquire()) {
                    result.errorMessage = "Stopped by user";
                    return finish();
                }
                const double low = (m_target == "BOTH" || !maximum)
                                       ? readPeak(ScopeMeasurement::Minimum)
                                       : std::numeric_limits<double>::quiet_NaN();
                if (stopFlag.loadAcquire()) {
                    result.errorMessage = "Stopped by user";
                    return finish();
                }
                if (std::isfinite(high))
                    it->maxVal = std::isfinite(it->maxVal) ? std::max(it->maxVal, high) : high;
                if (std::isfinite(low))
                    it->minVal = std::isfinite(it->minVal) ? std::min(it->minVal, low) : low;
                if (it.key() == triggerChannel)
                    peak = maximum ? high : low;
                it->rms = measurementOrNaN(scope->readMeasurement(it.key(), ScopeMeasurement::Rms));
                if (stopFlag.loadAcquire()) {
                    result.errorMessage = "Stopped by user";
                    return finish();
                }
                it->mean = measurementOrNaN(scope->readMeasurement(it.key(), ScopeMeasurement::Mean));
            }

            // 觸發通道失敗無法更新 Level，必須中止；其他通道失敗保留 NA。
            if (!std::isfinite(peak)) {
                result.errorMessage = "Trigger measurement: NA";
                return finish();
            }
            if (phase.elapsed() >= m_phaseTimeoutMs) {
                break;
            }

            // 5. 門檻必須持續向外推進；只有 NoTrigger 才算搜尋結束。
            haveCapture = true;
            const double nextLevel =
                maximum ? std::max(level + step, peak + step) : std::min(level - step, peak - step);
            if (!std::isfinite(nextLevel) || nextLevel == level) {
                result.errorMessage = "Search threshold cannot advance";
                return finish();
            }
            level = nextLevel;
        }

        // 用盡總時間／迭代次數屬於失敗，不作為收斂成功。
        if (!finished) {
            result.errorMessage = "Acquisition phase time/iteration limit reached";
            return finish();
        }
    }
    if (stopFlag.loadAcquire()) {
        result.errorMessage = "Stopped by user";
        return finish();
    }

    // 6. 所選搜尋階段均正常結束，且未取消，才回報成功。
    result.success = true;
    return finish();
}
