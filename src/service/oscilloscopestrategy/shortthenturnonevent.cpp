#include "transientevent.h"
#include "transientexecution.h"

namespace TransientDetail {
namespace {
class ShortThenTurnOnEvent final : public Event {
public:
    ShortThenTurnOnEvent() : Event(ShortProtection::Required, ReferenceMode::None) {}
    void prepareRun(Execution &run) override { run.requireShortExit(); }
    void prepareReferenceInput(Execution &) override {} // Event starts with input OFF.

    TrialRecord acquireTrial(Execution &run, const std::function<void()> &arm,
                             TrialInput) override
    {
        run.requireShortExit();
        run.relays(true, false);
        arm();
        run.waitAfterSingle();
        if (!run.waitForPowerOnReady())
            return {OscilloscopeAcquisition::AcquisitionResult::Completed, RecordOrigin::BeforeInput};
        run.power(true);

        const auto status = run.waitForEvent(); // Completed callback exits before transfer.
        run.requireShortExit(); // Also exit on MISS; an earlier failure stays latched.
        return {status, RecordOrigin::Event};
    }
};
}
std::unique_ptr<Event> makeShortThenTurnOnEvent()
{
    return std::make_unique<ShortThenTurnOnEvent>();
}
} // namespace TransientDetail
