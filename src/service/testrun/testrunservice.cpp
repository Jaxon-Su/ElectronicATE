#include "testrunservice.h"
#include "page5scopepolicy.h"
#include "page5capturevalidation.h"
#include "oscilloscope.h"
#include <QDir>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <stdexcept>

TestRunService::TestRunService(TestRunDependencies dependencies, QObject *parent)
    : QObject(parent), m_dependencies(std::move(dependencies))
{
}

void TestRunService::stop() { m_stopRequested.storeRelease(1); }

//  startTasks：主執行迴圈（worker thread）
void TestRunService::startTasks(const QVector<TaskPayload> &payloads, const Page5ExecutionContext &context)
{
    m_context = context;
    m_cleanupFailed = false;
    m_attemptActive = false;
    m_pendingCleanups.clear();
    m_runResults.clear();
    const QString runId = QUuid::createUuid().toString(QUuid::Id128);
    emit logMessage("=== Test started ===");
    for (int i = 0; i < payloads.size(); ++i)
        emit taskResultChanged(i, QString{});

    auto makeRecord = [&](int index, int attempt) {
        const auto &payload = payloads[index];
        Page5ResultRecord record;
        record.runId = runId;
        record.taskIndex = index;
        record.dutUid = payload.task.dutUid;
        record.attempt = attempt;
        record.taskName = payload.task.name;
        record.externalName = payload.ext;
        record.settings = payload.settings;
        record.report = payload.report;
        record.startedAt = QDateTime::currentDateTimeUtc();
        return record;
    };
    auto appendUnexecuted = [&](int index, const QString &status, const QString &reason) {
        auto record = makeRecord(index, 0); // No hardware attempt was started.
        record.status = status;
        record.error = reason;
        record.finishedAt = record.startedAt;
        m_runResults.append(record);
        emit taskResultChanged(index, status + ": " + reason);
    };
    auto rejectRun = [&](int failedIndex, const QString &error) {
        for (int i = 0; i < payloads.size(); ++i)
            appendUnexecuted(i, i == failedIndex ? "Fail" : "Skipped",
                             i == failedIndex ? error : "Run blocked by preflight failure");
        emit taskStatusChanged(failedIndex, TaskStatus::Fail);
        emit logMessage(error);
        emit resultRecordsReady(m_runResults);
        m_context.scope = nullptr;
        emit finished();
    };

    QVector<TaskSettings::PreparedTask> prepared;
    for (int i = 0; i < payloads.size(); ++i) {
        try {
            const auto &payload = payloads[i];
            if (!TaskSettingRules::retry.accepts(payload.retry))
                throw std::invalid_argument("Invalid Fail Retry: expected 0..100");
            OscilloscopeSettings configuration;
            if (Page5TaskKind::fromName(payload.task.name) == Page5TaskKind::Kind::WriteScope) {
                QString error;
                if (!m_context.scope || !m_dependencies.prepareConfiguration ||
                    !m_dependencies.prepareConfiguration(m_context.scope->model(), m_context.scope->getTotalChannel(),
                                                         payload.settings, configuration, error))
                    throw std::invalid_argument(
                        ("Invalid oscilloscope configuration: " + error).toStdString());
            }
            prepared.append(TaskSettings::decode(payload.task.name, payload.settings, std::move(configuration)));
            if (const auto *group = std::get_if<TaskSettings::Group>(&prepared.last().settings)) {
                if (group->measurement().input < 0 || group->measurement().input >= m_context.inputs.size() + m_context.dcInputs.size())
                    throw std::invalid_argument("Group: select a valid input condition");
                const bool dynamic = Page5TaskKind::singleKind(group->kind) == Page5TaskKind::Kind::Dynamic;
                auto validateLoads = [&](const auto &rows) {
                    for (int index : group->loads)
                        if (index < 0 || index >= rows.size() ||
                            std::all_of(rows[index].values.cbegin(), rows[index].values.cend(),
                                        [](const QString &value) { return value.trimmed().isEmpty(); }))
                            throw std::invalid_argument("Group: selected load condition is unavailable or empty");
                };
                if (dynamic)
                    validateLoads(m_context.dynamics);
                else
                    validateLoads(m_context.loads);
            }
        } catch (const std::exception &error) {
            rejectRun(i, QString::fromUtf8(error.what()));
            return;
        } catch (...) {
            rejectRun(i, "Unexpected task preparation error");
            return;
        }
    }

    // Missing adapters are configuration errors, never a reason to begin output control.
    for (int i = 0; i < payloads.size(); ++i) {
        const auto kind = Page5TaskKind::fromName(payloads[i].task.name);
        const bool measurement = Page5TaskKind::isMeasurement(payloads[i].task.name);
        const auto &operations = m_dependencies.instruments;
        QString error;
        if (kind == Page5TaskKind::Kind::Relay && !operations.relay)
            error = "Relay operation is unavailable";
        else if (kind == Page5TaskKind::Kind::WriteScope && !m_dependencies.prepareConfiguration)
            error = "Oscilloscope configuration is unavailable";
        else if (measurement) {
            if (!m_dependencies.strategy)
                error = "Strategy factory is unavailable";
            else {
                try {
                    const auto *steady = std::get_if<TaskSettings::Steady>(&prepared[i].settings);
                    const auto *group = std::get_if<TaskSettings::Group>(&prepared[i].settings);
                    if (group)
                        steady = std::get_if<TaskSettings::Steady>(&group->member);
                    const auto single = Page5TaskKind::singleKind(kind);
                    const TaskSettings::Measurement &config =
                        group ? group->measurement() : steady ? static_cast<const TaskSettings::Measurement &>(*steady)
                               : static_cast<const TaskSettings::Measurement &>(
                                     std::get<TaskSettings::Transient>(prepared[i].settings));
                    if ((config.input < m_context.inputs.size() && !operations.input) ||
                        (config.input >= m_context.inputs.size() && !operations.dcGroup))
                        error = "Input operation is unavailable";
                    else if ((single == Page5TaskKind::Kind::Dynamic)
                                 ? !operations.dynamic : !operations.load)
                        error = "Load operation is unavailable";
                    else if (steady && steady->autoPeriod && !m_dependencies.autoPeriod)
                        error = "Auto Period operation is unavailable";
                    else if ((single == Page5TaskKind::Kind::TurnOn || single == Page5TaskKind::Kind::OnShort ||
                              single == Page5TaskKind::Kind::ShortOn) &&
                             !operations.relay)
                        error = "Relay operation is unavailable";
                } catch (const std::exception &exception) {
                    error = QString::fromUtf8(exception.what());
                }
            }
        }
        if (!error.isEmpty()) {
            rejectRun(i, error);
            return;
        }
    }

    // Reject an invalid later Capture before earlier tasks can switch outputs.
    for (int i = 0; i < payloads.size(); ++i) {
        QString error;
        if (payloads[i].task.name == "Capture" && !validatePage5Capture(payloads[i].settings, error)) {
            rejectRun(i, error);
            return;
        }
    }
    // Validate the whole batch before any relay, source or load can be switched.
    const int scopeTask = Page5ScopePolicy::firstScopeTask(payloads);
    if (!m_stopRequested.loadAcquire() && scopeTask >= 0) {
        const QString error =
            Page5ScopePolicy::unavailableReason(m_context.scope ? m_context.scope->model() : QString{});
        if (!error.isEmpty()) {
            rejectRun(scopeTask, error);
            return;
        }
    }

    for (int i = 0; i < payloads.size(); ++i) {
        if (m_stopRequested.loadAcquire()) {
            emit logMessage(QString("[%1] Stopped by user.").arg(i + 1));
            break;
        }

        m_currentTaskIndex = i;
        const TaskPayload &p = payloads[i];
        emit logMessage(QString("[%1] Running: %2  (uid=%3)").arg(i + 1).arg(p.task.name).arg(p.task.dutUid));
        emit taskStatusChanged(i, TaskStatus::Running);

        if (!TaskSettingRules::retry.accepts(p.retry)) {
            emit logMessage("Invalid Fail Retry: expected 0..100");
            appendUnexecuted(i, "Fail", "Invalid Fail Retry: expected 0..100");
            emit taskStatusChanged(i, TaskStatus::Fail);
            break;
        }
        bool pass = false;
        for (int attempt = 0; attempt <= p.retry; ++attempt) {
            if (m_stopRequested.loadAcquire() || m_cleanupFailed)
                break;
            if (attempt > 0) {
                emit logMessage(QString("[%1] Retry %2/%3").arg(i + 1).arg(attempt).arg(p.retry));
                emit retryCountChanged(i, attempt);
            }
            const bool measures = Page5TaskKind::isMeasurement(p.task.name);
            emit taskResultChanged(i, measures ? QStringLiteral("NA") : QString{});
            m_actionResults.clear();
            m_attemptResult = makeRecord(i, attempt + 1);
            m_attemptActive = true;
            try {
                pass = executeTask(prepared[i]);
            } catch (const std::exception &ex) {
                emit logMessage(QString("Task exception: %1").arg(ex.what()));
                m_attemptResult.error = QString::fromUtf8(ex.what());
                m_actionResults << "Failed: " + QString::fromUtf8(ex.what());
                pass = false;
            } catch (...) {
                failTask("Unknown task exception");
                pass = false;
            }
            m_attemptActive = false;
            if (!measures) {
                if (m_stopRequested.loadAcquire()) {
                    m_actionResults << "Stopped by user";
                    pass = false;
                } else if (!pass) {
                    m_actionResults << "Failed: " + (m_attemptResult.error.isEmpty()
                                                         ? QStringLiteral("See log for details")
                                                         : m_attemptResult.error);
                }
                emit taskResultChanged(i, m_actionResults.join('\n'));
            }
            if (m_stopRequested.loadAcquire())
                pass = false;
            m_attemptResult.summary = m_actionResults.join("; ");
            m_attemptResult.finishedAt = QDateTime::currentDateTimeUtc();
            m_attemptResult.status = m_stopRequested.loadAcquire() ? "Cancelled" : pass ? "Pass" : "Fail";
            if (!pass && m_attemptResult.error.isEmpty())
                m_attemptResult.error =
                    m_stopRequested.loadAcquire() ? "Stopped by user" : "Task failed; see log for details";
            m_runResults.append(m_attemptResult);
            if (pass)
                break;
            if (m_stopRequested.loadAcquire() || m_cleanupFailed)
                break;
        }

        emit taskStatusChanged(i, pass ? TaskStatus::Pass : TaskStatus::Fail);
        emit logMessage(QString("[%1] %2 → %3").arg(i + 1).arg(p.task.name).arg(pass ? "Pass" : "Fail"));
        if (m_cleanupFailed) {
            emit logMessage("Cleanup failed; remaining tasks and retries aborted.");
            break;
        }
    }

    // Preserve every task in the Run, including those never started after an abort.
    for (int i = 0; i < payloads.size(); ++i) {
        const bool recorded = std::any_of(m_runResults.cbegin(), m_runResults.cend(),
                                          [i](const auto &record) { return record.taskIndex == i; });
        if (!recorded)
            appendUnexecuted(i, "Skipped",
                             m_stopRequested.loadAcquire() ? "Stopped by user before execution"
                                                           : "Run aborted before execution");
    }
    for (const auto &cleanup : m_pendingCleanups)
        cleanup();
    m_pendingCleanups.clear();
    if (m_cleanupFailed)
        emit logMessage("Run ended with cleanup failure; verify instrument output states.");
    emit resultRecordsReady(m_runResults);
    emit logMessage("=== Test finished ===");
    m_context.scope = nullptr;
    emit finished();
}

