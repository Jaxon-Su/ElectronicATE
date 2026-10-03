#include "brownoutstrategy.h"
#include "brownoutrunner.h"
#include <utility>

BrownOutStrategy::BrownOutStrategy(BrownOut::Settings settings, IScopeBrownOut &scope,
                                 BrownOutIoFactory createIo, OscCaptureObserver capture)
    : m_settings(std::move(settings)), m_scope(scope), m_createIo(std::move(createIo)), m_capture(std::move(capture)) {}

QString BrownOutStrategy::name() const { return QStringLiteral("Brown Out"); }

MeasurementResult BrownOutStrategy::execute(QAtomicInt &stop)
{
    return runBrownOut(m_settings, m_scope, m_createIo, stop, m_capture);
}
