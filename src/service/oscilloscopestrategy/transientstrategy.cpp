#include "transientstrategy.h"
#include "transientevent.h"
#include "transientexecution.h"
#include "transientsearch.h"
#include <stdexcept>

namespace {
std::unique_ptr<TransientDetail::Event> selectEvent(TransientKind kind)
{
    using namespace TransientDetail;
    switch (kind) {
    case TransientKind::TurnOn: return makeTurnOnEvent();
    case TransientKind::TurnOff: return makeTurnOffEvent();
    case TransientKind::ShortThenTurnOn: return makeShortThenTurnOnEvent();
    case TransientKind::TurnOnThenShort: return makeTurnOnThenShortEvent();
    }
    // Preserve the former unrecognized-enum path: power-off event, no reference.
    return makeTurnOffEvent(ReferenceMode::None);
}
}

OscMeasureResult runTransientStrategy(TransientKind kind, const TransientContext &ctx,
                                      IScopeMeasurement *scope, QAtomicInt &stop)
{
    OscMeasureResult result;
    if (!scope) {
        result.errorMessage = "No oscilloscope available";
        return result;
    }
    if (!ctx.power || !ctx.load || !ctx.relays || ctx.timeoutMs <= 0 || ctx.phaseTimeoutMs <= 0 ||
        ctx.maxIterations <= 0 || ctx.settleMs < 0 || ctx.dischargeMs < 0 || ctx.powerOffMs < 0 || ctx.singleProcessingMs < 0 ||
        ctx.trialsPerLevel < 1 || ctx.trialsPerLevel > 10 ||
        !QStringList{"MAX", "MIN", "BOTH"}.contains(ctx.target) ||
        !QStringList{"AUTO", "RISING", "FALLING", "BOTH"}.contains(ctx.edge)) {
        result.errorMessage = "Invalid transient strategy settings";
        return result;
    }

    auto event = selectEvent(kind);
    TransientDetail::Execution run(ctx, scope, stop, event->needsShortProtection, kind == TransientKind::TurnOff);
    try {
        TransientDetail::runSearch(run, *event,
                                  kind == TransientKind::TurnOff || kind == TransientKind::TurnOnThenShort);
    } catch (const std::exception &ex) {
        run.result.errorMessage = QString::fromUtf8(ex.what());
        run.diagnostic("failed: " + run.result.errorMessage);
    } catch (...) {
        run.result.errorMessage = "Unexpected transient strategy error";
        run.diagnostic("failed: " + run.result.errorMessage);
    }
    // Explicit cleanup covers normal completion, Stop and every exception.
    return run.finish();
}
