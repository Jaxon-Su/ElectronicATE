#include "oscilloscopeautoperiod.h"
#include "../oscilloscopestrategy/oscilloscopeacquisition.h"
#include "iscopemeasurement.h"
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <exception>

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
        double scale = 0;
        if (!session->readTimeScale(scale, error))
            return false;
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
        const int measurementTimeoutMs = int(std::ceil(std::clamp(5000 + windowMs * 0.25, 5000.0, 30000.0)));
        // Configure PERIOD before the next fresh acquisition, including after
        // clipping retries.
        if (!session->prepareRecord(error))
            return false;
        const auto capture = OscilloscopeAcquisition::acquireAutoRecord(
            scope, stop, qMax(0, totalLimitMs - int(total.elapsed())), error, log);
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
            if (OscilloscopeAcquisition::expandVerticalRange(scope, stop, scaleAdjustments, &vertical, "Auto Period",
                    capture == OscilloscopeAcquisition::AcquisitionResult::CompletedAfterTimeout,
                    QDeadlineTimer(qMax(0, totalLimitMs - int(total.elapsed())))))
            {
                stableCaptures = 0;
                inWindowCaptures = 0;
                continue;
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
        double previousReading = 0;
        int consistentReads = 0;
        bool measurementReady = false;
        do
        {
            if (!session->readPeriod(period, error))
                return false;
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
            QThread::msleep(50);
        } while (measurement.elapsed() < measurementTimeoutMs && ready());
        if (!ready() || !session->readTimeScale(scale, error))
            return false;
        if (!positive(scale))
        {
            error = "Auto Period: invalid time scale";
            return false;
        }
        const double cycles = scale * divisions / period;
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
            if (!session->readTimeScale(actual, error))
                return false;
            if (positive(actual) && (std::abs(actual - target) <= target * 0.01 ||
                                    (measurementReady && inWindow(actual * divisions / period))))
            {
                applied = true;
                break;
            }
            QThread::msleep(20);
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
        stableCaptures = 0;
        inWindowCaptures = 0;
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
