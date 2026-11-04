#include "oscilloscopeautoperiod.h"
#include "../oscilloscopestrategy/oscilloscopeacquisition.h"
#include "../measurement/triggerlevelgrid.h"
#include "iscopemeasurement.h"
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace {
struct PeriodScales {
    double time = 0, channel = 0;
    bool read(IScopeAutoPeriod &scope, IScopePeriodSession &session, QString &error) {
        if (!session.readTimeScale(time, error)) return false;
        channel = scope.getChannelScale(session.triggerChannel());
        error = scope.lastError();
        if (!error.isEmpty()) return false;
        if (std::isfinite(channel) && channel > 0 && channel < 1e20 &&
            std::isfinite(time) && time > 0 && time < 1e30) return true;
        error = "Auto Period: invalid trigger channel or time scale";
        return false;
    }
    bool matches(const PeriodScales &other) const {
        return qFuzzyCompare(time, other.time) && qFuzzyCompare(channel, other.channel);
    }
};

// A narrow/flat reference can be a short time window. Let the caller use its
// existing bounded expansion policy; device errors must stop the measurement.
bool configurePeriodTrigger(IScopeAutoPeriod *scope, IScopePeriodSession &session, double step, QAtomicInt &stop,
                            QDeadlineTimer deadline, QString &error, const OscilloscopeAcquisition::ReferenceLog &log)
{
    const int ch = session.triggerChannel();
    auto check = [&] {
        if (stop.loadAcquire()) throw std::runtime_error("stopped by user");
        if (deadline.hasExpired()) throw std::runtime_error("overall deadline reached");
    };
    auto extremum = [&](ScopeMeasurement type) {
        check();
        return OscilloscopeAcquisition::readReadyMeasurement(scope, stop, ch, type,
                int(qMin<qint64>(5000, deadline.remainingTime())));
    };
    const double maximum = extremum(ScopeMeasurement::Maximum);
    const double minimum = extremum(ScopeMeasurement::Minimum);
    check();
    const TriggerLevelGrid grid(step);
    const bool rise = session.periodSlope() == ScopeTriggerSlope::Rise;
    const QString details = QString("Auto Period: CH%1 %2 Max=%3 Min=%4 Step=%5")
            .arg(ch).arg(rise ? "Rise" : "Fall").arg(maximum,0,'g',12).arg(minimum,0,'g',12).arg(step,0,'g',12);
    if (maximum < minimum) { error = details + "; invalid extrema"; return false; }
    double requested = std::numeric_limits<double>::quiet_NaN();
    if (std::isfinite(minimum) && std::isfinite(maximum) && minimum < maximum) {
        const double inset = (maximum - minimum) * 0.1;
        const double index = rise ? grid.floorIndex(maximum - inset) : grid.ceilIndex(minimum + inset);
        if (grid.floorIndex(minimum) < index && index < grid.ceilIndex(maximum))
            requested = grid.valueAt(index);
    }
    if (!(minimum < requested && requested < maximum)) {
        if (log) log(details + "; no resolvable interior trigger level");
        return false;
    }
    double actual = 0;
    if (!session.applyTriggerLevel(requested, actual, error)) {
        if (error.isEmpty()) error = details + "; could not apply trigger level";
        return false;
    }
    check();
    const QString result = details + QString("; requested Level=%1 actual=%2").arg(requested,0,'g',12).arg(actual,0,'g',12);
    if (!std::isfinite(actual) || actual <= minimum || actual >= maximum || std::abs(actual-requested) > step * .01) {
        error = result + "; trigger level readback mismatch";
        return false;
    }
    if (log) log(result);
    return true;
}
}

