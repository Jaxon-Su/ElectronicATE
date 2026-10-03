#include "transientsearch.h"
#include "transientevent.h"
#include "transientexecution.h"
#include "../measurement/measurementformat.h"
#include "eventsearchbracket.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace TransientDetail {
namespace {
using OscilloscopeAcquisition::AcquisitionResult;
using OscilloscopeAcquisition::VerticalScaleState;

bool valid(double value)
{
    return std::isfinite(value) && std::abs(value) < 1e30;
}

double initialSearchStep(Execution &run, int trigger)
{
    run.check();
    const double scale = run.scope->getChannelScale(trigger);
    run.check();
    const double step = scale * 0.04;
    if (!valid(scale) || !valid(step) || step <= 0)
        throw std::runtime_error("Invalid trigger channel scale for search step");
    run.log(QString("CH%1 Scale=%2 units/div; initial search step=%3 units (0.04 div)")
            .arg(trigger)
            .arg(scale)
            .arg(step));
    return step;
}

// All state below belongs to ONE search direction. Event flows never change
// thresholds or accumulate measurements; they return trial status after any
// required short exit, including trials that did not trigger.
class DirectionSearch {
public:
    DirectionSearch(Execution &run, Event &event, VerticalScaleState &vertical,
                    QMap<int, OscChannelMeasure> &steadyChannels, bool &haveValidEvent,
                    int trigger, bool maximum)
        : run(run), event(event), vertical(vertical), steadyChannels(steadyChannels),
          haveValidEvent(haveValidEvent), trigger(trigger), maximum(maximum),
          direction(maximum ? 1.0 : -1.0) {}

    void execute();

private:
    enum class Probe { Hit, Miss, Baseline, Rescale };
    void captureSteadyReference();
    void arm();
    double readEvent();
    Probe probe(double candidate);
    Probe processBaseline(AcquisitionResult status, double candidate);

    Execution &run;
    Event &event;
    VerticalScaleState &vertical;
    QMap<int, OscChannelMeasure> &steadyChannels;
    bool &haveValidEvent;
    const int trigger;
    const bool maximum;
    const double direction;
    double tolerance = 0;
    double level = 0;
    double referenceFloor = -std::numeric_limits<double>::infinity();
    double observedPeak = -std::numeric_limits<double>::infinity();
    double baselineFloor = -std::numeric_limits<double>::infinity();
    bool referenceInputAvailable = false;
    int acquisitions = 0;
    int scaleAdjustments = 0;
};

} // namespace

void runSearch(Execution &run, Event &event)
{
    QMap<int, OscChannelMeasure> steadyChannels;
    bool haveValidEvent = false;
    run.check();
    const int trigger = run.scope->triggerChannel();
    for (int ch = 1; ch <= run.scope->getTotalChannel(); ++ch) {
        run.check();
        if (run.scope->isChannelEnabled(ch)) {
            OscChannelMeasure value;
            value.channel = ch;
            run.channels[ch] = value;
        }
    }
    if (!run.channels.contains(trigger))
        throw std::runtime_error("Trigger source must be an enabled analog channel");
    OscilloscopeAcquisition::VerticalScaleState vertical;
    vertical.captureRecord = run.ctx.captureRecord;
    vertical.includeRmsMean = false;
    initialSearchStep(run, trigger); // Validate before enabling hardware.
    run.touched = true;
    // Configure the load while input is off. Hardware callbacks use the same executors as Page3.
    event.prepareRun(run);
    QString error;
    run.checked(run.ctx.load(true, error), error);
    for (bool maximum : {true, false}) {
        if ((maximum && run.ctx.target == "MIN") || (!maximum && run.ctx.target == "MAX"))
            continue;
        DirectionSearch(run, event, vertical, steadyChannels, haveValidEvent, trigger, maximum).execute();
    }
    run.check();
    // Merge references only after all selected searches finish normally. Errors never become fallback
    // success. Transient results contain extrema only.
    if (event.needsSteadyReference) {
        for (auto it = run.channels.begin(); it != run.channels.end(); ++it) {
            const auto steady = steadyChannels.value(it.key());
            const bool useMax = !valid(it->maxVal) || steady.maxVal > it->maxVal;
            const bool useMin = !valid(it->minVal) || steady.minVal < it->minVal;
            if (useMax)
                it->maxVal = steady.maxVal;
            if (useMin)
                it->minVal = steady.minVal;
            if (useMax || useMin)
                run.log(QString("CH%1 Steady fallback:%2%3")
                        .arg(it.key())
                        .arg(useMax ? QString(" Max=%1").arg(it->maxVal) : QString())
                        .arg(useMin ? QString(" Min=%1").arg(it->minVal) : QString()));
        }
    }
    // Either edge can capture a complete event containing both Max and Min.
    // A missing opposite edge is informational, not a fabricated NA measurement.
    run.result.success = haveValidEvent || event.needsSteadyReference;
    if (!run.result.success)
        run.result.errorMessage = "No valid event trigger bracket in selected direction(s) (NA)";
}

