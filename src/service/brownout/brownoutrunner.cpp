#include "brownoutrunner.h"
#include <QElapsedTimer>
#include <QThread>
#include <QSet>
#include <atomic>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool ok, const QString &error)
{
    if (!ok) throw std::runtime_error(error.toStdString());
}
void validVoltage(double value)
{
    require(std::isfinite(value) && std::abs(value) < 1e20, "Invalid live output voltage");
}
QString exceptionText()
{
    try { throw; }
    catch (const std::exception &e) { return QString::fromUtf8(e.what()); }
    catch (...) { return "Unexpected Brown Out operation error"; }
}
QSet<int> validateRecord(const BrownOutRecord &record)
{
    QSet<int> channels;
    require(record.sequence > 0 && !record.channels.isEmpty(), "Scope returned an empty acquisition");
    for (const auto &v : record.channels) {
        require(v.channel >= 1 && v.channel <= 8 && !channels.contains(v.channel) &&
                    std::isfinite(v.maximum) && std::isfinite(v.minimum) && std::isfinite(v.rms) &&
                    std::isfinite(v.mean) && v.maximum >= v.minimum && v.rms >= 0 && !v.unit.isEmpty(),
                "Scope returned invalid record measurements");
        channels.insert(v.channel);
    }
    return channels;
}
}