bool adjustOscilloscopeAutoPeriod(IScopeAutoPeriod *scope, QAtomicInt &stop, QString &error,
                                  const OscCaptureObserver &capture, const VerticalScaleOptions &options,
                                  const std::function<void(const QString &)> &log)
{
    error.clear();
    QElapsedTimer total;
    total.start();
    constexpr int totalLimitMs = 15 * 60 * 1000;
    auto ready = [&]
    {
        if (stop.loadAcquire())
        {
            error = "Auto Period: stopped by user";
            return false;
        }
        if (total.elapsed() >= totalLimitMs)
        {
            error = "Auto Period: 15 min overall limit reached";
            return false;
        }
        return true;
    };
    if (!ready())
        return false;
    if (!scope)
    {
        error = "Auto Period: no oscilloscope connected";
        return false;
    }
    auto session = scope->beginPeriodSession(ready, error);
    if (!session)
        return false;
    const double divisions = session->horizontalDivisions();
    auto positive = [](double value) { return std::isfinite(value) && value > 0 && value < 1e30; };
    bool success = false;
    auto cleanup = [&] { return session->restore(success && !stop.loadAcquire() && !capture, error); };
    auto guard = qScopeGuard([&] {
        cleanup();
        if (log && !error.isEmpty()) log("Auto Period failed: " + error);
    });
    OscilloscopeAcquisition::VerticalScaleState vertical{capture};
    vertical.options = options;
    int scaleAdjustments = 0;
    int periodAttempts = 0;
    int stableCaptures = 0;
    int inWindowCaptures = 0;
    int timeScaleChanges = 0;
    bool triggerConfigured = false;
    double normalFitTimeScale = 0;
    PeriodScales calibrated;
    auto discardCalibration = [&] {
        triggerConfigured = false;
        stableCaptures = inWindowCaptures = 0;
    };
    double stablePeriod = 0;
    auto agrees = [](double a, double b) { return std::abs(a - b) <= 0.05 * std::max(a, b); };
    // Only absorb floating-point arithmetic at the inclusive 3/8 boundaries.
    auto inWindow = [](double cycles) {
        return std::isfinite(cycles) && cycles >= 3 - 1e-12 && cycles <= 8 + 1e-12;
    };
    auto number = [](double value) { return QString::number(value, 'g', 12); };
    auto progress = [&] {
        return QString("attempt=%1/24 adjustments=%2/6").arg(periodAttempts).arg(timeScaleChanges);
    };
    if (log) log("Auto Period: accept stable 3-8 cycles; adjustment target=5 cycles; "
                 "limits: 24 measurement attempts, 6 adjustments, 900000 ms overall");
    while (periodAttempts < 24)
    {
        if (!ready())
            return false;
        PeriodScales startScales;
        if (!startScales.read(*scope,*session,error) || !ready()) return false;
        const double scale = startScales.time;
        if (!qFuzzyCompare(normalFitTimeScale,scale)) normalFitTimeScale = 0;
        if (triggerConfigured && !calibrated.matches(startScales)) {
            if (log) log("Auto Period: scale changed between records; reacquiring trigger reference");
            discardCalibration();
            if (++periodAttempts >= 24) break;
        }
        IScopePeriodSession::HorizontalContext horizontal;
        if (!session->readHorizontalContext(horizontal, error)) return false;
        if (log) {
            const QString inputs = horizontal.mode == "AUTO" ? "N/A (AUTO)"
                : horizontal.manualInputs == "RECORDLENGTH" ? "Time Scale + Sample Rate"
                : "Sample Rate + Record Length";
            log(QString("Auto Period: %1; Mode=%2; Inputs=%3 (%4); actual=%5 s/div; "
                        "SampleRate=%6 S/s; RecordLength=%7 points")
                    .arg(progress(), horizontal.mode, inputs, horizontal.manualInputs.isEmpty() ? "N/A" : horizontal.manualInputs,
                         number(scale), number(horizontal.sampleRate), number(horizontal.recordLength)));
        }
        const double windowMs = scale * divisions * 1000;
        const double requiredCaptureMs = std::max(10000.0, 2 * windowMs + 5000);
        if (!positive(scale) || !std::isfinite(windowMs) || requiredCaptureMs > 300000)
        {
            error = "Auto Period: invalid or excessive time window (capture budget "
                    "exceeds 5 min)";
            return false;
        }
        // A frozen acquisition can still have pending measurement results.
        // Allow processing time for a full window, but poll so ready values do
        // not incur a fixed delay; the overall deadline remains shared.
        const int measurementTimeoutMs = int(std::ceil(std::clamp(10000 + windowMs, 10000.0, 60000.0)));
        // Configure PERIOD before the next fresh acquisition, including after
        // clipping retries.
        if (triggerConfigured && !session->prepareRecord(error))
            return false;
        const auto capture = OscilloscopeAcquisition::acquireReferenceRecord(
            scope, stop, qMax(0, totalLimitMs - int(total.elapsed())), error, log,
            triggerConfigured ? ScopeTriggerMode::Normal : ScopeTriggerMode::Auto);
        if (!OscilloscopeAcquisition::hasCompletedRecord(capture))
        {
            if (!error.isEmpty()) error.prepend("Auto Period: ");
            else error = capture == OscilloscopeAcquisition::AcquisitionResult::Cancelled
                        ? "Auto Period: stopped by user"
                    : capture == OscilloscopeAcquisition::AcquisitionResult::CommError
                        ? "Auto Period: acquisition communication error"
                        : QString("Auto Period: no completed reference (window %1 s)")
                              .arg(windowMs / 1000.0);
            return false;
        }
        // Discard clipped records before PERIOD; vertical retries have their own
        // limit.
        try
        {
            // A completed record remains traceable even when reading or archiving it
            // reaches the overall limit. Cancellation is checked by the helper.
            vertical.fitDisplay = triggerConfigured || normalFitTimeScale == 0;
            if (OscilloscopeAcquisition::expandVerticalRange(scope, stop, scaleAdjustments, &vertical,
                    triggerConfigured ? "Auto Period" : "Auto Period reference",
                    capture == OscilloscopeAcquisition::AcquisitionResult::CompletedAfterTimeout,
                    QDeadlineTimer(qMax(0, totalLimitMs - int(total.elapsed())))))
            {
                // Margin fitting can move only a display Position or another
                // channel. Recheck the calibrated trigger/time scales at loop
                // entry before deciding whether a new AUTO pilot is necessary.
                if (options.margin) {
                    if (triggerConfigured) normalFitTimeScale = scale;
                    stableCaptures = inWindowCaptures = 0;
                }
                else discardCalibration();
                continue;
            }
            PeriodScales frozen;
            if (!frozen.read(*scope,*session,error) || !ready()) return false;
            if (!startScales.matches(frozen)) {
                if (log) log("Auto Period: scale changed during acquisition; discarding record");
                ++periodAttempts;
                discardCalibration();
                continue;
            }
            if (!triggerConfigured) {
                triggerConfigured = configurePeriodTrigger(scope, *session, frozen.channel / 25, stop,
                        QDeadlineTimer(qMax(0, totalLimitMs - int(total.elapsed()))), error, log);
                if (!error.isEmpty() || !ready()) return false;
                if (triggerConfigured) {
                    calibrated = frozen;
                    continue; // The AUTO pilot never supplies a PERIOD.
                }
                if (!vertical.fitDisplay) {
                    normalFitTimeScale = 0;
                    ++periodAttempts;
                    continue; // Retry an unresolved pilot with display fitting before changing timebase.
                }
            }
        }
        catch (const std::exception &ex)
        {
            error = "Auto Period: " + QString::fromUtf8(ex.what());
            return false;
        }
        catch (...)
        {
            error = "Auto Period: unexpected clipping check failure";
            return false;
        }
        if (!ready())
            return false;
        ++periodAttempts;
        double period = 0;
        QElapsedTimer measurement;
        measurement.start();
        const int measurementLimitMs = qMin(measurementTimeoutMs, qMax(0, totalLimitMs - int(total.elapsed())));
        if (triggerConfigured && log) log(QString("Auto Period: PERIOD wait begin; %1; window=%2 s; wait limit=%3 ms; remaining overall=%4 ms")
            .arg(progress(), number(windowMs / 1000)).arg(measurementLimitMs).arg(qMax(0, totalLimitMs - int(total.elapsed()))));
        double previousReading = 0;
        int consistentReads = 0;
        bool measurementReady = false;
        while (triggerConfigured && measurement.elapsed() < measurementLimitMs)
        {
            if (!ready()) return false;
            if (measurement.elapsed() >= measurementLimitMs) break;
            if (!session->readPeriod(period, error)) {
                error.prepend(QString("Auto Period: PERIOD read failed after %1 ms; %2; ").arg(measurement.elapsed()).arg(progress()));
                return false;
            }
            if (!ready()) return false;
            if (measurement.elapsed() >= measurementLimitMs) {
                if (positive(period)) {
                    error = QString("Auto Period: valid result arrived after PERIOD wait limit; %1; elapsed=%2 ms; limit=%3 ms")
                        .arg(progress()).arg(measurement.elapsed()).arg(measurementLimitMs);
                    return false;
                }
                break;
            }
            // Repeated reads only wait for measurement settling; independent captures
            // below establish consistency. Recreating the badge excludes the previous
            // record's statistics.
            consistentReads = positive(period) && positive(previousReading) && agrees(period, previousReading)
                                  ? consistentReads + 1
                                  : 0;
            previousReading = period;
            if (consistentReads >= 3 && measurement.elapsed() >= 200)
            {
                measurementReady = true;
                break;
            }
            QThread::msleep(static_cast<unsigned long>(qMin<qint64>(50, qMax<qint64>(0, measurementLimitMs - measurement.elapsed()))));
        }
        if (triggerConfigured && log) log(QString("Auto Period: PERIOD wait %1; %2; elapsed=%3 ms; limit=%4 ms; last=%5 s")
            .arg(measurementReady ? "ready" : positive(period) ? "expired (not stable within deadline)" : "expired (NA)", progress())
            .arg(measurement.elapsed()).arg(measurementLimitMs).arg(positive(period) ? number(period) : "NA"));
        PeriodScales measured;
        if (!ready() || !measured.read(*scope,*session,error) || !ready()) return false;
        if (!startScales.matches(measured)) {
            if (log) log("Auto Period: scale changed during measurement; discarding record");
            discardCalibration();
            continue;
        }
        const double cycles = measurementReady ? scale * divisions / period : 0;
        if (measurementReady)
        {
            stableCaptures = stableCaptures > 0 && agrees(period, stablePeriod) ? stableCaptures + 1 : 1;
            if (stableCaptures == 1)
                stablePeriod = period;
            // PERIOD stability permits an out-of-window adjustment; acceptance
            // additionally requires three consecutive records inside the window.
            inWindowCaptures = inWindow(cycles)
                ? (stableCaptures > 1 ? inWindowCaptures + 1 : 1) : 0;
        }
        else
        {
            stableCaptures = 0;
            inWindowCaptures = 0;
        }
        if (log) log(QString("Auto Period: %1; PERIOD=%2 s; actual=%3 s/div; cycles=%4; stable=%5/3; in-window=%6/3")
            .arg(progress(), measurementReady ? number(period) : "NA", number(scale),
                 measurementReady ? number(cycles) : "NA").arg(stableCaptures).arg(inWindowCaptures));
        if (measurementReady && stableCaptures < 3) continue;
        if (measurementReady && inWindow(cycles))
        {
            if (inWindowCaptures < 3) continue;
            if (log) log("Auto Period accepted: stable 3-8 cycles; " + progress());
            success = true;
            break;
        }
        // The last allowed adjustment still gets fresh verification records.
        if (timeScaleChanges >= 6 || periodAttempts >= 24) break;
        // No measurable period: expand the initial range, with bounded retries.
        const double target = measurementReady ? 5 * period / divisions : scale * 10;
        if (!positive(target))
        {
            error = "Auto Period: period unavailable (NA)";
            return false;
        }
        if (log) log(QString("Auto Period: %1; adjustment=%2/6; target=%3 s/div; actual=%4 s/div; %5")
            .arg(progress()).arg(timeScaleChanges + 1).arg(number(target), number(scale),
                measurementReady ? "outside 3-8 cycles" : "PERIOD unavailable"));
        if (!session->setTimeScale(target, error))
            return false;
        ++timeScaleChanges;
        // Do not start the verification acquisition while the old timebase is still
        // active.
        QElapsedTimer readback;
        readback.start();
        bool applied = false;
        double actual = 0;
        do
        {
            if (!ready()) return false;
            if (readback.elapsed() >= 5000) break;
            if (!session->readTimeScale(actual, error))
                return false;
            if (!ready()) return false;
            if (readback.elapsed() >= 5000) break;
            // Without a usable PERIOD, only require real forward progress from
            // a quantized expansion. Fresh records still have to prove 3-8 cycles.
            const bool forwardExpansion = !measurementReady && positive(actual) && actual > scale * 1.01;
            if (positive(actual) && (std::abs(actual - target) <= target * 0.01 ||
                                    (measurementReady && inWindow(actual * divisions / period)) || forwardExpansion))
            {
                applied = true;
                break;
            }
            QThread::msleep(static_cast<unsigned long>(qMin<qint64>(20, qMax<qint64>(0, 5000 - readback.elapsed()))));
        } while (readback.elapsed() < 5000 && ready());
        if (!ready()) return false;
        if (log) log(QString("Auto Period: %1; target=%2 s/div; actual=%3 s/div; %4")
            .arg(progress(), number(target), number(actual),
                 applied ? "readback usable; verify three fresh stable records" : "readback rejected after 5000 ms"));
        if (!applied)
        {
            error = QString("Auto Period: requested time scale was not applied; target=%1 s/div; actual=%2 s/div; %3")
                        .arg(number(target), number(actual), progress());
            return false;
        }
        discardCalibration();
    }
    if (!success)
        error = "Auto Period: could not verify stable PERIOD and 3-8 cycles (NA); " + progress();
    if (!ready())
        success = false;
    const bool cleaned = cleanup();
    guard.dismiss();
    if (log && !error.isEmpty()) log("Auto Period failed: " + error);
    return success && cleaned && ready();
}
