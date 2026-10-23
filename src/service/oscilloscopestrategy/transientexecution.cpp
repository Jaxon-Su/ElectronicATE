#include "transientexecution.h"
#include "../operations/shortcircuitexit.h"
#include <QThread>
#include <limits>
#include <stdexcept>

namespace TransientDetail {
using OscilloscopeAcquisition::AcquisitionResult;

Execution::Execution(const TransientContext &context, IScopeMeasurement *instrument,
                     QAtomicInt &stopFlag, bool shortProtection)
    : ctx(context), scope(instrument), stop(stopFlag), shortTest(shortProtection),
      shortMayBeOn(shortProtection) // A preceding Relay task may have left short ON.
{
}

void Execution::log(const QString &message)
{
    if (ctx.log)
        ctx.log(message);
}

void Execution::check()
{
    if (stop.loadAcquire())
        throw std::runtime_error("Stopped by user");
    if (phase.isValid() && phase.elapsed() >= ctx.phaseTimeoutMs)
        throw std::runtime_error("Transient phase time limit reached");
}

void Execution::checked(bool ok, const QString &error)
{
    if (!ok)
        throw std::runtime_error(error.toStdString());
    check();
}

void Execution::configure(const std::function<bool(QString &)> &operation)
{
    check();
    QString error;
    checked(operation(error), error);
}

void Execution::power(bool on)
{
    check();
    if (on) shortExitNeeded = true; // An ON exception may still have energized the DUT.
    log(on ? "Input ON" : "Input OFF");
    QString error;
    checked(ctx.power(on, error), error);
}

void Execution::relays(bool shorted, bool discharge)
{
    check();
    if (shorted) {
        shortMayBeOn = true; // Set before IO: a multi-relay write can partially succeed.
        shortExitNeeded = true;
    }
    log(QString("Relay: short=%1 discharge=%2").arg(shorted).arg(discharge));
    QString error;
    checked(ctx.relays(shorted, discharge, error), error);
    shortMayBeOn = shorted;
}

bool Execution::exitShort()
{
    if (shortExitFailed) return false; // No automatic retry may release a retained short.
    if (!shortExitNeeded) return true;
    log("Short exit: Input OFF -> wait -> discharge -> release short");
    if (!finishShortCircuit([&](QString& e) { return ctx.power(false, e); }, ctx.relays,
                            shortMayBeOn, ctx.powerOffMs, ctx.dischargeMs, shortExitError)) {
        shortExitFailed = true;
        result.success = false;
        result.cleanupFailed = true;
        return false;
    }
    shortExitNeeded = false;
    shortMayBeOn = false;
    return true;
}

void Execution::requireShortExit()
{
    if (!exitShort()) throw std::runtime_error(shortExitError.toStdString());
}

void Execution::delay(int ms)
{
    for (int elapsed = 0; elapsed < ms;) {
        check();
        int chunk = qMin(20, ms - elapsed);
        QThread::msleep(chunk);
        elapsed += chunk;
    }
    check();
}

void Execution::waitAfterSingle()
{
    if (ctx.singleProcessingMs > 0)
        log(QString("Single processing wait: %1 ms before Baseline/READY").arg(ctx.singleProcessingMs));
    delay(ctx.singleProcessingMs);
}

AcquisitionResult Execution::waitForBaseline()
{
    return waitForRecord(false);
}

AcquisitionResult Execution::waitForEvent()
{
    return waitForRecord(shortTest);
}

AcquisitionResult Execution::waitForRecord(bool exitShortOnCompletion)
{
    check();
    auto status = OscilloscopeAcquisition::waitForCapture(
        scope, stop, qMin(ctx.timeoutMs, qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed()))),
        qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed())),
        exitShortOnCompletion ? std::function<void()>([this] { requireShortExit(); }) : std::function<void()>{});
    // Archive a completed record before applying the phase deadline.
    if (!OscilloscopeAcquisition::hasCompletedRecord(status)) check();
    if (status == AcquisitionResult::CommError)
        throw std::runtime_error("Acquisition communication error");
    if (status == AcquisitionResult::CaptureTimeout)
        throw std::runtime_error("Acquisition incomplete or scope not ready (not a trigger MISS)");
    if (status == AcquisitionResult::Cancelled)
        throw std::runtime_error("Stopped by user");
    return status;
}

void Execution::requireReady()
{
    waitForReadiness(ReadinessPurpose::Event);
}

bool Execution::waitForPowerOnReady()
{
    return waitForReadiness(ReadinessPurpose::PowerOn) == AcquisitionResult::NoTrigger;
}

AcquisitionResult Execution::waitForShortReady()
{
    return waitForReadiness(ReadinessPurpose::Short);
}

