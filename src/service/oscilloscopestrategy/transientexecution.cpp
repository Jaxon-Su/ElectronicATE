#include "transientexecution.h"
#include "../operations/shortcircuitexit.h"
#include <QThread>
#include <limits>
#include <stdexcept>

namespace TransientDetail {
using OscilloscopeAcquisition::AcquisitionResult;

Execution::Execution(const TransientContext &context, IScopeMeasurement *instrument,
                     QAtomicInt &stopFlag, bool shortProtection, bool diagnostics)
    : ctx(context), scope(instrument), stop(stopFlag), diagnostics(diagnostics), shortTest(shortProtection),
      shortMayBeOn(shortProtection) // A preceding Relay task may have left short ON.
{
}

void Execution::log(const QString &message)
{
    if (ctx.log)
        ctx.log(message);
}

void Execution::logProgress(const QString &message)
{
    if (!diagnostics) { log(message); return; }
    // Additional diagnostics must never replace an operation error or prevent cleanup.
    try {
        const qint64 elapsed = phase.isValid() ? phase.elapsed() : 0;
        log(message + QString("; Phase=%1; elapsed=%2 ms; remaining=%3 ms; limit=%4 ms")
            .arg(phaseName).arg(elapsed).arg(qMax<qint64>(0, ctx.phaseTimeoutMs - elapsed))
            .arg(ctx.phaseTimeoutMs));
    } catch (...) {}
}

void Execution::diagnostic(const QString &message)
{
    if (diagnostics)
        logProgress(QString("Diagnostic: stage=%1; %2").arg(diagnosticStageName, message));
}

void Execution::diagnosticStage(const QString &name, const QString &details)
{
    if (!diagnostics) return;
    diagnosticStageName = name;
    diagnostic(details.isEmpty() ? "begin" : "begin; " + details);
}

void Execution::diagnosticPhase(bool maximum)
{
    if (!diagnostics) return;
    phaseName = maximum ? "Rise" : "Fall";
    diagnosticStage("Phase initialization");
}

void Execution::diagnosticSettings()
{
    if (!diagnostics) return;
    logProgress(QString("Diagnostic: Settings: direction=%1; trigger timeout=%2 ms; phase limit=%3 ms; "
                        "power-off wait=%4 ms; settle=%5 ms; trials per level=%6; acquisition limit=%7; "
                        "vertical margin=%8; Capture=%9")
        .arg(ctx.target).arg(ctx.timeoutMs).arg(ctx.phaseTimeoutMs).arg(ctx.powerOffMs)
        .arg(ctx.settleMs).arg(ctx.trialsPerLevel).arg(ctx.maxIterations)
        .arg(ctx.verticalScale.margin ? "ON" : "OFF", ctx.captureRecord ? "ON" : "OFF"));
}

void Execution::diagnosticDelay(int ms, const QString &name)
{
    diagnosticStage(name, QString("requested=%1 ms").arg(ms));
    delay(ms);
    diagnostic("complete");
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
    diagnosticStage(on ? "Input ON" : "Input OFF");
    check();
    if (on) shortExitNeeded = true; // An ON exception may still have energized the DUT.
    log(on ? "Input ON" : "Input OFF");
    QString error;
    checked(ctx.power(on, error), error);
    diagnostic("complete");
}

void Execution::stopScope(const QString &diagnosticName)
{
    diagnosticStage(diagnosticName);
    configure([&](QString &error) { return scope->stopAcquisition(error); });
    diagnostic("complete");
}

void Execution::relays(bool shorted, bool discharge)
{
    diagnosticStage("Relay callback");
    check();
    if (shorted) {
        shortMayBeOn = true; // Set before IO: a multi-relay write can partially succeed.
        shortExitNeeded = true;
    }
    log(QString("Relay: short=%1 discharge=%2").arg(shorted).arg(discharge));
    QString error;
    checked(ctx.relays(shorted, discharge, error), error);
    shortMayBeOn = shorted;
    diagnostic("complete");
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
    diagnosticStage("Baseline wait");
    const auto result = waitForRecord(false);
    diagnostic("complete");
    return result;
}

AcquisitionResult Execution::waitForEvent()
{
    diagnosticStage("Event wait");
    const auto result = waitForRecord(shortTest);
    diagnostic("complete");
    return result;
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
    diagnosticStage("READY check");
    waitForReadiness(ReadinessPurpose::Event);
    diagnostic("complete");
}

bool Execution::waitForPowerOnReady()
{
    return waitForReadiness(ReadinessPurpose::PowerOn) == AcquisitionResult::NoTrigger;
}

AcquisitionResult Execution::waitForShortReady()
{
    return waitForReadiness(ReadinessPurpose::Short);
}

AcquisitionResult Execution::waitForPowerOffReady()
{
    return waitForReadiness(ReadinessPurpose::PowerOff);
}

AcquisitionResult Execution::waitForReadiness(ReadinessPurpose purpose)
{
    const bool beforeShort = purpose == ReadinessPurpose::Short;
    const bool beforePowerOff = purpose == ReadinessPurpose::PowerOff;
    const bool poweredBaseline = beforeShort || beforePowerOff;
    const QString recordName = beforePowerOff ? "Pre-power-off" : beforeShort ? "Pre-short" : "Pre-input";
    QString error;
    auto rejectedState = ScopeTriggerState::Invalid;
    try {
        if (OscilloscopeAcquisition::waitForReady(
                scope, stop,
                qMin(ctx.timeoutMs, qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed()))),
                error, &rejectedState)) {
            if (poweredBaseline && !scope->lastError().isEmpty())
                throw std::runtime_error((recordName + " readiness query failed: " + scope->lastError()).toStdString());
            check();
            if (!poweredBaseline) return AcquisitionResult::NoTrigger;
            // A record can complete just after the READY reply. This final
            // query narrows the gap; it is not atomic with the physical relay.
            const auto state = scope->acquisitionState();
            if (stop.loadAcquire()) throw std::runtime_error("Stopped by user");
            if (state == ScopeAcquisitionState::Invalid || !scope->lastError().isEmpty())
                throw std::runtime_error((recordName + " acquisition query failed: " + scope->lastError()).toStdString());
            if (state == ScopeAcquisitionState::Running) {
                check();
                return AcquisitionResult::NoTrigger;
            }
            rejectedState = ScopeTriggerState::Triggered;
            error = beforePowerOff ? "Acquisition completed before power-off" : "Acquisition completed before short";
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
                    if (recover && poweredBaseline) {
                        log(error + "; acquisition=STOP; " +
                            (beforePowerOff ? "pre-power-off" : "pre-short") + " record excluded from event results");
                        return late ? AcquisitionResult::CompletedAfterTimeout : AcquisitionResult::Completed;
                    }
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
            if (beforePowerOff && state == ScopeAcquisitionState::Running)
                error += "; acquisition=RUN; pre-power-off completion unconfirmed; not continuing";
        } catch (const std::exception &ex) { error += "; Diagnostic Capture: " + QString::fromUtf8(ex.what()); }
          catch (...) { error += "; Diagnostic Capture: unexpected error"; }
    }
    throw std::runtime_error(error.toStdString());
}

OscMeasureResult Execution::finish()
{
    diagnosticStage("Cleanup");
    // Cleanup is independent of stopFlag; failures never become successful measurements.
    auto cleanup = [&](const QString &label, const std::function<bool(QString &)> &action) {
        diagnosticStage("Cleanup " + label);
        QString error;
        try {
            if (action(error)) {
                diagnostic("complete");
                return;
            }
        } catch (const std::exception &ex) {
            error = QString::fromUtf8(ex.what());
        } catch (...) {
            error = "unexpected cleanup error";
        }
        result.success = false;
        result.cleanupFailed = true;
        result.errorMessage += (result.errorMessage.isEmpty() ? "" : "; ") + label + ": " + error;
        diagnostic("failed: " + error);
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
    diagnostic(QString("cleanup finished; result=%1%2")
        .arg(result.success ? "success" : "failure", result.errorMessage.isEmpty() ? QString{} : "; " + result.errorMessage));
    return result;
}

} // namespace TransientDetail
