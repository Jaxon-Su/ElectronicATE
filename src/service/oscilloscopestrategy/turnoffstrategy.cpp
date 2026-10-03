#include "turnoffstrategy.h"
QString TurnOffStrategy::name() const { return "Turn off"; }
OscMeasureResult TurnOffStrategy::execute(IScopeMeasurement *scope, QAtomicInt &stopFlag)
{
    auto context = m_context;
    context.captureRecord = m_captureObserver;
    return runTransientStrategy(TransientKind::TurnOff, context, scope, stopFlag);
}
