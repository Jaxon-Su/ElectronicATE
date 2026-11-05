#include "testrunservice.h"
#include "measurementresultmapping.h"
#include "oscilloscope.h"
#include "strategywaveformcapture.h"
#include <QScopeGuard>

bool TestRunService::executeBrownOut(const BrownOut::Settings &settings)
{
    if (!m_context.scope) return failTask("Brown Out: oscilloscope is unavailable");
    emit logMessage(settings.mode == BrownOut::Mode::FixedFall
                        ? "Brown Out: Run + Single, wait 3000 ms before READY, fixed Fall threshold, one voltage descent"
                        : settings.captureEnabled
                            ? "Brown Out: Normal + repeated Single; wait 3000 ms after each Single before READY; pause voltage descent during read/save/rearm; resume after READY; Vout monitoring continues"
                            : "Brown Out: Normal + Run, wait 3000 ms before initial READY, track new records during one voltage descent");
    auto capture = prepareStrategyCapture(settings.captureEnabled, settings.captureSettings, settings.inputs, "Brown Out");
    auto publish = qScopeGuard([&] { publishStrategyCapture(capture); });
    BrownOutMeasurementRequest request{settings, *m_context.scope,
                                       m_brownOutFactories.value(m_currentTaskIndex), {}};
    request.log = [this](const QString &text) { emit logMessage("Brown Out: " + text); };
    if (capture) request.capture = [&, capture](const ScopeCaptureRecord &record) {
        capture->record(m_context.scope, record,
                        {settings.load, m_context.loads[settings.load].label, LoadConditionKind::Static});
    };
    auto strategy = m_dependencies.strategy(request);
    if (!strategy) return failTask("Brown Out: missing strategy");
    if (!turnOffUnselectedInputs(settings.inputs)) return false;
    // Brown Out owns its input transports on the controller thread. Its result
    // reports output cleanup separately from scope restoration.
    if (settings.inputs.ac >= 0) m_unreleasedOutputs.insert(TableKind::Input);
    if (settings.inputs.dc >= 0) m_unreleasedOutputs.insert(TableKind::Dc);
    m_unreleasedOutputs.insert(TableKind::Load);
    const auto result = strategy->execute(m_stopRequested);
    publishStrategyCapture(capture);
    publish.dismiss();
    if (!result.outputCleanupFailed) {
        if (settings.inputs.ac >= 0) m_unreleasedOutputs.remove(TableKind::Input);
        if (settings.inputs.dc >= 0) m_unreleasedOutputs.remove(TableKind::Dc);
        m_unreleasedOutputs.remove(TableKind::Load);
        m_unreleasedOutputs.remove(TableKind::DyLoad);
    }
    const bool success = result.success && !result.cleanupFailed;
    m_attemptResult.trials = result.trials;
    m_attemptResult.channels = representativeChannels(result.trials);
    m_actionResults << result.summary;
    if (!success)
        failTask(result.error.isEmpty() ? "Brown Out failed" : result.error);
    if (result.cleanupFailed) m_cleanupFailed = true;
    m_attemptResult.summary = m_actionResults.join('\n');
    emit measurementReady(m_currentTaskIndex, m_attemptResult, success);
    return success;
}
