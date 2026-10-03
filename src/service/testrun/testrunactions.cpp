#include "testrunservice.h"
#include "capturefile.h"
#include "../../hardware/communication/transfercancellation.h"
#include "oscilloscope.h"
#include <QDir>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <stdexcept>

bool TestRunService::executeCapture(const QString &task, const TaskSettings::Capture &cfg)
{
    const TransferCancellation transferCancellation(m_stopRequested);
    auto *scope = m_context.scope; // Page5 retains Page3's exclusive scope lease for the Run.
    if (m_stopRequested.loadAcquire())
        return false;
    if (!scope || !scope->isConnected()) {
        return failTask("Capture: oscilloscope is not connected");
    }
    const QString directory = cfg.directory;
    if (directory.isEmpty() || !QDir::isAbsolutePath(directory) || !QDir(directory).exists()) {
        return failTask("Capture: select an existing absolute save directory first");
    }
    const bool all = task.startsWith("All");
    const QString format = all ? task.mid(3) : task;
    QList<int> channels;
    if (format != "PNG") {
        const int total = scope->getTotalChannel();
        if (all) {
            for (int ch = 1; ch <= total; ++ch) {
                if (m_stopRequested.loadAcquire())
                    return false;
                if (scope->isChannelEnabled(ch))
                    channels.append(ch);
            }
        } else {
            int ch = cfg.channel;
            if (ch == 0)
                ch = scope->triggerChannel();
            if (ch >= 1 && ch <= total)
                channels.append(ch);
        }
        if (channels.isEmpty()) {
            return failTask("Capture: no valid capture channel (All requires enabled channels)");
        }
    } else {
        channels.append(0);
    }
    const QString base = task + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + "_" +
                         QUuid::createUuid().toString(QUuid::Id128);
    bool success = true;
    for (int ch : channels) {
        if (m_stopRequested.loadAcquire())
            return false;
        const QString suffix = ch ? QString("_CH%1").arg(ch) : QString{};
        const QString path = QDir(directory).filePath(base + suffix + "." + format.toLower());
        const bool saved = format == "PNG" ? CaptureFile::screenshot(scope, path, format)
                                           : CaptureFile::waveform(scope, ch, path, format);
        if (saved)
            m_attemptResult.files.append(path);
        else
            failTask(QString("Capture %1 failed: %2").arg(task, path));
        emit logMessage(QString("Capture %1: %2").arg(saved ? "saved" : "failed", path));
        m_actionResults << QString("%1: %2 — %3").arg(task, saved ? "Saved" : "Failed", path);
        emit taskResultChanged(m_currentTaskIndex, m_actionResults.join('\n'));
        success = success && saved; // A partial All capture fails, while preserving saved files.
    }
    return success && !m_stopRequested.loadAcquire();
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
    release();
    return false;
}
