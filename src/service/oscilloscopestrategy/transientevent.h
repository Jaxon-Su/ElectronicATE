#pragma once
#include "oscilloscopeacquisition.h"
#include <functional>
#include <memory>

namespace TransientDetail {
class Execution;

enum class ShortProtection { NotRequired, Required };
enum class ReferenceMode { None, Steady };
enum class TrialInput { PrepareFresh, ReuseReference };
enum class RecordOrigin { Event, Baseline };

struct TrialRecord {
    OscilloscopeAcquisition::AcquisitionResult status;
    RecordOrigin origin = RecordOrigin::Event;
};

// One Event per run; acquireTrial executes one attempt at one search level.
// Event owns output timing; search owns thresholds, results and reference reuse.
// A returned short-circuit trial has already exited short, including on MISS.
class Event {
public:
    Event(ShortProtection protection, ReferenceMode reference)
        : needsShortProtection(protection == ShortProtection::Required),
          needsSteadyReference(reference == ReferenceMode::Steady) {}
    virtual ~Event() = default;

    const bool needsShortProtection;
    const bool needsSteadyReference;

    // Called once before Load ON.
    virtual void prepareRun(Execution &run) = 0;
    // Called once per direction when needsSteadyReference; search captures afterwards.
    virtual void prepareReferenceInput(Execution &run) = 0;
    // arm is synchronous and must not be retained. Input reuse is read-only here.
    virtual TrialRecord acquireTrial(Execution &run, const std::function<void()> &arm,
                                     TrialInput input) = 0;
};

std::unique_ptr<Event> makeTurnOnEvent();
std::unique_ptr<Event> makeTurnOffEvent(ReferenceMode reference = ReferenceMode::Steady);
std::unique_ptr<Event> makeShortThenTurnOnEvent();
std::unique_ptr<Event> makeTurnOnThenShortEvent();
} // namespace TransientDetail
