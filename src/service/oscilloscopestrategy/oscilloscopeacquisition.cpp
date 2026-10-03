#include "oscilloscopeacquisition.h"
#include "iscopemeasurement.h"
#include <QElapsedTimer>
#include <QThread>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace
{
double readReadyMeasurement(IScopeMeasurement *scope, QAtomicInt &stop, int channel, ScopeMeasurement type,
                            int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    double value = std::numeric_limits<double>::quiet_NaN();
    do {
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        value = scope->readMeasurement(channel, type);
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        if (std::isfinite(value) && std::abs(value) < 1e20)
            return value;
        if (!scope->lastError().isEmpty())
            throw std::runtime_error(
                ("Measurement communication error: " + scope->lastError()).toStdString());
        if (timer.elapsed() >= timeoutMs)
            break;
        QThread::msleep(20);
    } while (timer.elapsed() < timeoutMs);
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace

namespace OscilloscopeAcquisition
{

bool waitForReady(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs, QString &error)
{
    if (!scope) {
        error = "No oscilloscope";
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (stop.loadAcquire()) {
            error = "Stopped by user";
            return false;
        }
        const auto state = scope->triggerState();
        if (stop.loadAcquire()) {
            error = "Stopped by user";
            return false;
        }
        if (timer.elapsed() >= timeoutMs)
            break;
        if (state == ScopeTriggerState::Ready)
            return true;
        if (state != ScopeTriggerState::Arming) {
            error = "Scope not ready before event: " + scope->lastError();
            return false;
        }
        QThread::msleep(1);
    }
    error = "Scope trigger readiness timeout";
    return false;
}

AcquisitionResult waitForCapture(IScopeMeasurement *scope, QAtomicInt &stop, int timeoutMs,
                                 int completionTimeoutMs)
{
    if (stop.loadAcquire())
        return AcquisitionResult::Cancelled;
    if (!scope)
        return AcquisitionResult::CommError;

    QElapsedTimer timer;
    timer.start();
    bool triggered = false;
    auto triggerState = ScopeTriggerState::Invalid;
    int deadline = timeoutMs;

    while (timer.elapsed() < deadline) {
        if (stop.loadAcquire())
            return AcquisitionResult::Cancelled;

        const auto state = scope->acquisitionState();

        // 通訊期間也可能收到停止要求。
        if (stop.loadAcquire())
            return AcquisitionResult::Cancelled;

        switch (state) {
        case ScopeAcquisitionState::Invalid:
            return AcquisitionResult::CommError;

        case ScopeAcquisitionState::Completed: {
            if (timer.elapsed() >= deadline)
                return AcquisitionResult::CaptureTimeout;
            // STOP can precede display/measurement updates. Keep the record frozen before
            // clipping checks, automatic fitting, peak reads or the capture observer run.
            const int settleMs = qMax(0, scope->waveformSettleTimeMs());
            QElapsedTimer settling;
            settling.start();
            while (settling.elapsed() < settleMs) {
                if (stop.loadAcquire())
                    return AcquisitionResult::Cancelled;
                QThread::msleep(
                    static_cast<unsigned long>(std::clamp<qint64>(settleMs - settling.elapsed(), 0, 20)));
            }
            if (stop.loadAcquire())
                return AcquisitionResult::Cancelled;
            if (settleMs > 0) {
                const auto settledState = scope->acquisitionState();
                if (stop.loadAcquire())
                    return AcquisitionResult::Cancelled;
                if (settledState == ScopeAcquisitionState::Invalid)
                    return AcquisitionResult::CommError;
                if (settledState != ScopeAcquisitionState::Completed)
                    return AcquisitionResult::CaptureTimeout;
            }
            return AcquisitionResult::Completed;
        }

        case ScopeAcquisitionState::Running:
            break;
        }

        triggerState = scope->triggerState();
        if (stop.loadAcquire())
            return AcquisitionResult::Cancelled;
        if (triggerState == ScopeTriggerState::Invalid)
            return AcquisitionResult::CommError;
        if (triggerState == ScopeTriggerState::Triggered) {
            triggered = true;
            if (completionTimeoutMs > deadline)
                deadline = completionTimeoutMs;
        }

        const auto remaining = deadline - timer.elapsed();
        if (remaining <= 0)
            break;

        QThread::msleep(static_cast<unsigned long>(qMin<qint64>(20, remaining)));
    }

    if (triggered || triggerState != ScopeTriggerState::Ready)
        return AcquisitionResult::CaptureTimeout;
    return AcquisitionResult::NoTrigger;
}
} // namespace OscilloscopeAcquisition
bool OscilloscopeAcquisition::expandClippedChannels(IScopeMeasurement *scope, QAtomicInt &stop,
                                                    int &adjustments, VerticalScaleState *vertical,
                                                    bool saveCapture)
{
    auto check = [&] {
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
    };
    auto valid = [](double v) { return std::isfinite(v) && std::abs(v) < 1e20; };
    auto setScale = [&](int channel, double target) {
        check();
        if (adjustments >= 8)
            throw std::runtime_error("Clipped waveform: vertical scale adjustment limit reached");
        if (!valid(target) || target <= 0)
            throw std::runtime_error("Invalid calculated channel scale");
        QString error;
        if (!scope->setVerticalScale(channel, target, error))
            throw std::runtime_error(("Scale adjustment failed: " + error).toStdString());
        check();
        QElapsedTimer applied;
        applied.start();
        while (true) {
            const double actual = scope->getChannelScale(channel);
            check();
            if (!valid(actual))
                throw std::runtime_error("Invalid channel scale readback");
            if (actual > 0 && std::abs(actual - target) <= target * 0.01)
                break;
            if (applied.elapsed() >= 2000)
                throw std::runtime_error("Channel scale readback did not reach requested scale");
            QThread::msleep(20);
        }
    };
    bool changed = false;
    for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
        check();
        if (!scope->isChannelEnabled(ch) || !scope->isClipping(ch))
            continue;
        const double previous = scope->getChannelScale(ch);
        if (!valid(previous) || previous <= 0)
            throw std::runtime_error("Clipped waveform: invalid channel scale");
        setScale(ch, previous * 2);
        changed = true;
    }
    if (changed) {
        ++adjustments;
        return true;
    }
    if (saveCapture && vertical && vertical->captureRecord) {
        QVector<OscChannelMeasure> record;
        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            check();
            if (!scope->isChannelEnabled(ch))
                continue;
            OscChannelMeasure value;
            value.channel = ch;
            value.maxVal = readReadyMeasurement(scope, stop, ch, ScopeMeasurement::Maximum, 5000);
            value.minVal = readReadyMeasurement(scope, stop, ch, ScopeMeasurement::Minimum, 5000);
            if (!valid(value.maxVal) || !valid(value.minVal) || value.minVal > value.maxVal)
                throw std::runtime_error("Capture requires valid Max/Min");
            value.rms = scope->readMeasurement(ch, ScopeMeasurement::Rms);
            check();
            value.mean = scope->readMeasurement(ch, ScopeMeasurement::Mean);
            record.append(value);
        }
        check();
        vertical->captureRecord(record);
        check();
    }
    return false;
}

