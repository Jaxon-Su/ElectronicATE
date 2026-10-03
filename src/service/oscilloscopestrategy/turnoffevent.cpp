#include "transientevent.h"
#include "transientexecution.h"

namespace TransientDetail {
namespace {
class TurnOffEvent final : public Event {
public:
    explicit TurnOffEvent(ReferenceMode reference)
        : Event(ShortProtection::NotRequired, reference) {}
    void prepareRun(Execution &run) override { run.power(false); }

    void prepareReferenceInput(Execution &run) override
    {
        prepareInputOff(run);
        run.power(true);
        run.delay(run.ctx.settleMs);
    }

    TrialRecord acquireTrial(Execution &run, const std::function<void()> &arm,
                             TrialInput input) override
    {
        // The first trial reuses the already-powered AUTO reference.
        if (input == TrialInput::PrepareFresh) {
            prepareInputOff(run);
            run.power(true);
            run.log(QString("Settle %1 ms").arg(run.ctx.settleMs));
            run.delay(run.ctx.settleMs);
        }
        arm();
        const auto baseline = run.waitForBaseline();
        if (OscilloscopeAcquisition::hasCompletedRecord(baseline))
            return {baseline, RecordOrigin::Baseline};

        run.log("Power off; wait for event");
        run.requireReady();
        run.power(false);
        return {run.waitForEvent(), RecordOrigin::Event};
    }

private:
    static void prepareInputOff(Execution &run)
    {
        run.power(false);
        run.delay(run.ctx.powerOffMs);
        run.relays(false, false);
    }
};
}
std::unique_ptr<Event> makeTurnOffEvent(ReferenceMode reference)
{
    return std::make_unique<TurnOffEvent>(reference);
}
} // namespace TransientDetail