void TestRunService::recordCleanupFailure(int taskIndex, const QString &error)
{
    if (m_attemptActive && taskIndex == m_currentTaskIndex) {
        failTask(error);
        return;
    }
    for (auto it = m_runResults.rbegin(); it != m_runResults.rend(); ++it) {
        if (it->taskIndex != taskIndex)
            continue;
        if (!it->error.isEmpty())
            it->error += '\n';
        it->error += error;
        if (it->status == "Pass")
            it->status = "Fail";
        break;
    }
    emit logMessage(error);
}

bool TestRunService::failTask(const QString &error)
{
    if (!m_attemptResult.error.isEmpty())
        m_attemptResult.error += '\n';
    m_attemptResult.error += error;
    emit logMessage(error);
    return false;
}

bool TestRunService::executeTask(const TaskSettings::PreparedTask &task)
{
    using Kind = Page5TaskKind::Kind;
    const auto &cfg = task.settings;
    switch (task.kind) {
    case Kind::Capture: {
        const auto &capture = std::get<TaskSettings::Capture>(cfg);
        bool success = true;
        for (const auto &format : capture.formats) {
            if (m_stopRequested.loadAcquire())
                return false;
            const bool saved = executeCapture(format, capture);
            success = success && saved;
        }
        return success;
    }
    case Kind::Delay:
        return executeDelay(std::get<TaskSettings::Delay>(cfg).milliseconds);
    case Kind::Relay:
        return executeRelay(std::get<TaskSettings::Relay>(cfg).index);
    case Kind::WriteScope:
        return executeWriteOscilloscope(std::get<TaskSettings::WriteScope>(cfg));
    case Kind::Static:
        return executeStaticTest(std::get<TaskSettings::Steady>(cfg));
    case Kind::Dynamic:
        return executeDynamicTest(std::get<TaskSettings::Steady>(cfg));
    case Kind::StaticGroup:
    case Kind::DynamicGroup:
    case Kind::TurnOnGroup:
    case Kind::TurnOffGroup:
    case Kind::OnShortGroup:
    case Kind::ShortOnGroup:
        return executeGroupTest(std::get<TaskSettings::Group>(cfg));
    case Kind::TurnOn:
        return executeTurnOn(std::get<TaskSettings::Transient>(cfg));
    case Kind::TurnOff:
        return executeTurnOff(std::get<TaskSettings::Transient>(cfg));
    case Kind::OnShort:
        return executeTurnOnThenShort(std::get<TaskSettings::Transient>(cfg));
    case Kind::ShortOn:
        return executeShortThenTurnOn(std::get<TaskSettings::Transient>(cfg));
    default:
        return failTask("Unknown task");
    }
}
