#include "descentstrategy.h"
#include "transientexecution.h"
#include "descentsettings.h"
#include <cmath>
#include <stdexcept>

namespace {
using TransientDetail::Execution;
using OscilloscopeAcquisition::AcquisitionResult;

// Both descent modes recover clipping. Keep one adjustment budget across
// preflight records, event retries, Levels and Trials in this execution.
bool expandCompleted(Execution &run, AcquisitionResult status, int &adjustments, const QString &phase)
{
    ScopeCaptureRecord diagnostic;
    diagnostic.phase = phase;
    for (const auto &channel : run.channels) {
        OscChannelMeasure value;
        value.channel = channel.channel;
        diagnostic.channels.append(value);
    }
    bool captureAttempted = false;
    auto capture = [&] {
        if (run.stop.loadAcquire()) throw std::runtime_error("Stopped by user");
        captureAttempted = true;
        if (run.ctx.captureRecord) run.ctx.captureRecord(diagnostic);
    };
    OscilloscopeAcquisition::VerticalScaleState vertical;
    vertical.options = run.ctx.verticalScale;
    vertical.options.margin = false;
    vertical.readCaptureMeasurements = false;
    vertical.captureRecord = [&](const ScopeCaptureRecord &record) {
        diagnostic = record;
        if (record.clipped || status == AcquisitionResult::CompletedAfterTimeout) capture();
    };
    try {
        return OscilloscopeAcquisition::expandVerticalRange(
            run.scope, run.stop, adjustments, &vertical, phase,
            status == AcquisitionResult::CompletedAfterTimeout,
            QDeadlineTimer(qMax(0, run.ctx.phaseTimeoutMs - int(run.phase.elapsed()))));
    } catch (...) {
        QString failure;
        try { throw; }
        catch (const std::exception &error) { failure = QString::fromUtf8(error.what()); }
        catch (...) { failure = "Unexpected descent clipping check error"; }
        if (status == AcquisitionResult::CompletedAfterTimeout &&
            !failure.contains("Acquisition completed after timeout"))
            failure.prepend("Acquisition completed after timeout; ");
        // A completed record remains diagnostic evidence when queries or the
        // phase deadline fail; cancellation takes priority over transfers.
        if (!captureAttempted && !run.stop.loadAcquire()) {
            try { capture(); }
            catch (const std::exception &saveError) {
                failure += "; Capture: " + QString::fromUtf8(saveError.what());
            } catch (...) { failure += "; Unexpected Capture error"; }
        }
        throw std::runtime_error(failure.toStdString());
    }
}

void prepareUnclippedReference(Execution &run, int &adjustments)
{
    if (run.channels.isEmpty()) return; // Trigger-only runs have no displayed waveform to scale.
    run.log("Descent preflight: checking enabled channels with AUTO reference");
    // AUTO can complete on a DC line and does not require displaying the
    // selected trigger channel. Keep source, slope, Position and Offset intact.
    QString failure;
    try {
        run.configure([&](QString &e) { return run.scope->setTriggerMode(ScopeTriggerMode::Auto, e); });
        for (;;) {
            run.check();
            QString error;
            const auto status = OscilloscopeAcquisition::acquireAutoRecord(run.scope, run.stop,
                qMax(0,run.ctx.phaseTimeoutMs-int(run.phase.elapsed())), error, run.ctx.log);
            if (!OscilloscopeAcquisition::hasCompletedRecord(status))
                throw std::runtime_error(("Descent preflight: " + error).toStdString());
            if (!expandCompleted(run, status, adjustments, "Descent preflight")) break;
            run.log("Descent preflight: scale expanded; acquiring a new reference");
        }
    } catch (const std::exception &error) { failure = QString::fromUtf8(error.what()); }
      catch (...) { failure = "Unexpected descent preflight error"; }
    // Restore STOP/NORMAL even on Stop or timeout; preserve the original error
    // and do not let a failed restoration become a successful reference.
    auto restore = [&](const QString &label, const auto &operation) {
        QString error;
        try { if (operation(error)) return; }
        catch (const std::exception &ex) { error = QString::fromUtf8(ex.what()); }
        catch (...) { error = "unexpected error"; }
        run.result.cleanupFailed = true;
        failure += (failure.isEmpty() ? "" : "; ") + label + ": " + error;
    };
    restore("Descent preflight STOP", [&](QString &e) { return run.scope->stopAcquisition(e); });
    restore("Descent preflight NORMAL", [&](QString &e) { return run.scope->setTriggerMode(ScopeTriggerMode::Normal, e); });
    if (!failure.isEmpty()) throw std::runtime_error(failure.toStdString());
    run.check();
}

void saveCompleted(Execution &run, AcquisitionResult status, int step, int trial, double level)
{
    // The record is frozen, and short exit has already succeeded. Preserve a
    // diagnostic even when a query or the phase deadline prevents a valid Trial.
    ScopeCaptureRecord capture;
    capture.phase = QString("Descent Level %1 Trial %2 (%3)").arg(step).arg(trial).arg(level, 0, 'g', 16);
    for (const auto &channel : run.channels) {
        OscChannelMeasure current;
        current.channel = channel.channel;
        capture.channels.append(current);
    }
    MeasurementTrial record{int(run.result.trials.size()) + 1, MeasurementDirection::Fall,
                            step, trial, level, {}};
    QString failure;
    auto checkStop = [&] {
        if (run.stop.loadAcquire()) throw std::runtime_error("Stopped by user");
    };
    if (status == AcquisitionResult::CompletedAfterTimeout) failure = "Acquisition completed after timeout";
    else try {
        for (auto &channel : capture.channels) {
            checkStop();
            if (run.scope->isClipping(channel.channel)) {
                capture.clipped = true;
                throw std::runtime_error("Descent waveform clipped; adjust channel scale");
            }
            auto read = [&](ScopeMeasurement type) {
                checkStop();
                const double value = OscilloscopeAcquisition::readMeasurementChecked(run.scope, channel.channel, type);
                checkStop();
                return std::isfinite(value) && std::abs(value) < 1e30
                           ? value : std::numeric_limits<double>::quiet_NaN();
            };
            channel.maxVal = read(ScopeMeasurement::Maximum);
            channel.minVal = read(ScopeMeasurement::Minimum);
            channel.rms = read(ScopeMeasurement::Rms);
            channel.mean = read(ScopeMeasurement::Mean);
            record.channels.append({channel.channel, {}, channel.maxVal, channel.minVal, channel.rms, channel.mean});
        }
    } catch (const std::exception &error) { failure = QString::fromUtf8(error.what()); }
      catch (...) { failure = "Unexpected descent measurement error"; }
    checkStop();
    if (run.ctx.captureRecord) {
        try { run.ctx.captureRecord(capture); }
        catch (const std::exception &error) {
            failure += (failure.isEmpty() ? "" : "; ") + QString("Capture: %1").arg(QString::fromUtf8(error.what()));
        } catch (...) { failure += "; Unexpected Capture error"; }
    }
    if (!failure.isEmpty()) throw std::runtime_error(failure.toStdString());
    run.check();
    run.result.trials.append(record);
    run.log(QString("Descent event accepted: Step=%1 Trial=%2 Trigger Level=%3 Record=%4")
        .arg(step).arg(trial).arg(level, 0, 'g', 16).arg(record.sequence));
    for (const auto &channel : capture.channels) run.channels[channel.channel] = channel;
}

void rejectTrial(Execution &run, AcquisitionResult status, int step, int trial, double level, bool shortTest)
{
    const bool completed = OscilloscopeAcquisition::hasCompletedRecord(status);
    const QString purpose = shortTest ? "Pre-short" : "Pre-power-off";
    const QString label = QString("Descent Level %1 Trial %2 CH%3 Level=%4")
        .arg(step).arg(trial).arg(run.ctx.descentChannel).arg(level, 0, 'g', 16);
    QString reason = !completed ? "Trigger timeout: no completed event record (MISS)"
        : shortTest ? "acquisition completed before Short ON; no valid short event"
                    : "acquisition completed before Input OFF; no valid turn-off event";
    // Preserve a known deadline failure before cleanup or diagnostic Capture,
    // either of which can fail too. Neither error may hide the other.
    if (status == AcquisitionResult::CompletedAfterTimeout)
        reason += "; " + purpose + " acquisition completed after timeout";
    else if (run.phase.elapsed() >= run.ctx.phaseTimeoutMs)
        reason += "; Transient phase time limit reached";
    const QString failure = label + ": Fail — " + reason;
    run.result.trialIssues.append({MeasurementDirection::Fall, step, trial, level, false, failure});
    run.result.errorMessage += (run.result.errorMessage.isEmpty() ? "" : "; ") + failure;
    run.log(failure);
    // Cleanup must run even if the deadline expired during a blocking query.
    // Completion was confirmed before STOP; it is not manufactured by cleanup.
    auto cleanup = [&](const QString &name, const auto &operation) {
        QString error;
        try {
            if (operation(error)) return;
        } catch (const std::exception &ex) { error = QString::fromUtf8(ex.what()); }
          catch (...) { error = "unexpected error"; }
        run.result.cleanupFailed = true;
        throw std::runtime_error((purpose + " cleanup " + name + ": " + error).toStdString());
    };
    if (shortTest) {
        run.requireShortExit();
        run.log("Short descent cleanup: protected short exit complete -> Scope STOP");
    } else {
        run.log("Pre-power-off cleanup: Input OFF -> Scope STOP");
        cleanup("Input OFF", [&](QString &e) { return run.ctx.power(false, e); });
    }
    cleanup("Scope STOP", [&](QString &e) { return run.scope->stopAcquisition(e); });
    if (run.stop.loadAcquire()) throw std::runtime_error("Stopped by user");
    // A MISS has no completed record. STOP must not manufacture event or
    // diagnostic data; only the pretrigger path may archive a natural record.
    if (completed) {
        const auto frozen = run.scope->acquisitionState();
        const QString queryError = run.scope->lastError();
        if (run.stop.loadAcquire()) throw std::runtime_error("Stopped by user");
        if (frozen != ScopeAcquisitionState::Completed || !queryError.isEmpty())
            throw std::runtime_error(QString("%1 diagnostic record is not frozen: acquisition=%2%3")
                .arg(purpose, frozen == ScopeAcquisitionState::Completed ? "STOP" : frozen == ScopeAcquisitionState::Running ? "RUN" : "Invalid",
                     queryError.isEmpty() ? QString{} : "; " + queryError).toStdString());
        if (run.ctx.captureRecord) {
            ScopeCaptureRecord record;
            record.phase = label + (shortTest ? " (pre-short diagnostic; not an event)" : " (pre-power-off diagnostic; not an event)");
            for (const auto &ch : run.channels) {
                OscChannelMeasure value; value.channel = ch.channel;
                record.channels.append(value);
            }
            run.ctx.captureRecord(record);
        }
    }
    if (status == AcquisitionResult::CompletedAfterTimeout)
        throw std::runtime_error((purpose + " acquisition completed after timeout").toStdString());
    run.check();
    if (shortTest) {
        run.log(QString("Descent Level %1 Trial %2: cleanup complete; continuing with remaining Trials/Levels; task remains Fail")
                    .arg(step).arg(trial));
        return;
    }
    for (int skipped = trial + 1; skipped <= run.ctx.trialsPerLevel; ++skipped)
        run.result.trialIssues.append({MeasurementDirection::Fall, step, skipped, level, true,
                                      "Skipped after an earlier pre-power-off trigger at this Level"});
    run.log(QString("Descent Level %1: remaining trials skipped; cleanup complete; %2")
        .arg(step).arg(step < run.ctx.descentLevels.size() ? "continuing with next Level; task remains Fail"
                                                         : "no more Levels; task remains Fail"));
}
}

