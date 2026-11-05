#include "transientevent.h"
#include "transientexecution.h"

namespace TransientDetail {
namespace {
class TurnOffEvent final : public Event {
public:
    explicit TurnOffEvent(ReferenceMode reference)
        : Event(ShortProtection::NotRequired, reference) {}
    void prepareRun(Execution &run) override
    {
        run.diagnosticStage("Scope STOP");
        run.log("Scope preparation: STOP");
        run.configure([&](QString &error) {
            if (run.scope->stopAcquisition(error)) return true;
            error.prepend("Scope preparation STOP: ");
            return false;
        });
        run.diagnostic("complete");
        run.diagnosticStage("Scope NORMAL");
        run.log("Scope preparation: NORMAL");
        run.configure([&](QString &error) {
            if (run.scope->setTriggerMode(ScopeTriggerMode::Normal, error)) return true;
            error.prepend("Scope preparation NORMAL: ");
            return false;
        });
        run.diagnostic("complete");
        run.power(false);
    }

    void prepareReferenceInput(Execution &run) override
    {
        prepareInputOff(run);
        run.power(true);
        run.diagnosticDelay(run.ctx.settleMs, "Initial Settle");
    }

    TrialRecord acquireTrial(Execution &run, const std::function<void()> &arm,
                             TrialInput input) override
    {
        // The first trial reuses the already-powered AUTO reference.
        if (input == TrialInput::PrepareFresh) {
            prepareInputOff(run);
            run.power(true);
            run.log(QString("Settle %1 ms").arg(run.ctx.settleMs));
            run.diagnosticDelay(run.ctx.settleMs, "Trial Settle");
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
        run.diagnosticDelay(run.ctx.powerOffMs, "Power-off wait");
        run.relays(false, false);
    }
};
}
std::unique_ptr<Event> makeTurnOffEvent(ReferenceMode reference)
{
    return std::make_unique<TurnOffEvent>(reference);
}
} // namespace TransientDetail
