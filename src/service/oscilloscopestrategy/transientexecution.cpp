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
    QString error;
    try {
        if (OscilloscopeAcquisition::waitForReady(
                scope, stop,
                qMin(ctx.timeoutMs, qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed()))), error)) {
            check();
            return;
        }
    } catch (const std::exception &ex) { error = QString::fromUtf8(ex.what()); }
      catch (...) { error = "Unexpected trigger readiness error"; }
    // A pre-event trigger is still a diagnostic record. Never force STOP
    // to manufacture completion, and never bypass retained-short protection.
    if (ctx.captureRecord && !stop.loadAcquire()) {
        try {
            if (scope->acquisitionState() == ScopeAcquisitionState::Completed) {
                if (shortTest) requireShortExit();
                if (!stop.loadAcquire() && scope->acquisitionState() == ScopeAcquisitionState::Completed) {
                    ScopeCaptureRecord record;
                    record.phase = "Before READY";
                    for (auto it = channels.cbegin(); it != channels.cend(); ++it) {
                        OscChannelMeasure value;
                        value.channel = it.key();
                        record.channels.append(value);
                    }
                    ctx.captureRecord(record);
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
