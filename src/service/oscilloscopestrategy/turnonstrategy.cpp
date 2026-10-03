#include "turnonstrategy.h"
QString TurnOnStrategy::name() const { return "Turn on"; }
OscMeasureResult TurnOnStrategy::execute(IScopeMeasurement *scope, QAtomicInt &stopFlag)
{
    auto context = m_context;
    context.captureRecord = m_captureObserver;
    return runTransientStrategy(TransientKind::TurnOn, context, scope, stopFlag);
}
