#include "oscilloscopeacquisition.h"
#include "iscopemeasurement.h"
#include <QElapsedTimer>
#include <QThread>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <exception>

namespace
{
QString exceptionMessage(const std::exception_ptr &error)
{
    try { std::rethrow_exception(error); }
    catch (const std::exception &e) { return QString::fromUtf8(e.what()); }
    catch (...) { return "Unexpected acquisition error"; }
}

double readReadyMeasurement(IScopeMeasurement *scope, QAtomicInt &stop, int channel, ScopeMeasurement type,
                            int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    double value = std::numeric_limits<double>::quiet_NaN();
    do {
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        value = OscilloscopeAcquisition::readMeasurementChecked(scope, channel, type);
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        if (std::isfinite(value) && std::abs(value) < 1e20)
            return value;
        if (timer.elapsed() >= timeoutMs)
            break;
        QThread::msleep(20);
    } while (timer.elapsed() < timeoutMs);
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace

namespace OscilloscopeAcquisition
{

bool channelEnabledChecked(IScopeMeasurement *scope, int channel)
{
    const bool enabled = scope->isChannelEnabled(channel);
    const QString error = scope->lastError();
    if (!error.isEmpty())
        throw std::runtime_error(
            QString("CH%1 display state query failed: %2").arg(channel).arg(error).toStdString());
    return enabled;
}

double readMeasurementChecked(IScopeMeasurement *scope, int channel, ScopeMeasurement type)
{
    const double value = scope->readMeasurement(channel, type);
    const QString error = scope->lastError();
    if (!error.isEmpty())
        throw std::runtime_error(
            QString("CH%1 measurement communication error: %2").arg(channel).arg(error).toStdString());
    return value;
}

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
                                 int completionTimeoutMs, const std::function<void()>& onRecordStopped)
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
    bool finalPoll = false;

    while (timeoutMs > 0) {
        // Check once at the deadline as well: a record can complete during the
        // last polling sleep. Do not force STOP and mistake a partial record for a hit.
        if (timer.elapsed() >= deadline) {
            if (finalPoll) break;
            finalPoll = true;
        }
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
            const bool late = timer.elapsed() >= deadline;
            // The record is frozen now. Let short-circuit tests de-energize before
            // measurement reads or file transfers, without starting another record.
            if (onRecordStopped) onRecordStopped();
            if (stop.loadAcquire())
                return AcquisitionResult::Cancelled;
            // Confirm that acquisition remains stopped after cleanup. Readiness of
            // measurements and SAVE operations is checked by their callers, not a fixed sleep.
            const auto frozenState = scope->acquisitionState();
            if (stop.loadAcquire())
                return AcquisitionResult::Cancelled;
            if (frozenState == ScopeAcquisitionState::Invalid)
                return AcquisitionResult::CommError;
            if (frozenState != ScopeAcquisitionState::Completed)
                return AcquisitionResult::CaptureTimeout;
            return late ? AcquisitionResult::CompletedAfterTimeout : AcquisitionResult::Completed;
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
            if (completionTimeoutMs > deadline) {
                deadline = completionTimeoutMs;
                finalPoll = false;
            }
        }

        const auto remaining = deadline - timer.elapsed();
        if (remaining <= 0) {
            if (finalPoll) break;
            continue;
        }

        QThread::msleep(static_cast<unsigned long>(qMin<qint64>(20, remaining)));
    }

