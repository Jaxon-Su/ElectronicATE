#include "transientstrategy.h"
#include "../../../../src/service/measurement/measurementformat.h"
#include "eventsearchbracket.h"
#include "iscopemeasurement.h"
#include "oscilloscopeacquisition.h"
#include "../../../../src/service/operations/shortcircuitexit.h"
#include <QElapsedTimer>
#include <QMap>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <stdexcept>

OscMeasureResult runLegacyTransientStrategy(TransientKind kind, const TransientContext &ctx,
                                      IScopeMeasurement *scope, QAtomicInt &stop)
{
    using OscilloscopeAcquisition::AcquisitionResult;
    OscMeasureResult result;
    if (!scope) {
        result.errorMessage = "No oscilloscope available";
        return result;
    }
    if (!ctx.power || !ctx.load || !ctx.relays || ctx.timeoutMs <= 0 || ctx.phaseTimeoutMs <= 0 ||
        ctx.maxIterations <= 0 || ctx.settleMs < 0 || ctx.dischargeMs < 0 || ctx.powerOffMs < 0 ||
        ctx.trialsPerLevel < 1 || ctx.trialsPerLevel > 10 ||
        !QStringList{"MAX", "MIN", "BOTH"}.contains(ctx.target) ||
        !QStringList{"AUTO", "RISING", "FALLING", "BOTH"}.contains(ctx.edge)) {
        result.errorMessage = "Invalid transient strategy settings";
        return result;
    }
    const bool single = kind == TransientKind::TurnOn || kind == TransientKind::ShortThenTurnOn;
    const bool shortBefore = kind == TransientKind::ShortThenTurnOn;
    const bool shortAfter = kind == TransientKind::TurnOnThenShort;
    const bool shortTest = shortBefore || shortAfter;
    const bool useSteadyReference = kind == TransientKind::TurnOff || shortAfter;
    QMap<int, OscChannelMeasure> channels;
    QMap<int, OscChannelMeasure> steadyChannels;
    bool haveValidEvent = false;
    QElapsedTimer phase;
    bool touched = false;
    bool shortMayBeOn = shortTest; // A preceding Relay task may already have closed this path.
    bool shortExitNeeded = true;
    bool shortExitFailed = false;
    QString shortExitError;
    auto log = [&](const QString &message) {
        if (ctx.log)
            ctx.log(message);
    };
    auto check = [&] {
        if (stop.loadAcquire())
            throw std::runtime_error("Stopped by user");
        if (phase.isValid() && phase.elapsed() >= ctx.phaseTimeoutMs)
            throw std::runtime_error("Transient phase time limit reached");
    };
    auto checked = [&](bool ok, const QString &error) {
        if (!ok)
            throw std::runtime_error(error.toStdString());
        check();
    };
    auto configure = [&](auto operation) {
        check();
        QString error;
        checked(operation(error), error);
    };
    auto power = [&](bool on) {
        check();
        if (on) shortExitNeeded = true; // An ON exception may still have energized the DUT.
        log(on ? "Input ON" : "Input OFF");
        QString error;
        checked(ctx.power(on, error), error);
    };
    auto relays = [&](bool shorted, bool discharge) {
        check();
        if (shorted) {
            shortMayBeOn = true; // Set before IO: a multi-relay write can partially succeed.
            shortExitNeeded = true;
        }
        log(QString("Relay: short=%1 discharge=%2").arg(shorted).arg(discharge));
        QString error;
        checked(ctx.relays(shorted, discharge, error), error);
        shortMayBeOn = shorted;
    };
    auto exitShort = [&] {
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
    };
    auto requireShortExit = [&] {
        if (!exitShort()) throw std::runtime_error(shortExitError.toStdString());
    };
    auto delay = [&](int ms) {
        for (int elapsed = 0; elapsed < ms;) {
            check();
            int chunk = qMin(20, ms - elapsed);
            QThread::msleep(chunk);
            elapsed += chunk;
        }
        check();
    };
    auto wait = [&](bool shortEvent = false) {
        check();
        auto status = OscilloscopeAcquisition::waitForCapture(
            scope, stop, qMin(ctx.timeoutMs, qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed()))),
            qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed())),
            shortTest && shortEvent ? std::function<void()>(requireShortExit) : std::function<void()>{});
        // Archive a completed record before applying the phase deadline.
        if (!OscilloscopeAcquisition::hasCompletedRecord(status)) check();
        if (status == AcquisitionResult::CommError)
            throw std::runtime_error("Acquisition communication error");
        if (status == AcquisitionResult::CaptureTimeout)
            throw std::runtime_error("Acquisition incomplete or scope not ready (not a trigger MISS)");
        if (status == AcquisitionResult::Cancelled)
            throw std::runtime_error("Stopped by user");
        return status;
    };
    auto valid = [](double value) { return std::isfinite(value) && std::abs(value) < 1e30; };
    try {
        check();
        const int trigger = scope->triggerChannel();
        for (int ch = 1; ch <= scope->getTotalChannel(); ++ch) {
            check();
            if (scope->isChannelEnabled(ch)) {
                OscChannelMeasure value;
                value.channel = ch;
                channels[ch] = value;
            }
        }
        if (!channels.contains(trigger))
            throw std::runtime_error("Trigger source must be an enabled analog channel");
        auto initialSearchStep = [&] {
            check();
            const double scale = scope->getChannelScale(trigger);
            check();
            const double step = scale * 0.04;
            if (!valid(scale) || !valid(step) || step <= 0)
                throw std::runtime_error("Invalid trigger channel scale for search step");
            log(QString("CH%1 Scale=%2 units/div; initial search step=%3 units (0.04 div)")
                    .arg(trigger)
                    .arg(scale)
                    .arg(step));
            return step;
        };
        OscilloscopeAcquisition::VerticalScaleState vertical;
        vertical.captureRecord = ctx.captureRecord;
        vertical.includeRmsMean = false;
        auto requireReady = [&] {
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
        };
        initialSearchStep(); // Validate before enabling hardware.
        touched = true;
        // Configure the load while input is off. Hardware callbacks use the same executors as ManualControl.
        if (shortTest) requireShortExit();
        else power(false);
        QString error;
        checked(ctx.load(true, error), error);
        auto prepareInputOff = [&] {
            if (shortTest) {
                requireShortExit();
                if (shortBefore) relays(true, false);
                return;
            }
            power(false);
            delay(ctx.powerOffMs);
            relays(shortBefore, false);
        };
        for (bool maximum : {true, false}) {
            if ((maximum && ctx.target == "MIN") || (!maximum && ctx.target == "MAX"))
                continue;
            phase.restart();
            double tolerance = initialSearchStep();
            // A prior Min search can leave a negative level; restart Max inside the positive domain.
            double level = maximum ? tolerance : -tolerance;
            double referenceFloor = -std::numeric_limits<double>::infinity();
            bool referenceReady = false;
            if (useSteadyReference) {
                prepareInputOff();
                power(true);
                delay(ctx.settleMs);
                const auto reference = OscilloscopeAcquisition::captureAutoReference(
                    scope, stop, qMax(0, ctx.phaseTimeoutMs - int(phase.elapsed())), &vertical);
                checked(reference.success, reference.errorMessage);
                for (const auto &value : reference.channels) {
                    if (!channels.contains(value.channel))
                        continue;
                    auto &steady = steadyChannels[value.channel];
                    steady.channel = value.channel;
                    steady.maxVal =
                        valid(steady.maxVal) ? std::max(steady.maxVal, value.maxVal) : value.maxVal;
                    steady.minVal =
                        valid(steady.minVal) ? std::min(steady.minVal, value.minVal) : value.minVal;
                }
                for (auto it = channels.cbegin(); it != channels.cend(); ++it) {
                    const auto found = std::find_if(
                        reference.channels.cbegin(), reference.channels.cend(),
                        [&](const OscChannelMeasure &value) { return value.channel == it.key(); });
                    if (found == reference.channels.cend() || !valid(found->maxVal) || !valid(found->minVal))
                        throw std::runtime_error("AUTO reference channel measurement missing");
                }
                tolerance = initialSearchStep();
                bool found = false;
                for (const auto &value : reference.channels)
                    if (value.channel == trigger) {
                        referenceFloor = maximum ? value.maxVal : -value.minVal;
                        level = maximum ? std::max(initialSearchStep(), value.maxVal + tolerance)
                                        : value.minVal - tolerance;
                        found = true;
                        log(QString("AUTO steady reference Max=%1 Min=%2; initial Level=%3 channel units")
                                .arg(value.maxVal)
                                .arg(value.minVal)
                                .arg(level));
                    }
                if (!found)
                    throw std::runtime_error("AUTO reference trigger channel missing");
                referenceReady = true;
            }
            configure([&](QString &e) { return scope->selectEdgeTrigger(e); });
            configure([&](QString &e) {
                return scope->setEdgeSlope(maximum ? ScopeTriggerSlope::Rise : ScopeTriggerSlope::Fall, e);
            });
            configure([&](QString &e) { return scope->setTriggerMode(ScopeTriggerMode::Normal, e); });
            int acquisitions = useSteadyReference ? 1 : 0; // Includes AUTO reference.
            auto arm = [&] {
                check();
                if (++acquisitions > ctx.maxIterations)
                    throw std::runtime_error("Transient acquisition iteration limit reached");
                log(QString("%1: arm Level=%2 channel units").arg(maximum ? "Max" : "Min").arg(level));
                configure([&](QString &e) { return scope->setChannelTriggerLevel(trigger, level, e); });
                if (!scope->beginSingleAcquisition())
                    throw std::runtime_error("Failed to start single acquisition");
                check();
            };
            auto readEvent = [&] {
                double peak = std::numeric_limits<double>::quiet_NaN();
                for (auto it = channels.begin(); it != channels.end(); ++it) {
                    check();
                    if (scope->isClipping(it.key()))
                        throw std::runtime_error("Event waveform clipped; adjust channel scale");
                    auto read = [&](ScopeMeasurement type) {
                        check();
                        const double v = OscilloscopeAcquisition::readMeasurementChecked(scope, it.key(), type);
                        check();
                        return valid(v) ? v : std::numeric_limits<double>::quiet_NaN();
                    };
                    const double high = read(ScopeMeasurement::Maximum), low = read(ScopeMeasurement::Minimum);
                    if (valid(high))
                        it->maxVal = valid(it->maxVal) ? std::max(it->maxVal, high) : high;
                    if (valid(low))
                        it->minVal = valid(it->minVal) ? std::min(it->minVal, low) : low;
                    log(QString("CH%1 event Max=%2 Min=%3; recorded Max=%4 Min=%5")
                            .arg(it.key())
                            .arg(formatOscMeasurement(high), formatOscMeasurement(low),
                                 formatOscMeasurement(it->maxVal), formatOscMeasurement(it->minVal)));
                    if (it.key() == trigger)
                        peak = maximum ? high : low;
                }
                if (!valid(peak))
                    throw std::runtime_error("Trigger measurement: NA");
                haveValidEvent = true;
                return peak;
            };
            // Search in signed coordinates: larger x means a more extreme threshold for either phase.
            const double direction = maximum ? 1.0 : -1.0;
            double observedPeak = -std::numeric_limits<double>::infinity();
            double baselineFloor = referenceFloor;
            enum class Probe { Hit, Miss, Baseline, Rescale };
            int scaleAdjustments = 0;
            auto probe = [&](double candidate) {
                if (!valid(candidate))
                    throw std::runtime_error("Invalid search threshold");
                level = direction * candidate;
                bool hit = false;
                for (int trial = 0; trial < ctx.trialsPerLevel; ++trial) {
                    check();
                    log(QString("Trial %1/%2").arg(trial + 1).arg(ctx.trialsPerLevel));
                    if (!referenceReady) {
                        prepareInputOff();
                    }
                    if (single) {
                        if (!shortBefore) {
                            relays(false, true);
                            delay(ctx.dischargeMs);
                            relays(false, false);
                        }
                        arm();
                        requireReady();
                        power(true);
                    } else {
                        if (!referenceReady) {
                            power(true);
                            log(QString("Settle %1 ms").arg(ctx.settleMs));
                            delay(ctx.settleMs);
                        }
                        referenceReady = false;
                        arm();
                        const auto baselineStatus = wait();
                        if (OscilloscopeAcquisition::hasCompletedRecord(baselineStatus)) {
                            if (shortTest) requireShortExit();
                            if (OscilloscopeAcquisition::expandClippedChannels(scope, stop, scaleAdjustments,
                                                                               &vertical, "Baseline",
                                                                               baselineStatus == AcquisitionResult::CompletedAfterTimeout)) {
                                configure([&](QString &e) { return scope->stopAcquisition(e); });
                                return Probe::Rescale;
                            }
                            if (scope->isClipping(trigger))
                                throw std::runtime_error("Baseline waveform clipped; adjust channel scale");
                            const double peak =
                                OscilloscopeAcquisition::readMeasurementChecked(
                                    scope, trigger, maximum ? ScopeMeasurement::Maximum : ScopeMeasurement::Minimum);
                            check();
                            if (!valid(peak))
                                throw std::runtime_error("Baseline measurement: NA");
                            baselineFloor = std::max(candidate, direction * peak);
                            configure([&](QString &e) { return scope->stopAcquisition(e); });
                            log("Baseline trigger excluded from event results");
                            return Probe::Baseline;
                        }
                        log(shortAfter ? "Apply short; wait for event" : "Power off; wait for event");
                        requireReady();
                        if (shortAfter) {
                            relays(true, false);
                        } else
                            power(false);
                    }
                    const auto eventStatus = wait(true);
                    // Single completion has frozen the record. De-energize before measurements,
                    // Capture transfer or scale changes. Also exit immediately on a trigger MISS.
                    if (shortTest) requireShortExit();
                    if (OscilloscopeAcquisition::hasCompletedRecord(eventStatus)) {
                        if (OscilloscopeAcquisition::expandClippedChannels(scope, stop, scaleAdjustments,
                                                                           &vertical, "Measurement",
                                                                           eventStatus == AcquisitionResult::CompletedAfterTimeout)) {
                            configure([&](QString &e) { return scope->stopAcquisition(e); });
                            return Probe::Rescale;
                        }
                        observedPeak = std::max(observedPeak, direction * readEvent());
                        hit = true;
                    }
                    configure([&](QString &e) { return scope->stopAcquisition(e); });
                }
                // One hit establishes the lower edge; only all misses establish the upper edge.
                log(hit ? "Level: event captured" : "Level: all trials missed");
                return hit ? Probe::Hit : Probe::Miss;
            };
            bool noEvent = false;
            EventSearchBracket bracket(direction * level, initialSearchStep());
            for (;;) {
                check();
                const auto outcome = probe(bracket.candidate());
                if (outcome == Probe::Rescale) {
                    log("Vertical scale adjusted; repeat event with a new acquisition");
                    tolerance = initialSearchStep();
                    bracket.reset(tolerance);
                    continue;
                }
                const auto progress =
                    outcome == Probe::Baseline
                        ? bracket.baseline(baselineFloor)
                        : bracket.observe(outcome == Probe::Hit, observedPeak, baselineFloor);
                if (bracket.bounded())
                    log(QString("Bracket [%1, %2]; width=%3 channel units")
                            .arg(direction * bracket.hit())
                            .arg(direction * bracket.miss())
                            .arg(bracket.miss() - bracket.hit()));
                if (progress == EventSearchBracket::Progress::NoEvent) {
                    noEvent = true;
                    break;
                }
                if (progress == EventSearchBracket::Progress::Converged)
                    break;
            }
            if (noEvent) {
                const QString message =
                    QString("%1: no event trigger bracket; retaining valid event measurements")
                        .arg(maximum ? "No rising event" : "No falling event");
                log(message);
                continue;
            }
            log(QString("%1 search converged; returning accumulated measured peaks")
                    .arg(maximum ? "Max" : "Min"));
        }
        check();
        // Merge references only after all selected searches finish normally. Errors never become fallback
        // success. Transient results contain extrema only.
        if (useSteadyReference) {
            for (auto it = channels.begin(); it != channels.end(); ++it) {
                const auto steady = steadyChannels.value(it.key());
                const bool useMax = !valid(it->maxVal) || steady.maxVal > it->maxVal;
                const bool useMin = !valid(it->minVal) || steady.minVal < it->minVal;
                if (useMax)
                    it->maxVal = steady.maxVal;
                if (useMin)
                    it->minVal = steady.minVal;
                if (useMax || useMin)
                    log(QString("CH%1 Steady fallback:%2%3")
                            .arg(it.key())
                            .arg(useMax ? QString(" Max=%1").arg(it->maxVal) : QString())
                            .arg(useMin ? QString(" Min=%1").arg(it->minVal) : QString()));
            }
        }
        // Either edge can capture a complete event containing both Max and Min.
        // A missing opposite edge is informational, not a fabricated NA measurement.
        result.success = haveValidEvent || useSteadyReference;
        if (!result.success)
            result.errorMessage = "No valid event trigger bracket in selected direction(s) (NA)";
    } catch (const std::exception &ex) {
        result.errorMessage = QString::fromUtf8(ex.what());
    } catch (...) {
        result.errorMessage = "Unexpected transient strategy error";
    }
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
