#include "turnonthenshortstrategy.h"
QString TurnOnThenShortStrategy::name() const { return "Turn on then short"; }
OscMeasureResult TurnOnThenShortStrategy::execute(IScopeMeasurement *scope, QAtomicInt &stopFlag)
{
    auto context = m_context;
    context.captureRecord = m_captureObserver;
    return runTransientStrategy(TransientKind::TurnOnThenShort, context, scope, stopFlag);
}