AcquisitionResult Execution::waitForReadiness(ReadinessPurpose purpose)
{
    const bool beforeShort = purpose == ReadinessPurpose::Short;
    const QString recordName = beforeShort ? "Pre-short" : "Pre-input";
    QString error;
    auto rejectedState = ScopeTriggerState::Invalid;
    try {
        if (OscilloscopeAcquisition::waitForReady(
                scope, stop,
                qMin(ctx.timeoutMs, qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed()))),
                error, &rejectedState)) {
            check();
            if (!beforeShort) return AcquisitionResult::NoTrigger;
            // A record can complete just after the READY reply. This final
            // query narrows the gap; it is not atomic with the physical relay.
            const auto state = scope->acquisitionState();
            if (stop.loadAcquire()) throw std::runtime_error("Stopped by user");
            if (state == ScopeAcquisitionState::Invalid || !scope->lastError().isEmpty())
                throw std::runtime_error(("Pre-short acquisition query failed: " + scope->lastError()).toStdString());
            if (state == ScopeAcquisitionState::Running) {
                check();
                return AcquisitionResult::NoTrigger;
            }
            rejectedState = ScopeTriggerState::Triggered;
            error = "Acquisition completed before short";
        }
    } catch (const std::exception &ex) { error = QString::fromUtf8(ex.what()); }
      catch (...) { error = "Unexpected trigger readiness error"; }
    // A pre-event trigger is still a diagnostic record. Never force STOP
    // to manufacture completion, and never bypass retained-short protection.
    const bool recover = purpose != ReadinessPurpose::Event &&
        (rejectedState == ScopeTriggerState::Triggered || rejectedState == ScopeTriggerState::Saving);
    if ((ctx.captureRecord || recover) && !stop.loadAcquire()) {
        try {
            auto state = scope->acquisitionState();
            if (recover && stop.loadAcquire())
                throw std::runtime_error("Stopped by user");
            if (recover && (state == ScopeAcquisitionState::Invalid || !scope->lastError().isEmpty()))
                throw std::runtime_error((recordName + " acquisition query failed: " + scope->lastError()).toStdString());
            bool late = false;
            if (recover && rejectedState == ScopeTriggerState::Triggered && state == ScopeAcquisitionState::Running) {
                // Trigger was observed before the event. Wait for its natural end;
                // a forced STOP could turn an incomplete record into false evidence.
                const int remaining = qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed()));
                const auto status = OscilloscopeAcquisition::waitForCapture(
                    scope, stop, remaining, remaining,
                    shortTest ? std::function<void()>([this] { requireShortExit(); }) : std::function<void()>{});
                if (!OscilloscopeAcquisition::hasCompletedRecord(status)) {
                    const QString detail = scope->lastError();
                    check();
                    throw std::runtime_error((recordName + " acquisition did not complete" +
                        (detail.isEmpty() ? QString{} : ": " + detail)).toStdString());
                }
                late = status == AcquisitionResult::CompletedAfterTimeout;
                state = ScopeAcquisitionState::Completed;
            }
            if (state == ScopeAcquisitionState::Completed) {
                if (shortTest) requireShortExit();
                if (!stop.loadAcquire()) {
                    const auto frozen = scope->acquisitionState();
                    if (stop.loadAcquire())
                        throw std::runtime_error("Stopped by user");
                    if (recover && (frozen != ScopeAcquisitionState::Completed || !scope->lastError().isEmpty()))
                        throw std::runtime_error((recordName + " record is not frozen: " + scope->lastError()).toStdString());
                    // Baseline processing owns its diagnostic and extrema. Do
                    // not save twice or discard a naturally completed late record.
                    if (recover && beforeShort)
                        return late ? AcquisitionResult::CompletedAfterTimeout : AcquisitionResult::Completed;
                    if (frozen == ScopeAcquisitionState::Completed && ctx.captureRecord) {
                        ScopeCaptureRecord record;
                        record.phase = "Before READY";
                        for (auto it = channels.cbegin(); it != channels.cend(); ++it) {
                            OscChannelMeasure value;
                            value.channel = it.key();
                            record.channels.append(value);
                        }
                        ctx.captureRecord(record);
                    }
                    if (recover) {
                        check();
                        if (late) throw std::runtime_error("Pre-input acquisition completed after timeout");
                        log(error + "; pre-input record excluded from event results");
                        return AcquisitionResult::Completed;
                    }
                }
            }
        } catch (const std::exception &ex) { error += "; Diagnostic Capture: " + QString::fromUtf8(ex.what()); }
          catch (...) { error += "; Diagnostic Capture: unexpected error"; }
    }
    throw std::runtime_error(error.toStdString());
}

OscMeasureResult Execution::finish()
{
    // Cleanup is independent of stopFlag; failures never become successful measurements.
    auto cleanup = [&](const QString &label, const std::function<bool(QString &)> &action) {
        QString error;
        try {
            if (action(error))
                return;
        } catch (const std::exception &ex) {
            error = QString::fromUtf8(ex.what());
        } catch (...) {
            error = "unexpected cleanup error";
        }
        result.success = false;
        result.cleanupFailed = true;
        result.errorMessage += (result.errorMessage.isEmpty() ? "" : "; ") + label + ": " + error;
    };
    if (touched) {
        if (shortTest && !exitShort() && !result.errorMessage.contains(shortExitError)) {
            result.success = false;
            result.errorMessage += (result.errorMessage.isEmpty() ? "" : "; ") + shortExitError;
        }
        cleanup("Scope STOP",
                [&](QString &e) { return scope->stopAcquisition(e); });
        if (!shortTest) {
            cleanup("Input OFF", [&](QString &e) { return ctx.power(false, e); });
            cleanup("Relay release", [&](QString &e) { return ctx.relays(false, false, e); });
        }
        cleanup("Load OFF", [&](QString &e) { return ctx.load(false, e); });
    }
    if (stop.loadAcquire()) {
        result.success = false;
        if (result.errorMessage.isEmpty())
            result.errorMessage = "Stopped by user";
    }
    for (auto channel : channels) {
        if (ctx.target == "MAX")
            channel.minVal = std::numeric_limits<double>::quiet_NaN();
        if (ctx.target == "MIN")
            channel.maxVal = std::numeric_limits<double>::quiet_NaN();
        result.channels.append(channel);
    }
    return result;
}

} // namespace TransientDetail