QString DescentStrategy::name() const
{
    return m_shortTest ? "Turn on then short (descent)" : "Turn off (descent)";
}

OscMeasureResult DescentStrategy::execute(IScopeMeasurement *scope, QAtomicInt &stop)
{
    auto context = m_context;
    context.captureRecord = m_captureObserver;
    context.target = "BOTH"; // Results contain all four values from each completed record.
    OscMeasureResult invalid;
    if (!scope || !context.power || !context.load || !context.relays || context.timeoutMs <= 0 ||
        context.phaseTimeoutMs <= 0 || context.settleMs < 0 || context.powerOffMs < 0 || context.dischargeMs < 0 ||
        context.trialsPerLevel < 1 || context.trialsPerLevel > 10 || context.descentChannel < 1 ||
        context.descentChannel > 8 || context.descentLevels.isEmpty() ||
        context.descentLevels.size() > DescentSettings::maximumLevels) {
        invalid.errorMessage = "Invalid descent settings or missing oscilloscope";
        return invalid;
    }
    for (double level : context.descentLevels) {
        if (!DescentSettings::validLevel(level)) {
            invalid.errorMessage = "Invalid descent Trigger level";
            return invalid;
        }
    }
    Execution run(context, scope, stop, m_shortTest);
    try {
        run.check();
        const int count = scope->getTotalChannel();
        if (context.descentChannel > count)
            throw std::runtime_error("Descent Trigger channel is unavailable on this oscilloscope");
        for (int ch = 1; ch <= count; ++ch) {
            run.check();
            if (OscilloscopeAcquisition::channelEnabledChecked(scope, ch)) {
                OscChannelMeasure channel;
                channel.channel = ch;
                run.channels[ch] = channel;
            }
        }
        if (m_shortTest && !run.channels.contains(context.descentChannel))
            throw std::runtime_error("Descent Trigger channel must be enabled on the oscilloscope");
        run.touched = true;
        if (m_shortTest) run.requireShortExit();
        else run.power(false);
        run.configure([&](QString &e) { return scope->stopAcquisition(e); });
        if (!m_shortTest) {
            run.log(QString("Descent Trigger CH%1 display %2 (Falling trigger remains selected)")
                        .arg(context.descentChannel).arg(context.descentChannelEnabled ? "ON" : "OFF"));
            run.configure([&](QString &e) {
                const bool ok = scope->setChannelDisplay(context.descentChannel, context.descentChannelEnabled, e);
                if (!ok) e = QString("Descent Trigger CH%1 display: %2").arg(context.descentChannel).arg(e);
                return ok;
            });
            const bool enabled = OscilloscopeAcquisition::channelEnabledChecked(scope, context.descentChannel);
            run.check();
            if (enabled != context.descentChannelEnabled)
                throw std::runtime_error(QString("Descent Trigger CH%1 display readback mismatch")
                                             .arg(context.descentChannel).toStdString());
            if (enabled) {
                OscChannelMeasure channel;
                channel.channel = context.descentChannel;
                run.channels[context.descentChannel] = channel;
            } else run.channels.remove(context.descentChannel);
        }
        run.configure([&](QString &e) { return scope->selectEdgeTrigger(e); });
        run.configure([&](QString &e) { return scope->setEdgeTriggerChannel(context.descentChannel, e); });
        run.configure([&](QString &e) { return scope->setEdgeSlope(ScopeTriggerSlope::Fall, e); });
        run.configure([&](QString &e) { return scope->setTriggerMode(ScopeTriggerMode::Normal, e); });
        run.configure([&](QString &e) { return context.load(true, e); });
        run.phase.start();
        int scaleAdjustments = 0;
        for (int step = 0; step < context.descentLevels.size(); ++step) {
            const double level = context.descentLevels[step];
            bool skipLevel = false;
            for (int trial = 1; trial <= context.trialsPerLevel; ++trial) {
                for (;;) {
                    run.check();
                    if (m_shortTest) run.requireShortExit();
                    else {
                        run.power(false);
                        run.delay(context.powerOffMs);
                        run.relays(false, false);
                    }
                    run.log(QString("Descent %1/%2: CH%3 Falling Level=%4; Trial %5/%6")
                                .arg(step + 1).arg(context.descentLevels.size()).arg(context.descentChannel)
                                .arg(level, 0, 'g', 16).arg(trial).arg(context.trialsPerLevel));
                    run.power(true);
                    run.delay(context.settleMs);
                    prepareUnclippedReference(run, scaleAdjustments);
                    run.configure([&](QString &e) { return scope->setChannelTriggerLevel(context.descentChannel, level, e); });
                    run.check();
                    if (!scope->beginSingleAcquisition())
                        throw std::runtime_error(("Failed to start descent acquisition: " + scope->lastError()).toStdString());
                    run.log("Single processing wait: 3000 ms before READY");
                    run.delay(3000);
                    {
                        if (m_shortTest)
                            run.log(QString("Descent Level %1 Trial %2: checking READY before Short ON (CH%3 Level=%4)")
                                .arg(step + 1).arg(trial).arg(context.descentChannel).arg(level, 0, 'g', 16));
                        else
                            run.log(QString("Descent Level %1 Trial %2: checking READY before Input OFF (CH%3 Level=%4)")
                                .arg(step + 1).arg(trial).arg(context.descentChannel).arg(level, 0, 'g', 16));
                        const auto ready = m_shortTest ? run.waitForShortReady() : run.waitForPowerOffReady();
                        if (OscilloscopeAcquisition::hasCompletedRecord(ready)) {
                            rejectTrial(run, ready, step + 1, trial, level, m_shortTest);
                            skipLevel = !m_shortTest;
                            break;
                        }
                    }
                    if (m_shortTest) run.relays(true, false);
                    else run.power(false);
                    const auto status = run.waitForEvent();
                    if (m_shortTest) run.requireShortExit(); // Includes a trigger miss.
                    if (m_shortTest && status == AcquisitionResult::NoTrigger) {
                        rejectTrial(run, status, step + 1, trial, level, true);
                        break;
                    }
                    if (!OscilloscopeAcquisition::hasCompletedRecord(status))
                        throw std::runtime_error("Descent Trigger timeout: no completed event record");
                    if (expandCompleted(run, status, scaleAdjustments,
                            QString("Descent Level %1 Trial %2 (%3)").arg(step + 1).arg(trial).arg(level, 0, 'g', 16))) {
                        run.log(QString("Descent waveform clipped: scale expanded; retry Level %1 Trial %2")
                                    .arg(step + 1).arg(trial));
                        continue;
                    }
                    saveCompleted(run, status, step + 1, trial, level);
                    run.configure([&](QString &e) { return scope->stopAcquisition(e); });
                    break;
                }
                if (skipLevel) break;
            }
        }
        run.check();
        run.result.success = run.result.trialIssues.isEmpty();
    } catch (const std::exception &error) {
        run.result.errorMessage += (run.result.errorMessage.isEmpty() ? "" : "; ") + QString::fromUtf8(error.what());
    } catch (...) { run.result.errorMessage += "; Unexpected descent strategy error"; }
    return run.finish();
}