    if (triggered || triggerState != ScopeTriggerState::Ready)
        return AcquisitionResult::CaptureTimeout;
    return AcquisitionResult::NoTrigger;
}
} // namespace OscilloscopeAcquisition
bool OscilloscopeAcquisition::expandClippedChannels(IScopeMeasurement *scope, QAtomicInt &stop,
                                                    int &adjustments, VerticalScaleState *vertical,
                                                    const QString &phase, bool completionTimedOut)
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
    ScopeCaptureRecord record;
    record.phase = phase;
    // Establish the channel list before clipping/measurement queries. A diagnostic
    // transfer must not depend on those queries succeeding.
    for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
        check();
        if (channelEnabledChecked(scope, ch)) {
            OscChannelMeasure value;
            value.channel = ch;
            record.channels.append(value);
        }
    }
    QVector<int> clipped;
    std::exception_ptr queryFailure;
    try {
        for (const auto &value : record.channels) {
            check();
            if (scope->isClipping(value.channel)) clipped.append(value.channel);
        }
        record.clipped = !clipped.isEmpty();
        if (vertical && vertical->captureRecord && !completionTimedOut) {
            for (auto &value : record.channels) {
                check();
                value.maxVal = readReadyMeasurement(scope, stop, value.channel, ScopeMeasurement::Maximum, 5000);
                value.minVal = readReadyMeasurement(scope, stop, value.channel, ScopeMeasurement::Minimum, 5000);
                if (vertical->includeRmsMean) {
                    value.rms = readMeasurementChecked(scope, value.channel, ScopeMeasurement::Rms);
                    check();
                    value.mean = readMeasurementChecked(scope, value.channel, ScopeMeasurement::Mean);
                }
            }
        }
    } catch (...) { queryFailure = std::current_exception(); }
    check(); // Stop and failed short-circuit exit never start a diagnostic transfer.
    if (vertical && vertical->captureRecord) {
        try { vertical->captureRecord(record); }
        catch (...) {
            if (queryFailure)
                throw std::runtime_error((exceptionMessage(queryFailure) + "; Capture: " +
                                          exceptionMessage(std::current_exception())).toStdString());
            throw;
        }
    }
    check();
    if (queryFailure) std::rethrow_exception(queryFailure);
    if (completionTimedOut) throw std::runtime_error("Acquisition completed after timeout");
    for (int ch : clipped) {
        const double previous = scope->getChannelScale(ch);
        if (!valid(previous) || previous <= 0)
            throw std::runtime_error("Clipped waveform: invalid channel scale");
        setScale(ch, previous * 2);
    }
    if (!clipped.isEmpty()) ++adjustments;
    return !clipped.isEmpty();
}

OscMeasureResult OscilloscopeAcquisition::captureAutoReference(IScopeMeasurement *scope, QAtomicInt &stop,
                                                               int timeoutMs, VerticalScaleState *vertical,
                                                               ReferenceMetrics metrics)
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
        if (channel < 1 || channel > scope->getTotalChannel() || !channelEnabledChecked(scope, channel))
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
            if (!hasCompletedRecord(state))
                throw std::runtime_error(state == AcquisitionResult::CommError
                                             ? "AUTO reference communication error"
                                             : "AUTO reference timeout: NA");
            // Reading or archiving a completed record can cross the phase deadline.
            // Archive the frozen record before enforcing that deadline.
            if (expandClippedChannels(scope, stop, adjustments, vertical, "Auto reference",
                                      state == AcquisitionResult::CompletedAfterTimeout)) {
                continue;
            }
            check();
            break;
        }
        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            check();
            if (!channelEnabledChecked(scope, ch))
                continue;
            if (scope->isClipping(ch))
                throw std::runtime_error("AUTO reference clipped; adjust channel scale");
            auto read = [&](ScopeMeasurement type) {
                check();
                const double value =
                    (type == ScopeMeasurement::Maximum || type == ScopeMeasurement::Minimum)
                        ? readReadyMeasurement(scope, stop, ch, type,
                                               qMin(5000, qMax(0, timeoutMs - int(referenceTimer.elapsed()))))
                        : readMeasurementChecked(scope, ch, type);
                check();
                return std::isfinite(value) && std::abs(value) < 1e30
                           ? value
                           : std::numeric_limits<double>::quiet_NaN();
            };
            OscChannelMeasure value;
            value.channel = ch;
            value.maxVal = read(ScopeMeasurement::Maximum);
            value.minVal = read(ScopeMeasurement::Minimum);
            // Reference results can request a quartet independently of Capture's contract.
            if (vertical->includeRmsMean || metrics == ReferenceMetrics::Quartet) {
                value.rms = read(ScopeMeasurement::Rms);
                value.mean = read(ScopeMeasurement::Mean);
            }
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
