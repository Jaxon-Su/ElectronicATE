#include "steadysearch.h"

#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <limits>

namespace SteadyDetail
{
namespace
{
double measurementOrNaN(double value)
{
    return std::isfinite(value) && std::abs(value) < 1e30
               ? value : std::numeric_limits<double>::quiet_NaN();
}
}

Search::Search(IScopeMeasurement *instrument, QAtomicInt &stop, const QString &target,
               int timeoutMs, int phaseTimeoutMs, int maxIterations, const OscCaptureObserver &capture,
               const VerticalScaleOptions &options)
    : scope(instrument), stopFlag(stop), captureObserver(capture), m_target(target), m_timeoutMs(timeoutMs),
      m_phaseTimeoutMs(phaseTimeoutMs), m_maxIterations(maxIterations)
{
    vertical.options = options;
}

OscMeasureResult Search::execute()
{
    if (!initializeChannels() || !captureReference())
        return finish();
    for (bool maximum : {true, false}) {
        if ((maximum && m_target == "MIN") || (!maximum && m_target == "MAX"))
            continue;
        if (!searchDirection(maximum))
            return finish();
    }
    if (stopFlag.loadAcquire())
        result.errorMessage = "Stopped by user";
    else
        result.success = true;
    return finish();
}

bool Search::initializeChannels()
{
    if (stopFlag.loadAcquire()) {
        result.errorMessage = "Stopped by user";
        return false;
    }

    // 僅接受已啟用的類比觸發通道；避免無法更新門檻而持續等待。
    triggerChannel = scope->triggerChannel();
    // 每個已啟用的示波器通道，建立量測結果欄位。
    try {
        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            if (stopFlag.loadAcquire()) {
                result.errorMessage = "Stopped by user";
                return false;
            }
            if (OscilloscopeAcquisition::channelEnabledChecked(scope, ch)) {
                OscChannelMeasure value;
                value.channel = ch;
                channels.insert(ch, value);
            }
        }
    } catch (const std::exception &ex) {
        result.errorMessage = QString::fromUtf8(ex.what());
        return false;
    } catch (...) {
        result.errorMessage = "Unexpected channel state query error";
        return false;
    }
    if (!channels.contains(triggerChannel)) {
        result.errorMessage = "Trigger source must be an enabled analog channel";
        return false;
    }
    return true;
}

bool Search::captureReference()
{
    vertical.captureRecord = captureObserver;
    // AUTO obtains a fresh steady reference; V/div expands for clipping or optional margin, preserving timebase.
    const auto reference =
        OscilloscopeAcquisition::captureAutoReference(scope, stopFlag, m_phaseTimeoutMs, &vertical);
    if (!reference.success) {
        result.errorMessage = reference.errorMessage;
        return false;
    }
    for (const auto &value : reference.channels)
        channels[value.channel] = value;
    initialMax = channels[triggerChannel].maxVal;
    initialMin = channels[triggerChannel].minVal;

    return true;
}

