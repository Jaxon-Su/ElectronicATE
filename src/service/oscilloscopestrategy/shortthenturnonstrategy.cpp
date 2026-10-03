#include "shortthenturnonstrategy.h"
QString ShortThenTurnOnStrategy::name() const { return "Short then turn on"; }
OscMeasureResult ShortThenTurnOnStrategy::execute(IScopeMeasurement *scope, QAtomicInt &stopFlag)
{
    auto context = m_context;
    context.captureRecord = m_captureObserver;
    return runTransientStrategy(TransientKind::ShortThenTurnOn, context, scope, stopFlag);
}
