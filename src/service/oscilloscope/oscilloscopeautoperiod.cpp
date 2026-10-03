#include "oscilloscopeautoperiod.h"
#include "../oscilloscopestrategy/oscilloscopeacquisition.h"
#include "iscopemeasurement.h"
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <exception>

bool adjustOscilloscopeAutoPeriod(IScopeAutoPeriod *scope, QAtomicInt &stop, QString &error)
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
    auto cleanup = [&] { return session->restore(success && !stop.loadAcquire(), error); };
    auto guard = qScopeGuard([&] { cleanup(); });
    OscilloscopeAcquisition::VerticalScaleState vertical;
    int scaleAdjustments = 0;
    int periodAttempts = 0;
    int stableCaptures = 0;
    int timeScaleChanges = 0;
    double stablePeriod = 0;
    bool adjusted = false;
    auto agrees = [](double a, double b) { return std::abs(a - b) <= 0.05 * std::max(a, b); };
    while (periodAttempts < 24 && timeScaleChanges < 6)
    {
        if (!ready())
            return false;
        double scale = 0;
        if (!session->readTimeScale(scale, error))
            return false;
        const double windowMs = scale * divisions * 1000;
        const double requiredCaptureMs = std::max(10000.0, 2 * windowMs + 5000);
        if (!positive(scale) || !std::isfinite(windowMs) || requiredCaptureMs > 300000)
        {
            error = "Auto Period: invalid or excessive time window (capture budget "
                    "exceeds 5 min)";
            return false;
        }
        const int captureTimeoutMs = int(std::ceil(requiredCaptureMs));
        const int measurementTimeoutMs = int(std::ceil(std::clamp(5000 + windowMs * 0.25, 5000.0, 30000.0)));
        // Configure PERIOD before the next fresh acquisition, including after
        // clipping retries.
        if (!session->prepareRecord(error))
            return false;
        if (!scope->beginSingleAcquisition())
        {
            error = "Auto Period: single acquisition failed";
            return false;
        }
        const auto capture = OscilloscopeAcquisition::waitForCapture(
            scope, stop, qMin(captureTimeoutMs, qMax(0, totalLimitMs - int(total.elapsed()))));
        if (capture != OscilloscopeAcquisition::AcquisitionResult::Completed)
        {
            error = capture == OscilloscopeAcquisition::AcquisitionResult::Cancelled
                        ? "Auto Period: stopped by user"
                    : capture == OscilloscopeAcquisition::AcquisitionResult::CommError
                        ? "Auto Period: acquisition communication error"
                        : QString("Auto Period: no completed acquisition within %1 s "
                                  "(window %2 s)")
                              .arg(captureTimeoutMs / 1000.0)
                              .arg(windowMs / 1000.0);
            return false;
        }
        // Discard clipped records before PERIOD; vertical retries have their own
        // limit.
        try
        {
            if (!ready())
                return false;
            if (OscilloscopeAcquisition::expandClippedChannels(scope, stop, scaleAdjustments, &vertical))
            {
                stableCaptures = 0;
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
        if (measurementReady)
        {
            stableCaptures = stableCaptures > 0 && agrees(period, stablePeriod) ? stableCaptures + 1 : 1;
            if (stableCaptures == 1)
                stablePeriod = period;
            if (stableCaptures < 3)
                continue;
        }
        else
        {
            stableCaptures = 0;
        }
        const double cycles = scale * divisions / period;
        if (adjusted && measurementReady && cycles >= 3 && cycles <= 8)
        {
            success = true;
            break;
        }
        // No measurable period: expand the initial range, with bounded retries.
        const double target = measurementReady ? 5 * period / divisions : scale * 10;
        if (!positive(target))
        {
            error = "Auto Period: period unavailable (NA)";
            return false;
        }
        if (!session->setTimeScale(target, error))
            return false;
        // Do not start the verification acquisition while the old timebase is still
        // active.
        QElapsedTimer readback;
        readback.start();
        bool applied = false;
        do
        {
            double actual = 0;
            if (!session->readTimeScale(actual, error))
                return false;
            if (positive(actual) && std::abs(actual - target) <= target * 0.01)
            {
                applied = true;
                break;
            }
            QThread::msleep(20);
        } while (readback.elapsed() < 5000 && ready());
        if (!applied)
        {
            error = "Auto Period: requested time scale was not applied";
            return false;
        }
        ++timeScaleChanges;
        adjusted = true;
        stableCaptures = 0;
    }
    if (!success)
        error = "Auto Period: could not verify stable PERIOD and 3-8 cycles (NA)";
    if (!ready())
        success = false;
    const bool cleaned = cleanup();
    guard.dismiss();
    return success && cleaned && ready();
}
