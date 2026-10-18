#include "testrunservice.h"
#include "oscilloscope.h"
#include <QThread>
#include <stdexcept>
#include <cmath>

namespace {
QString number(double value) { return QString::number(value, 'g', 15); }
QString requestedHorizontal(const OscilloscopeSettings::Horizontal &h)
{
    QStringList fields;
    if (h.mode) fields << "Mode=" + *h.mode;
    if (h.adjustment)
        fields << (*h.adjustment == "RECORDLENGTH" ? "Inputs=Time Scale + Sample Rate" : "Inputs=Sample Rate + RL");
    if (h.secondsPerDivision) fields << "Time Scale=" + number(*h.secondsPerDivision) + " s/div";
    if (h.sampleRate) fields << "Sample Rate=" + number(*h.sampleRate) + " S/s";
    if (h.recordLength) fields << "RL=" + number(*h.recordLength) + " samples";
    return fields.join("; ");
}
bool adjusted(const OscilloscopeSettings::Horizontal &requested,
              const OscilloscopeConfigurationResult::Horizontal &actual)
{
    auto differs = [](const std::optional<double> &wanted, double value) {
        return wanted && std::abs(*wanted - value) > std::abs(*wanted) * 1e-6;
    };
    return (requested.mode && *requested.mode != actual.mode) ||
           (requested.adjustment && *requested.adjustment != actual.adjustment) ||
           differs(requested.secondsPerDivision, actual.secondsPerDivision) ||
           differs(requested.sampleRate, actual.sampleRate) ||
           (requested.recordLength && *requested.recordLength != actual.recordLength);
}
}

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
    const QString requested = requestedHorizontal(cfg.configuration.horizontal);
    if (!requested.isEmpty())
        emit logMessage("  Write Oscilloscope requested: " + requested);
    OscilloscopeConfigurationResult result;
    if (!configuration->applySettings(cfg.configuration, m_stopRequested, error, &result))
        return failTask("Write Oscilloscope: " + error);
    if (m_stopRequested.loadAcquire())
        return failTask("Stopped by user");
    if (!requested.isEmpty() && !result.horizontal)
        return failTask("Write Oscilloscope: missing horizontal readback");
    bool wasAdjusted = false;
    if (result.horizontal) {
        const auto &actual = *result.horizontal;
        const QString mode = actual.mode == "MANUAL"
            ? actual.mode + (actual.adjustment == "RECORDLENGTH" ? " (Inputs=Time Scale + Sample Rate)" : " (Inputs=Sample Rate + RL)")
            : actual.mode;
        emit logMessage(QString("  Write Oscilloscope actual: Mode=%1; Time Scale=%2 s/div; "
                                "Sample Rate=%3 S/s; RL=%4 samples; Divisions=%5")
                            .arg(mode, number(actual.secondsPerDivision), number(actual.sampleRate),
                                 number(actual.recordLength), number(actual.divisions)));
        wasAdjusted = adjusted(cfg.configuration.horizontal, actual);
        if (wasAdjusted)
            emit logMessage("  Write Oscilloscope: adjusted by instrument; see requested and actual values above.");
    }
    m_actionResults << (wasAdjusted ? "Oscilloscope settings applied (adjusted)" : "Oscilloscope settings applied");
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
            const auto result = m_dependencies.instruments.relay(m_context.instrumentSetup, m_context.relays,
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
        const auto result = m_dependencies.instruments.relay(m_context.instrumentSetup, m_context.relays, relayIndex,
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