bool Search::searchDirection(bool maximum)
{
    using OscilloscopeAcquisition::AcquisitionResult;
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
        return false;
    for (int iteration = 0; iteration < m_maxIterations; ++iteration) {
        if (stopFlag.loadAcquire()) {
            result.errorMessage = "Stopped by user";
            return false;
        }
        if (phase.elapsed() >= m_phaseTimeoutMs) {
            break;
        }

        // Read each round so clipping recovery also updates the step (V or A).
        const double scale = measurementOrNaN(scope->getChannelScale(triggerChannel));
        const double step = scale * 0.04;
        if (stopFlag.loadAcquire()) {
            result.errorMessage = "Stopped by user";
            return false;
        }
        if (!std::isfinite(step) || step <= 0) {
            result.errorMessage = "Invalid trigger channel scale for search step";
            return false;
        }
        if (phase.elapsed() >= m_phaseTimeoutMs)
            break;

        if (maximum && iteration == 0)
            level = std::max(level, step);

        // 每輪重新啟動 Single；啟動失敗不能讀取上一筆波形當成新資料。
        if (!scope->setChannelTriggerLevel(triggerChannel, level, result.errorMessage))
            return false;
        if (!scope->beginSingleAcquisition()) {
            result.errorMessage = "Failed to start single acquisition";
            return false;
        }

        // 每輪等待時間不能超過該階段剩餘時間。
        const int remaining = int(m_phaseTimeoutMs - phase.elapsed());
        const auto status = OscilloscopeAcquisition::waitForCapture(
            scope, stopFlag, qMin(m_timeoutMs, qMax(0, remaining)), qMax(0, remaining));

        // 停止與通訊錯誤直接判失敗，不能當成搜尋完成。
        if (status == AcquisitionResult::Cancelled) {
            result.errorMessage = "Stopped by user";
            return false;
        }
        if (status == AcquisitionResult::CommError) {
            result.errorMessage = "Acquisition state communication error";
            return false;
        }
        if (status == AcquisitionResult::CaptureTimeout) {
            result.errorMessage = "Acquisition incomplete or scope not ready (not a trigger MISS)";
            return false;
        }
        if (!OscilloscopeAcquisition::hasCompletedRecord(status) && phase.elapsed() >= m_phaseTimeoutMs) {
            break;
        }

        // 正常 MISS 才結束搜尋；AUTO 參考也屬於有效量測。
        // 不重新讀波形，也不宣稱找到訊號的絕對極值。
        if (status == AcquisitionResult::NoTrigger) {
            QString stopError;
            if (!scope->stopAcquisition(stopError)) {
                result.cleanupFailed = true;
                result.errorMessage = "Scope STOP: " + stopError;
                return false;
            }
            if (!haveCapture) {
                result.errorMessage = "No completed acquisition: NA";
                return false;
            }
            finished = true;
            break;
        }

        // Scale changes require a fresh record; clipped data is never retained.
        try {
            if (OscilloscopeAcquisition::expandVerticalRange(scope, stopFlag, scaleAdjustments,
                                                               &vertical, "Measurement",
                                                               status == AcquisitionResult::CompletedAfterTimeout,
                                                               QDeadlineTimer(qMax(0, m_phaseTimeoutMs - int(phase.elapsed()))))) {
                continue;
            }
        } catch (const std::exception &ex) {
            result.errorMessage = QString::fromUtf8(ex.what());
            return false;
        } catch (...) {
            result.errorMessage = "Unexpected acquisition/Capture error";
            return false;
        }
        double peak = std::numeric_limits<double>::quiet_NaN();
        if (!accumulateRecord(maximum, iteration + 1, level, peak))
            return false;

        // 通訊失敗已中止；合法 NA 沿用既有搜尋語意。
        if (!std::isfinite(peak)) {
            result.errorMessage = "Trigger measurement: NA";
            return false;
        }
        if (phase.elapsed() >= m_phaseTimeoutMs) {
            break;
        }

        // 門檻必須持續向外推進；只有 NoTrigger 才算搜尋結束。
        haveCapture = true;
        const double nextLevel =
            maximum ? std::max(level + step, peak + step) : std::min(level - step, peak - step);
        if (!std::isfinite(nextLevel) || nextLevel == level) {
            result.errorMessage = "Search threshold cannot advance";
            return false;
        }
        level = nextLevel;
    }

    // 用盡總時間／迭代次數屬於失敗，不作為收斂成功。
    if (!finished) {
        result.errorMessage = "Acquisition phase time/iteration limit reached";
        return false;
    }
    return true;
}

bool Search::accumulateRecord(bool maximum, int step, double level, double &peak)
{
    MeasurementTrial record{int(result.trials.size()) + 1,
                            maximum ? MeasurementDirection::Rise : MeasurementDirection::Fall,
                            step, 1, level, {}};
    try {
        for (auto it = channels.begin(); it != channels.end(); ++it) {
            if (stopFlag.loadAcquire()) {
                result.errorMessage = "Stopped by user";
                return false;
            }
            auto readPeak = [&](ScopeMeasurement type) {
                const double value = measurementOrNaN(
                    OscilloscopeAcquisition::readMeasurementChecked(scope, it.key(), type));
                return value;
            };
            // Direction controls the search, not which values belong to a record.
            const double high = readPeak(ScopeMeasurement::Maximum);
            if (stopFlag.loadAcquire()) {
                result.errorMessage = "Stopped by user";
                return false;
            }
            const double low = readPeak(ScopeMeasurement::Minimum);
            if (stopFlag.loadAcquire()) {
                result.errorMessage = "Stopped by user";
                return false;
            }
            if (std::isfinite(high))
                it->maxVal = std::isfinite(it->maxVal) ? std::max(it->maxVal, high) : high;
            if (std::isfinite(low))
                it->minVal = std::isfinite(it->minVal) ? std::min(it->minVal, low) : low;
            if (it.key() == triggerChannel)
                peak = maximum ? high : low;
            it->rms = readPeak(ScopeMeasurement::Rms);
            if (stopFlag.loadAcquire()) {
                result.errorMessage = "Stopped by user";
                return false;
            }
            it->mean = readPeak(ScopeMeasurement::Mean);
            record.channels.append({it.key(), {}, high, low, it->rms, it->mean});
        }
    } catch (const std::exception &ex) {
        result.errorMessage = QString::fromUtf8(ex.what());
        return false;
    } catch (...) {
        result.errorMessage = "Unexpected measurement query error";
        return false;
    }

    if (std::isfinite(peak))
        result.trials.append(record);
    return true;
}

OscMeasureResult Search::finish()
{
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
}
} // namespace SteadyDetail
