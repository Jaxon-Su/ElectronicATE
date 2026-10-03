#include "transientevent.h"
#include "transientexecution.h"

namespace TransientDetail {
namespace {
class TurnOnThenShortEvent final : public Event {
public:
    TurnOnThenShortEvent() : Event(ShortProtection::Required, ReferenceMode::Steady) {}
    void prepareRun(Execution &run) override { run.requireShortExit(); }

    void prepareReferenceInput(Execution &run) override
    {
        run.requireShortExit();
        run.power(true);
        run.delay(run.ctx.settleMs);
    }

    TrialRecord acquireTrial(Execution &run, const std::function<void()> &arm,
                             TrialInput input) override
    {
        // Reuse AUTO reference power only once; later trials prepare a new event.
        if (input == TrialInput::PrepareFresh) {
            run.requireShortExit();
            run.power(true);
            run.log(QString("Settle %1 ms").arg(run.ctx.settleMs));
            run.delay(run.ctx.settleMs);
        }
        arm();
        const auto baseline = run.waitForBaseline();
        if (OscilloscopeAcquisition::hasCompletedRecord(baseline)) {
            run.requireShortExit(); // Baseline must be safe before diagnostic Capture.
            return {baseline, RecordOrigin::Baseline};
        }

        run.log("Apply short; wait for event");
        run.requireReady();
        run.relays(true, false);
        const auto status = run.waitForEvent();
        run.requireShortExit(); // Covers MISS as well as completed records.
        return {status, RecordOrigin::Event};
    }
};
}
std::unique_ptr<Event> makeTurnOnThenShortEvent()
{
    return std::make_unique<TurnOnThenShortEvent>();
}
} // namespace TransientDetail
