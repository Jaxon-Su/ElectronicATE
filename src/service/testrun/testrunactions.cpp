#include "testrunservice.h"
#include "oscilloscope.h"
#include <QThread>
#include <stdexcept>

bool TestRunService::executeDelay(int totalMs)
{
    if (!TaskSettingRules::settle.accepts(totalMs))
        return failTask("Invalid delay duration");
    const double seconds = totalMs / 1000.0;
    emit logMessage(QString("  Delay: %1 s").arg(seconds, 0, 'f', 3));

    const int chunkMs = 100;
    int elapsed = 0;
    while (elapsed < totalMs) {
        if (m_stopRequested.loadAcquire())
            return false;
        const int wait = qMin(chunkMs, totalMs - elapsed);
        QThread::msleep(static_cast<unsigned long>(wait));
        elapsed += wait;
    }
    if (m_stopRequested.loadAcquire())
        return false;
    m_actionResults << QString("Waited %1 s").arg(seconds, 0, 'g', 10);
    return true;
}

bool TestRunService::executeWriteOscilloscope(const TaskSettings::WriteScope &cfg)
{
    QString error;
    if (m_stopRequested.loadAcquire())
        return failTask("Stopped by user");
    IScopeConfiguration *configuration = m_context.scope;
    if (!configuration->applySettings(cfg.configuration, m_stopRequested, error))
        return failTask("Write Oscilloscope: " + error);
    if (m_stopRequested.loadAcquire())
        return failTask("Stopped by user");
    m_actionResults << "Oscilloscope settings applied";
    emit logMessage("  Write Oscilloscope: done");
    return true;
}

bool TestRunService::executeRelay(int relayIndex)
{
    if (m_stopRequested.loadAcquire())
        return false;
    if (relayIndex < 0 || relayIndex >= m_context.relays.size())
        return failTask("Selected relay row is out of range");
    auto release = [this, relayIndex, taskIndex = m_currentTaskIndex] {
        if (m_shortCircuit.active()) {
            m_cleanupFailed = true;
            const QString error = "Relay OFF blocked: retained short circuit requires Input OFF and discharge";
            recordCleanupFailure(taskIndex, error);
            emit taskResultChanged(taskIndex, "Failed: " + error);
            emit taskStatusChanged(taskIndex, TaskStatus::Fail);
            return;
        }
        QString error;
        try {
            const auto result = m_dependencies.instruments.relay(m_context.page1, m_context.relays,
                                                                 relayIndex, RelayAction::RelayOff);
            if (result.success)
                return;
            error = result.errorMessage;
        } catch (const std::exception &ex) {
            error = QString::fromUtf8(ex.what());
        } catch (...) {
            error = "unexpected relay cleanup exception";
        }
        m_cleanupFailed = true;
        recordCleanupFailure(taskIndex, "Cleanup failed: Relay OFF - " + error);
        emit taskResultChanged(taskIndex, "Failed: Relay cleanup — " + error);
        emit taskStatusChanged(taskIndex, TaskStatus::Fail);
    };
    try {
        const auto result = m_dependencies.instruments.relay(m_context.page1, m_context.relays, relayIndex,
                                                             RelayAction::RelayOn);
        if (!result.success)
            throw std::runtime_error(result.errorMessage.toStdString());
        if (m_stopRequested.loadAcquire())
            throw std::runtime_error("Stopped by user");
        m_pendingCleanups.append(release);
        const QString label = m_context.relays[relayIndex].label.trimmed();
        m_actionResults << "Applied: " +
                               (label.isEmpty() ? QString("Relay condition %1").arg(relayIndex + 1) : label);
        emit logMessage(QString("Relay: done (idx=%1)").arg(relayIndex));
        return true;
    } catch (const std::exception &ex) {
        failTask("Relay: " + QString::fromUtf8(ex.what()));
    } catch (...) {
        failTask("Relay: unexpected execution error");
    }
    // Skip cleanup only for a failed read-only attempt with no earlier output
    // obligation. Successful tasks retain their normal end-of-Run OFF.
    if (m_unreleasedOutputs.contains(TableKind::Relay) || m_shortCircuit.active())
        release();
    return false;
}
