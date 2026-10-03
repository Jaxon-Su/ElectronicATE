#include "transientevent.h"
#include "transientexecution.h"

namespace TransientDetail {
namespace {
class TurnOnEvent final : public Event {
public:
    TurnOnEvent() : Event(ShortProtection::NotRequired, ReferenceMode::None) {}
    void prepareRun(Execution &run) override { run.power(false); }
    void prepareReferenceInput(Execution &) override {} // Turn-on has no steady reference.

    TrialRecord acquireTrial(Execution &run, const std::function<void()> &arm,
                             TrialInput) override
    {
        run.power(false);
        run.delay(run.ctx.powerOffMs);
        run.relays(false, false);
        run.relays(false, true);
        run.delay(run.ctx.dischargeMs);
        run.relays(false, false);

        arm();
        run.requireReady();
        run.power(true);
        return {run.waitForEvent(), RecordOrigin::Event};
    }
};
}
std::unique_ptr<Event> makeTurnOnEvent() { return std::make_unique<TurnOnEvent>(); }
} // namespace TransientDetail