OscMeasureResult OscilloscopeAcquisition::captureAutoReference(IScopeMeasurement *scope, QAtomicInt &stop,
                                                               int timeoutMs, VerticalScaleState *vertical)
{
    OscMeasureResult result;
    if (!scope || timeoutMs <= 0) {
        result.errorMessage = "Invalid AUTO reference acquisition";
        return result;
    }
    QElapsedTimer referenceTimer;
    referenceTimer.start();
    auto check = [&] {
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        if (referenceTimer.elapsed() >= timeoutMs)
            throw std::runtime_error("AUTO reference phase timeout: NA");
    };
    try {
        check();
        QString error;
        const int channel = scope->triggerChannel();
        if (channel < 1 || channel > scope->getTotalChannel() || !scope->isChannelEnabled(channel))
            throw std::runtime_error("AUTO reference requires an enabled analog trigger channel");
        const double level = scope->getChannelScale(channel) * 0.04;
        if (!std::isfinite(level) || level <= 0 || level >= 1e19)
            throw std::runtime_error("Invalid channel scale for AUTO reference trigger level");
        auto configure = [&](auto operation) {
            check();
            if (!operation(error))
                throw std::runtime_error(error.toStdString());
        };
        configure([&](QString &e) { return scope->selectEdgeTrigger(e); });
        configure([&](QString &e) { return scope->setEdgeSlope(ScopeTriggerSlope::Rise, e); });
        configure([&](QString &e) { return scope->setChannelTriggerLevel(channel, level, e); });
        configure([&](QString &e) { return scope->setTriggerMode(ScopeTriggerMode::Auto, e); });
        check();
        VerticalScaleState localVertical;
        if (!vertical)
            vertical = &localVertical;
        int adjustments = 0;
        for (;;) {
            if (referenceTimer.elapsed() >= timeoutMs)
                throw std::runtime_error("AUTO reference timeout: NA");
            // Single explicitly transitions STOP -> sequence -> RUN before waiting.
            if (!scope->beginSingleAcquisition())
                throw std::runtime_error(
                    ("AUTO reference acquisition failed to start: " + scope->lastError()).toStdString());
            const auto state =
                waitForCapture(scope, stop, qMax(0, timeoutMs - int(referenceTimer.elapsed())));
            check();
            if (state != AcquisitionResult::Completed)
                throw std::runtime_error(state == AcquisitionResult::CommError
                                             ? "AUTO reference communication error"
                                             : "AUTO reference timeout: NA");
            if (expandClippedChannels(scope, stop, adjustments, vertical)) {
                continue;
            }
            break;
        }
        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            check();
            if (!scope->isChannelEnabled(ch))
                continue;
            if (scope->isClipping(ch))
                throw std::runtime_error("AUTO reference clipped; adjust channel scale");
            auto read = [&](ScopeMeasurement type) {
                check();
                const double value =
                    (type == ScopeMeasurement::Maximum || type == ScopeMeasurement::Minimum)
                        ? readReadyMeasurement(scope, stop, ch, type,
                                               qMin(5000, qMax(0, timeoutMs - int(referenceTimer.elapsed()))))
                        : scope->readMeasurement(ch, type);
                check();
                return std::isfinite(value) && std::abs(value) < 1e30
                           ? value
                           : std::numeric_limits<double>::quiet_NaN();
            };
            OscChannelMeasure value;
            value.channel = ch;
            value.maxVal = read(ScopeMeasurement::Maximum);
            value.minVal = read(ScopeMeasurement::Minimum);
            value.rms = read(ScopeMeasurement::Rms);
            value.mean = read(ScopeMeasurement::Mean);
            if (!std::isfinite(value.maxVal) || !std::isfinite(value.minVal) || value.maxVal < value.minVal)
                throw std::runtime_error("AUTO reference Max/Min: NA");
            result.channels.append(value);
        }
        if (result.channels.isEmpty())
            throw std::runtime_error("AUTO reference has no enabled channels");
        result.success = true;
    } catch (const std::exception &ex) {
        result.errorMessage = QString::fromUtf8(ex.what());
    } catch (...) {
        result.errorMessage = "Unexpected AUTO reference error";
    }
    auto cleanup = [&](auto operation) {
        QString error;
        try {
            if (operation(error))
                return;
        } catch (...) {
            error = "cleanup exception";
        }
        result.success = false;
        result.errorMessage += "; AUTO cleanup: " + error;
    };
    cleanup([&](QString &e) { return scope->stopAcquisition(e); });
    cleanup([&](QString &e) { return scope->setTriggerMode(ScopeTriggerMode::Normal, e); });
    if (stop.loadAcquire()) {
        result.success = false;
        result.errorMessage = "Stopped by user";
    }
    return result;
}
