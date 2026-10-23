#include "descentstrategy.h"
#include "transientexecution.h"
#include "descentsettings.h"
#include <cmath>
#include <stdexcept>

namespace {
using TransientDetail::Execution;
using OscilloscopeAcquisition::AcquisitionResult;

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
    for (const auto &channel : capture.channels) run.channels[channel.channel] = channel;
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
        for (int step = 0; step < context.descentLevels.size(); ++step) {
            const double level = context.descentLevels[step];
            for (int trial = 1; trial <= context.trialsPerLevel; ++trial) {
                run.check();
                if (m_shortTest) run.requireShortExit();
                else {
                    run.power(false);
                    run.delay(context.powerOffMs);
                    run.relays(false, false);
                }
                run.configure([&](QString &e) { return scope->setChannelTriggerLevel(context.descentChannel, level, e); });
                run.log(QString("Descent %1/%2: CH%3 Falling Level=%4; Trial %5/%6")
                            .arg(step + 1).arg(context.descentLevels.size()).arg(context.descentChannel)
                            .arg(level, 0, 'g', 16).arg(trial).arg(context.trialsPerLevel));
                run.power(true);
                run.delay(context.settleMs);
                run.check();
                if (!scope->beginSingleAcquisition())
                    throw std::runtime_error(("Failed to start descent acquisition: " + scope->lastError()).toStdString());
                run.log("Single processing wait: 3000 ms before READY");
                run.delay(3000);
                run.requireReady();
                if (m_shortTest) run.relays(true, false);
                else run.power(false);
                const auto status = run.waitForEvent();
                if (m_shortTest) run.requireShortExit(); // Includes a trigger miss.
                if (!OscilloscopeAcquisition::hasCompletedRecord(status))
                    throw std::runtime_error("Descent Trigger timeout: no completed event record");
                saveCompleted(run, status, step + 1, trial, level);
                run.configure([&](QString &e) { return scope->stopAcquisition(e); });
            }
        }
        run.check();
        run.result.success = true;
    } catch (const std::exception &error) { run.result.errorMessage = QString::fromUtf8(error.what()); }
      catch (...) { run.result.errorMessage = "Unexpected descent strategy error"; }
    return run.finish();
}