MeasurementResult runBrownOut(const BrownOut::Settings &cfg, IScopeBrownOut &scope,
                          const BrownOutIoFactory &createIo, QAtomicInt &stop, const OscCaptureObserver &capture,
                          const std::function<void(const QString &)> &log)
{
    MeasurementResult result;
    QString error;
    auto archive = [&](const BrownOutRecord &record, const QString &phase) {
        if (!capture) return;
        ScopeCaptureRecord data;
        data.phase = phase; data.clipped = record.clipped;
        for (const auto &v : record.channels)
            data.channels.append({v.channel, v.maximum, v.minimum, v.rms, v.mean});
        capture(data);
    };
    std::unique_ptr<IScopeBrownOutSession> session;
    std::atomic_bool ready{false}, ramp{false}, firstStep{false}, shutdownConfirmed{false},
        release{false}, failed{false}, outputsReleased{false};
    std::mutex mutex;
    // Serialize the pause acknowledgement with voltage writes. Monitoring never
    // holds this mutex while waiting for scope data or saving files.
    std::mutex rampMutex;
    QElapsedTimer wallClock; wallClock.start();
    bool rampHeld = false;
    qint64 heldSince = 0, heldTotal = 0;
    auto activeTimeLocked = [&] {
        return (rampHeld ? heldSince : wallClock.elapsed()) - heldTotal;
    };
    auto activeTime = [&] {
        std::lock_guard<std::mutex> lock(rampMutex);
        return activeTimeLocked();
    };
    auto pauseRamp = [&] {
        std::lock_guard<std::mutex> lock(rampMutex);
        if (!rampHeld) { heldSince = wallClock.elapsed(); rampHeld = true; }
    };
    auto resumeRamp = [&] {
        std::lock_guard<std::mutex> lock(rampMutex);
        if (rampHeld) { heldTotal += wallClock.elapsed() - heldSince; rampHeld = false; }
    };
    QString controlError, cleanupError;
    double finalVin = 0, maxStepDelay = 0;
    std::atomic_int steps{0};
    auto cancelled = [&] { return release.load() || stop.loadAcquire(); };
    auto sleep = [&](int milliseconds) {
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < milliseconds && !cancelled())
            QThread::msleep(10);
    };
    // Track Capture pauses input steps, but never shutdown monitoring. All controller transports
    // stay on this thread, including construction and cleanup (notably QSerialPort).
    std::thread controller;
    try {
        controller = std::thread([&] {
            std::unique_ptr<IBrownOutIo> io;
            try {
                require(bool(createIo), "Brown Out input/monitor adapter is unavailable");
                io = createIo();
                require(bool(io), "Brown Out input/monitor connection failed");
                if (cancelled()) throw std::runtime_error("Stopped by user");
                io->start(cancelled);
                sleep(cfg.settleMs);
                if (cancelled()) throw std::runtime_error("Stopped by user");
                const double initial = io->outputVoltage();
                validVoltage(initial);
                require(initial > cfg.shutdownVoltage, "Output is already below shutdown threshold before ramp");
                double voltage = io->initialVoltage();
                require(std::isfinite(voltage) && voltage > cfg.minimumVin,
                        "Initial Vin must exceed minimum Vin");
                ready.store(true);
                while (!ramp.load() && !cancelled()) {
                    const double output = io->outputVoltage();
                    if (cancelled()) break;
                    validVoltage(output);
                    require(output > cfg.shutdownVoltage,
                            "Output fell below shutdown threshold before ramp");
                    sleep(100);
                }
                QElapsedTimer clock; clock.start();
                qint64 previousStep = activeTime(), lastProgress = previousStep;
                qint64 nextStep = previousStep + cfg.stepIntervalMs, lowSince = -1, lowPrevious = -1;
                while (!cancelled() && !shutdownConfirmed.load()) {
                    // Slow ramps can legitimately exceed two minutes. Bound a stalled step or
                    // the wait at minimum Vin, rather than the duration of the whole descent.
                    require(activeTime() - lastProgress < qint64(cfg.stepIntervalMs) + cfg.timeoutMs,
                            "Output shutdown was not confirmed before the voltage-progress timeout");
                    const double output = io->outputVoltage();
                    if (cancelled()) break;
                    validVoltage(output);
                    const auto now = clock.elapsed();
                    if (output <= cfg.shutdownVoltage) {
                        // A long communication gap is not evidence of continuously low output.
                        if (lowSince < 0 || now - lowPrevious > 500) lowSince = now;
                        lowPrevious = now;
                        if (now - lowSince >= cfg.confirmMs) {
                            finalVin = voltage;
                            shutdownConfirmed.store(true);
                            break;
                        }
                    } else {
                        lowSince = lowPrevious = -1;
                    }
                    {
                        std::lock_guard<std::mutex> lock(rampMutex);
                        const auto activeNow = activeTimeLocked();
                        if (!rampHeld && !cancelled() && lowSince < 0 && activeNow >= nextStep && voltage > cfg.minimumVin) {
                            maxStepDelay = std::max(maxStepDelay, double(activeNow - previousStep - cfg.stepIntervalMs));
                            previousStep = activeNow;
                            voltage = std::max(cfg.minimumVin, voltage - cfg.voltageStep);
                            firstStep.store(true);
                            io->setVoltage(voltage);
                            lastProgress = activeTimeLocked();
                            ++steps;
                            // Paused time is excluded; never catch up with a burst of writes.
                            nextStep = lastProgress + cfg.stepIntervalMs;
                        }
                    }
                    sleep(100);
                }
                while (!cancelled()) {
                    const double output = io->outputVoltage();
                    validVoltage(output);
                    require(output <= cfg.shutdownVoltage,
                            "Output recovered during the shutdown observation interval");
                    sleep(100);
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex);
                controlError = exceptionText();
                failed.store(true);
            }
            if (io) {
                QString e;
                try {
                    if (!io->shutdown(e)) {
                        std::lock_guard<std::mutex> lock(mutex);
                        cleanupError = e.isEmpty() ? "Brown Out output cleanup failed" : e;
                    } else outputsReleased.store(true);
                } catch (...) {
                    std::lock_guard<std::mutex> lock(mutex); cleanupError = exceptionText();
                }
            }
        });
        auto check = [&] {
            require(!stop.loadAcquire(), "Stopped by user");
            if (failed.load()) {
                std::lock_guard<std::mutex> lock(mutex);
                throw std::runtime_error(controlError.toStdString());
            }
        };
        QElapsedTimer startup; startup.start();
        while (!ready.load()) {
            check();
            require(startup.elapsed() < cfg.settleMs + 60000, "Input/Load startup timed out");
            QThread::msleep(20);
        }
        check();
        session = scope.beginBrownOut(cfg, stop, error);
        if (!session) {
            // Do not retry an initialization failure whose hardware restoration is uncertain.
            result.cleanupFailed = true;
            throw std::runtime_error(("Brown Out scope preparation failed: " + error).toStdString());
        }
        const int recordDurationMs = session->recordDurationMs();
        require(recordDurationMs > 0, "Invalid scope acquisition duration");
        const qint64 acquisitionTimeoutMs = qint64(recordDurationMs) + cfg.dataTimeoutMs;
        const qint64 referenceLimit = qMin(acquisitionTimeoutMs,qint64(session->referenceTimeoutMs()));
        require(referenceLimit > 0 && referenceLimit <= 60000, "Invalid AUTO reference timeout budget");
        QElapsedTimer fresh;
        qint64 lastRecordPollElapsed = 0;
        // Error records with a sequence/channel list are frozen diagnostics supplied
        // by the driver. Save them before propagating their original read failure.
        auto pollRecord = [&](BrownOutRecord &record, const QString &phase, quint64 previousSequence = 0) {
            record = {};
            QString readError;
            const auto status = session->poll(record, readError);
            // Capture may be slow: classify acquisition completion before file transfer.
            lastRecordPollElapsed = fresh.elapsed();
            if (phase == "Auto reference" && status == BrownOutRecordStatus::Ready &&
                lastRecordPollElapsed >= referenceLimit)
                readError = "AUTO reference timeout; completed record is diagnostic only";
            if (status == BrownOutRecordStatus::Ready ||
                (status == BrownOutRecordStatus::Error && record.sequence > 0 && !record.channels.isEmpty())) {
                require(record.sequence > previousSequence, "Scope returned an old record; diagnostic capture rejected");
                try { archive(record, phase); }
                catch (...) {
                    const QString captureError = exceptionText();
                    throw std::runtime_error((readError.isEmpty() ? captureError : readError + "; Capture: " + captureError).toStdString());
                }
            }
            require(status != BrownOutRecordStatus::Error, readError);
            check(); // A transfer may have received Stop or a monitoring failure.
            return status;
        };
        auto waitUntilReady = [&](const QString &phase, quint64 previousSequence) {
            // Processing time is separate from the existing READY reply budget.
            // Track+Capture keeps the ramp paused across this wait and rearming.
            QElapsedTimer processing; processing.start();
            while (processing.elapsed() < 3000) {
                check();
                QThread::msleep(10);
            }
            check();
            QElapsedTimer arming; arming.start();
            for (;;) {
                check();
                QString readyError;
                if (session->readyToRamp(readyError)) {
                    check();
                    require(arming.elapsed() < acquisitionTimeoutMs, "Scope READY reply arrived after timeout");
                    return;
                }
                // A spontaneous completed record must not be discarded just because
                // the next ramp step has not been authorized yet.
                BrownOutRecord unexpected;
                const auto status = pollRecord(unexpected, phase, previousSequence);
                require(status != BrownOutRecordStatus::Ready, "Scope completed a record before ramp was ready; diagnostic capture attempted");
                require(readyError.isEmpty(), readyError);
                require(arming.elapsed() < acquisitionTimeoutMs, "Scope did not become ready before ramp");
                QThread::msleep(20);
            }
        };
        BrownOutRecord baseline;
        if (log) log(QString("AUTO reference: fresh RUNSTOP record; wait limit=%1 ms").arg(referenceLimit));
        fresh.start();
        QString lastReferenceState;
        while (true) {
            check();
            const auto status = pollRecord(baseline, "Auto reference");
            const auto state = session->referenceState();
            if (log && state != lastReferenceState) {
                log(QString("AUTO reference: %1; elapsed=%2 ms").arg(state).arg(lastRecordPollElapsed));
                lastReferenceState = state;
            }
            require(lastRecordPollElapsed < referenceLimit,
                    "AUTO reference timeout; no timely coherent reference; " + state);
            if (status == BrownOutRecordStatus::Ready) {
                if (log) log("AUTO reference complete; preparing Normal ramp trigger");
                break;
            }
            QThread::msleep(100);
        }
        require(!baseline.clipped, "Brown Out reference is clipped");
        auto trigger = std::find_if(baseline.channels.cbegin(), baseline.channels.cend(),
                                   [&](const auto &v) { return v.channel == cfg.triggerChannel; });
        require(trigger != baseline.channels.cend(), "Trigger channel is missing from reference");
        const auto expectedChannels = validateRecord(baseline);
        double level = cfg.triggerLevel;
        const double step = session->levelStep();
        require(std::isfinite(step) && step > 0, "Invalid trigger level step");
        if (cfg.mode == BrownOut::Mode::FixedFall) {
            require(trigger->unit.trimmed().compare("V", Qt::CaseInsensitive) == 0,
                    "Fixed Fall (Vout) requires a voltage trigger channel");
            require(trigger->minimum > level, "Fixed Fall level must be below the steady trigger-channel minimum");
        } else
            level = cfg.rising ? std::max(level, trigger->maximum + step)
                               : std::min(level, trigger->minimum - step);
        require(session->advanceLevel(level, error), error);
        require(session->arm(error), error);
        waitUntilReady("Before ramp", baseline.sequence);
        ramp.store(true);
        QElapsedTimer event; event.start();
        const qint64 activeStart = activeTime();
        const bool pauseForCapture = cfg.mode == BrownOut::Mode::Track && cfg.captureEnabled;
        qint64 closedAt = -1;
        qint64 lastProgressAt = 0;
        int observedSteps = 0;
        quint64 records = 0;
        quint64 previousSequence = baseline.sequence;
        while (true) {
            check();
            const int completedSteps = steps.load();
            if (completedSteps != observedSteps) {
                observedSteps = completedSteps;
                lastProgressAt = activeTime() - activeStart;
            }
            if (shutdownConfirmed.load() && closedAt < 0) closedAt = event.elapsed();
            if (cfg.mode == BrownOut::Mode::Track || records == 0) {
                BrownOutRecord record;
                // Wait for any voltage write already in flight before touching a
                // stopped record. Keep the pause through transfer, level update and READY.
                if (pauseForCapture) pauseRamp();
                const auto status = pollRecord(record, "Brown Out event", previousSequence);
                if (status == BrownOutRecordStatus::Ready) {
                    require(record.sequence > previousSequence, "Scope returned an old event record");
                    require(!record.clipped, "Brown Out waveform is clipped; adjust vertical settings before retrying");
                    require(firstStep.load(), "Scope triggered before the first voltage step; adjust the trigger level");
                    require(validateRecord(record) == expectedChannels, "Enabled channels changed during Brown Out");
                    require(record.sequence > previousSequence && !record.channels.isEmpty(),
                            "Scope returned an old or empty event record");
                    previousSequence = record.sequence;
                    ++records;
                    const double acquiredLevel = level;
                    for (const auto &v : record.channels) {
                        const auto reference = std::find_if(baseline.channels.cbegin(), baseline.channels.cend(),
                                                           [&](const auto &r) { return r.channel == v.channel; });
                        require(reference->unit == v.unit, "Channel unit changed during Brown Out");
                        auto existing = std::find_if(result.channels.begin(), result.channels.end(),
                                                    [&](const auto &r) { return r.channel == v.channel; });
                        if (existing == result.channels.end()) result.channels.append(v);
                        else {
                            require(existing->unit == v.unit, "Channel unit changed during Brown Out");
                            existing->maximum = std::max(existing->maximum, v.maximum);
                            existing->minimum = std::min(existing->minimum, v.minimum);
                            existing->mean = v.mean; existing->rms = v.rms;
                        }
                        if (v.channel == cfg.triggerChannel && cfg.mode == BrownOut::Mode::Track)
                            level = cfg.rising ? std::max(level + step, v.maximum + step)
                                               : std::min(level - step, v.minimum - step);
                    }
                    result.trials.append({records,
                        cfg.mode == BrownOut::Mode::Track && cfg.rising
                            ? MeasurementDirection::Rise : MeasurementDirection::Fall,
                        records, 1, acquiredLevel, record.channels});
                    if (cfg.mode == BrownOut::Mode::Track &&
                        !(cfg.captureEnabled && closedAt >= 0 && event.elapsed() - closedAt >= cfg.tailMs)) {
                        require(std::isfinite(level), "Trigger level overflow");
                        require(session->advanceLevel(level, error), error);
                        if (pauseForCapture) waitUntilReady("Before ramp resume", previousSequence);
                    }
                }
                if (pauseForCapture) resumeRamp();
            }
            check();
            if (closedAt >= 0 && event.elapsed() - closedAt >= cfg.tailMs) {
                if (records > 0) break;
                require(event.elapsed() - closedAt < cfg.tailMs + acquisitionTimeoutMs,
                        "Output shut down, but no valid event waveform was acquired");
            }
            require(activeTime() - activeStart - lastProgressAt < qint64(cfg.stepIntervalMs) + cfg.timeoutMs +
                                                         cfg.tailMs + acquisitionTimeoutMs,
                    "Brown Out acquisition timed out");
            QThread::msleep(100);
        }
        result.success = true;
        result.summary = QString("%1 valid trials; four values retained per acquisition")
                             .arg(records);
    } catch (...) {
        result.error = exceptionText();
    }
    // Measurements are already copied. A stalled scope restore must not postpone output OFF.
    release.store(true);
    try {
        if (session && !session->finish(error)) { result.cleanupFailed = true; result.error += "\nScope cleanup: " + error; }
    } catch (...) { result.cleanupFailed = true; result.error += "\nScope cleanup: " + exceptionText(); }
    if (controller.joinable()) controller.join();
    if (!outputsReleased.load() && cleanupError.isEmpty()) cleanupError = "Output cleanup did not complete";
    if (!cleanupError.isEmpty()) { result.cleanupFailed = result.outputCleanupFailed = true; result.error += "\nOutput cleanup: " + cleanupError; }
    if (!controlError.isEmpty() && !result.error.contains(controlError)) result.error += "\n" + controlError;
    if (stop.loadAcquire() && result.error.isEmpty()) result.error = "Stopped by user";
    result.success = result.success && !result.cleanupFailed && !failed.load() && !stop.loadAcquire();
    if (shutdownConfirmed.load())
        result.summary += QString("; shutdown Vin setting %1 V; %2 steps; max step delay %3 ms")
                              .arg(finalVin, 0, 'g', 10).arg(steps.load()).arg(maxStepDelay, 0, 'f', 0);
    return result;
}