namespace {
void DirectionSearch::execute()
{
    run.phase.restart();
    tolerance = initialSearchStep(run, trigger);
    level = maximum ? tolerance : -tolerance;
    if (event.needsSteadyReference)
        captureSteadyReference();
    run.configure([&](QString &e) { return run.scope->selectEdgeTrigger(e); });
    run.configure([&](QString &e) {
        return run.scope->setEdgeSlope(maximum ? ScopeTriggerSlope::Rise : ScopeTriggerSlope::Fall, e);
    });
    run.configure([&](QString &e) { return run.scope->setTriggerMode(ScopeTriggerMode::Normal, e); });
    acquisitions = event.needsSteadyReference ? 1 : 0; // Includes AUTO reference.
    baselineFloor = referenceFloor;

    bool noEvent = false;
    EventSearchBracket bracket(direction * level, initialSearchStep(run, trigger));
    for (;;) {
        run.check();
        const auto outcome = probe(bracket.candidate());
        if (outcome == Probe::Rescale) {
            run.log("Vertical scale adjusted; repeat event with a new acquisition");
            tolerance = initialSearchStep(run, trigger);
            bracket.reset(tolerance);
            continue;
        }
        const auto progress =
            outcome == Probe::Baseline
                ? bracket.baseline(baselineFloor)
                : bracket.observe(outcome == Probe::Hit, observedPeak, baselineFloor);
        if (bracket.bounded())
            run.log(QString("Bracket [%1, %2]; width=%3 channel units")
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
        run.log(message);
        return;
    }
    run.log(QString("%1 search converged; returning accumulated measured peaks")
            .arg(maximum ? "Max" : "Min"));
}

void DirectionSearch::captureSteadyReference()
{
    event.prepareReferenceInput(run);
    const auto reference = OscilloscopeAcquisition::captureAutoReference(
        run.scope, run.stop, qMax(0, run.ctx.phaseTimeoutMs - int(run.phase.elapsed())), &vertical);
    run.checked(reference.success, reference.errorMessage);
    for (const auto &value : reference.channels) {
        if (!run.channels.contains(value.channel))
            continue;
        auto &steady = steadyChannels[value.channel];
        steady.channel = value.channel;
        steady.maxVal =
            valid(steady.maxVal) ? std::max(steady.maxVal, value.maxVal) : value.maxVal;
        steady.minVal =
            valid(steady.minVal) ? std::min(steady.minVal, value.minVal) : value.minVal;
    }
    for (auto it = run.channels.cbegin(); it != run.channels.cend(); ++it) {
        const auto found = std::find_if(
            reference.channels.cbegin(), reference.channels.cend(),
            [&](const OscChannelMeasure &value) { return value.channel == it.key(); });
        if (found == reference.channels.cend() || !valid(found->maxVal) || !valid(found->minVal))
            throw std::runtime_error("AUTO reference channel measurement missing");
    }
    tolerance = initialSearchStep(run, trigger);
    bool found = false;
    for (const auto &value : reference.channels)
        if (value.channel == trigger) {
            referenceFloor = maximum ? value.maxVal : -value.minVal;
            level = maximum ? std::max(initialSearchStep(run, trigger), value.maxVal + tolerance)
                            : value.minVal - tolerance;
            found = true;
            run.log(QString("AUTO steady reference Max=%1 Min=%2; initial Level=%3 channel units")
                    .arg(value.maxVal)
                    .arg(value.minVal)
                    .arg(level));
        }
    if (!found)
        throw std::runtime_error("AUTO reference trigger channel missing");
    referenceInputAvailable = true;
}

void DirectionSearch::arm()
{
    run.check();
    if (++acquisitions > run.ctx.maxIterations)
        throw std::runtime_error("Transient acquisition iteration limit reached");
    run.log(QString("%1: arm Level=%2 channel units").arg(maximum ? "Max" : "Min").arg(level));
    run.configure([&](QString &e) { return run.scope->setChannelTriggerLevel(trigger, level, e); });
    if (!run.scope->beginSingleAcquisition())
        throw std::runtime_error("Failed to start single acquisition");
    run.check();
}

double DirectionSearch::readEvent()
{
    double peak = std::numeric_limits<double>::quiet_NaN();
    for (auto it = run.channels.begin(); it != run.channels.end(); ++it) {
        run.check();
        if (run.scope->isClipping(it.key()))
            throw std::runtime_error("Event waveform clipped; adjust channel scale");
        auto read = [&](ScopeMeasurement type) {
            run.check();
            const double v = OscilloscopeAcquisition::readMeasurementChecked(run.scope, it.key(), type);
            run.check();
            return valid(v) ? v : std::numeric_limits<double>::quiet_NaN();
        };
        const double high = read(ScopeMeasurement::Maximum), low = read(ScopeMeasurement::Minimum);
        if (valid(high))
            it->maxVal = valid(it->maxVal) ? std::max(it->maxVal, high) : high;
        if (valid(low))
            it->minVal = valid(it->minVal) ? std::min(it->minVal, low) : low;
        run.log(QString("CH%1 event Max=%2 Min=%3; recorded Max=%4 Min=%5")
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
}

DirectionSearch::Probe DirectionSearch::probe(double candidate)
{
    if (!valid(candidate))
        throw std::runtime_error("Invalid search threshold");
    level = direction * candidate;
    bool hit = false;
    for (int trial = 0; trial < run.ctx.trialsPerLevel; ++trial) {
        run.check();
        run.log(QString("Trial %1/%2").arg(trial + 1).arg(run.ctx.trialsPerLevel));
        // Only the first trial after AUTO reference reuses the powered input.
        // Search consumes this state; event implementations cannot modify it.
        const auto input = referenceInputAvailable ? TrialInput::ReuseReference : TrialInput::PrepareFresh;
        referenceInputAvailable = false;
        const auto record = event.acquireTrial(run, [this] { arm(); }, input);
        if (record.origin == RecordOrigin::Baseline)
            return processBaseline(record.status, candidate);
        const auto eventStatus = record.status;
        if (OscilloscopeAcquisition::hasCompletedRecord(eventStatus)) {
            if (OscilloscopeAcquisition::expandClippedChannels(run.scope, run.stop, scaleAdjustments,
                                                             &vertical, "Measurement",
                                                             eventStatus == AcquisitionResult::CompletedAfterTimeout)) {
                run.configure([&](QString &e) { return run.scope->stopAcquisition(e); });
                return Probe::Rescale;
            }
            observedPeak = std::max(observedPeak, direction * readEvent());
            hit = true;
        }
        run.configure([&](QString &e) { return run.scope->stopAcquisition(e); });
    }
    // One hit establishes the lower edge; only all misses establish the upper edge.
    run.log(hit ? "Level: event captured" : "Level: all trials missed");
    return hit ? Probe::Hit : Probe::Miss;
}

DirectionSearch::Probe DirectionSearch::processBaseline(AcquisitionResult status, double candidate)
{
    if (OscilloscopeAcquisition::expandClippedChannels(run.scope, run.stop, scaleAdjustments,
                                                     &vertical, "Baseline",
                                                     status == AcquisitionResult::CompletedAfterTimeout)) {
        run.configure([&](QString &e) { return run.scope->stopAcquisition(e); });
        return Probe::Rescale;
    }
    if (run.scope->isClipping(trigger))
        throw std::runtime_error("Baseline waveform clipped; adjust channel scale");
    const double peak =
        OscilloscopeAcquisition::readMeasurementChecked(
            run.scope, trigger, maximum ? ScopeMeasurement::Maximum : ScopeMeasurement::Minimum);
    run.check();
    if (!valid(peak))
        throw std::runtime_error("Baseline measurement: NA");
    baselineFloor = std::max(candidate, direction * peak);
    run.configure([&](QString &e) { return run.scope->stopAcquisition(e); });
    run.log("Baseline trigger excluded from event results");
    return Probe::Baseline;
}
} // namespace
} // namespace TransientDetail
