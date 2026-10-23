#include "steadystatesearchstrategy.h"
#include "steadysearch.h"
#include <QScopeGuard>

SteadyStateSearchStrategy::SteadyStateSearchStrategy(int timeoutMs, int phaseTimeoutMs, int maxIterations,
                                             const QString &target, VerticalScaleOptions verticalScale)
    : m_target(target.trimmed().toUpper()), m_timeoutMs(qMax(1, timeoutMs)),
      m_phaseTimeoutMs(qMax(1, phaseTimeoutMs)), m_maxIterations(qMax(1, maxIterations)),
      m_verticalScale(std::move(verticalScale))
{
}

QString SteadyStateSearchStrategy::name() const { return "NormalTrigger(" + m_target + ")"; }

OscMeasureResult SteadyStateSearchStrategy::execute(IScopeMeasurement *scope, QAtomicInt &stopFlag)
{
    // 驗證輸入；success 預設 false，只有所選階段都完成才設為 true。
    OscMeasureResult result;
    if (m_target != "BOTH" && m_target != "MAX" && m_target != "MIN") {
        result.errorMessage = "Invalid measurement target";
        return result;
    }
    if (!scope) {
        result.errorMessage = "No oscilloscope available";
        return result;
    }

    // 所有 return／例外離開都嘗試 STOP；清理例外不可覆蓋原始錯誤。
    auto cleanup = qScopeGuard([scope] {
        try {
            scope->stop();
        } catch (...) {
            // 清理不能拋出第二個例外。
        }
    });
    SteadyDetail::Search search(scope, stopFlag, m_target, m_timeoutMs, m_phaseTimeoutMs,
                                m_maxIterations, m_captureObserver, m_verticalScale);
    return search.execute();
}
