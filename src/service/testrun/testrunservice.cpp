#include "testrunservice.h"
#include "testsequencescopepolicy.h"
#include "transientrelayconditions.h"
#include "oscilloscope.h"
#include "../operations/configuredinputs.h"
#include "../operations/relayconditionvalidation.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
TestSequenceConditionSource singleLoadSource(const TaskSettings::PreparedTask &task,
                                     const TestSequenceExecutionContext &context)
{
    return std::visit([&](const auto &settings) -> TestSequenceConditionSource {
        using Settings = std::decay_t<decltype(settings)>;
        if constexpr (std::is_same_v<Settings, TaskSettings::Steady> ||
                      std::is_same_v<Settings, TaskSettings::Transient> ||
                      std::is_same_v<Settings, BrownOut::Settings>) {
            const int index = settings.load;
            if (task.kind == TestSequenceTaskKind::Kind::Dynamic) {
                if (index >= 0 && index < context.dynamics.size())
                    return {index, context.dynamics[index].label, LoadConditionKind::Dynamic};
            } else if (index >= 0 && index < context.loads.size()) {
                return {index, context.loads[index].label, LoadConditionKind::Static};
            }
        }
        return {}; // Group members have their own sources; non-measurement tasks have no load.
    }, task.settings);
}
} // namespace

TestRunService::TestRunService(TestRunDependencies dependencies, QObject *parent)
    : QObject(parent), m_dependencies(std::move(dependencies))
{
    auto &ops = m_dependencies.instruments;
    if (ops.input) ops.input = [this, run = ops.input](const auto &config, const auto &row, InputAction action) {
        return trackOutput(TableKind::Input, action != InputAction::PowerOff,
                           [&] { return run(config, row, action); });
    };
    if (ops.dcGroup) ops.dcGroup = [this, run = ops.dcGroup](const auto &config, const auto &row, InputAction action) {
        return trackOutput(TableKind::Dc, action != InputAction::PowerOff,
                           [&] { return run(config, row, action); });
    };
    if (ops.load) ops.load = [this, run = ops.load](const auto &config, const auto &rows, int index,
                                                   const auto &meta, LoadAction action, bool sync) {
        return trackOutput(TableKind::Load, action != LoadAction::LoadOff,
                           [&] { return run(config, rows, index, meta, action, sync); });
    };
    if (ops.dynamic) ops.dynamic = [this, run = ops.dynamic](const auto &config, const auto &rows, int index,
                          const auto &meta, DynamicLoadAction action, bool sync, bool dirty) {
        return trackOutput(TableKind::DyLoad, action != DynamicLoadAction::LoadOff,
                           [&] { return run(config, rows, index, meta, action, sync, dirty); });
    };
    if (ops.relay) ops.relay = [this, run = ops.relay](const auto &config, const auto &rows, int index, RelayAction action) {
        bool on = action != RelayAction::RelayOff;
        if (on && index >= 0 && index < rows.size())
            on = std::any_of(rows[index].values.cbegin(), rows[index].values.cend(),
                             [](const QString &value) { return value.trimmed().compare("ON", Qt::CaseInsensitive) == 0; });
        return trackOutput(TableKind::Relay, on, [&] { return run(config, rows, index, action); });
    };
}

InstrumentOperationResult TestRunService::trackOutput(
    TableKind kind, bool on, const std::function<InstrumentOperationResult()> &operation)
{
    // Opening a transport may fail before any write. An OFF request still has to
    // retain uncertainty about the output left by an earlier task.
    const bool previouslyActive = m_unreleasedOutputs.contains(kind) ||
        ((kind == TableKind::Load || kind == TableKind::DyLoad) &&
         (m_unreleasedOutputs.contains(TableKind::Load) || m_unreleasedOutputs.contains(TableKind::DyLoad)));
    m_unreleasedOutputs.insert(kind);
    const auto result = operation(); // Exceptions intentionally retain uncertainty.
    if ((result.confirmedOutput && !*result.confirmedOutput) || (result.success && !on)) {
        m_unreleasedOutputs.remove(kind);
        if (kind == TableKind::Load || kind == TableKind::DyLoad) {
            m_unreleasedOutputs.remove(TableKind::Load);
            m_unreleasedOutputs.remove(TableKind::DyLoad);
        }
    } else if (!previouslyActive && !result.confirmedOutput.value_or(false) && result.outputUnchanged) {
        // No earlier output to recover and no output write in this operation.
        // Keep the failure, without manufacturing an UNKNOWN handoff to ManualControl.
        m_unreleasedOutputs.remove(kind);
    }
    return result;
}

void TestRunService::stop() { m_stopRequested.storeRelease(1); }

//  startTasks：主執行迴圈（worker thread）
void TestRunService::startTasks(const QVector<TaskPayload> &payloads, const TestSequenceExecutionContext &context)
{
    m_context = context;
    m_cleanupFailed = false;
    m_attemptActive = false;
    m_pendingCleanups.clear();
    m_unreleasedOutputs.clear(); // The UI gates another Run until manual recovery succeeds.
    m_shortCircuit = {};
    m_brownOutFactories.clear();
    m_runResults.clear();
    const QString runId = QUuid::createUuid().toString(QUuid::Id128);
    emit logMessage("=== Test started ===");
    for (int i = 0; i < payloads.size(); ++i)
        emit taskResultChanged(i, QString{});

    auto makeRecord = [&](int index, int attempt) {
        const auto &payload = payloads[index];
        TestSequenceResultRecord record;
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
            if (TestSequenceTaskKind::fromName(payload.task.name) == TestSequenceTaskKind::Kind::WriteScope) {
                QString error;
                if (!m_context.scope || !m_dependencies.prepareConfiguration ||
                    !m_dependencies.prepareConfiguration(m_context.scope->model(), m_context.scope->getTotalChannel(),
                                                         payload.settings, configuration, error))
                    throw std::invalid_argument(
                        ("Invalid oscilloscope configuration: " + error).toStdString());
            }
            prepared.append(TaskSettings::decode(payload.task.name, payload.settings, std::move(configuration)));
            if (const auto *relay = std::get_if<TaskSettings::Relay>(&prepared.last().settings)) {
                const QString error = validateRelayCondition(m_context.instrumentSetup, m_context.relays, relay->index);
                if (!error.isEmpty()) throw std::invalid_argument(error.toStdString());
            }
            if (TestSequenceTaskKind::isMeasurement(payload.task.name) && payload.settings.value("capture_enabled").toBool()) {
                const CaptureOptions capture(payload.settings.value("capture_settings").toMap(), true);
                const QString directory = capture.directory.isEmpty() ? m_context.reportDirectory : capture.directory;
                if (!QDir::isAbsolutePath(directory) || !QFileInfo(directory).isDir() || !QFileInfo(directory).isWritable())
                    throw std::invalid_argument("Capture: select an existing writable directory in Capture settings or Report File Path");
            }
            if (TestSequenceTaskKind::isMeasurement(payload.task.name)) {
                if (!m_dependencies.validateMeasurement)
                    throw std::invalid_argument("Measurement preflight adapter is unavailable");
                const auto single = TestSequenceTaskKind::singleKind(prepared.last().kind);
                std::visit([&](const auto &settings) {
                    using Settings = std::decay_t<decltype(settings)>;
                    auto validate = [&](InputSelection inputs, int load, bool dynamic) {
                        if ((inputs.ac < 0 && hasConfiguredAc(m_context.instrumentSetup) && !m_dependencies.instruments.input) ||
                            (inputs.dc < 0 && hasConfiguredDc(m_context.instrumentSetup) && !m_dependencies.instruments.dcGroup))
                            throw std::invalid_argument("None input OFF adapter is unavailable");
                        m_dependencies.validateMeasurement(m_context, inputs, load, dynamic);
                    };
                    if constexpr (std::is_same_v<Settings, TaskSettings::Group>) {
                        if (const auto *transient = std::get_if<TaskSettings::Transient>(&settings.member))
                            transientRelayConditions(m_context, single, *transient);
                        for (int load : settings.loads)
                            validate(settings.measurement().inputs, load, single == TestSequenceTaskKind::Kind::Dynamic);
                    } else if constexpr (std::is_same_v<Settings, TaskSettings::Steady> ||
                                         std::is_same_v<Settings, TaskSettings::Transient> ||
                                         std::is_same_v<Settings, BrownOut::Settings>) {
                        if constexpr (std::is_same_v<Settings, TaskSettings::Transient>)
                            transientRelayConditions(m_context, single, settings);
                        validate(settings.inputs, settings.load, single == TestSequenceTaskKind::Kind::Dynamic);
                    }
                }, prepared.last().settings);
            }
            if (const auto *brownOut = std::get_if<BrownOut::Settings>(&prepared.last().settings)) {
                if (!m_dependencies.prepareBrownOut)
                    throw std::invalid_argument("Brown Out input/monitor adapter is unavailable");
                auto factory = m_dependencies.prepareBrownOut(m_context, *brownOut);
                if (!factory) throw std::invalid_argument("Brown Out adapter preparation failed");
                m_brownOutFactories.insert(i, std::move(factory));
            }
            if (const auto *group = std::get_if<TaskSettings::Group>(&prepared.last().settings)) {
                if (!group->measurement().inputs.valid(m_context.inputs.size(), m_context.dcInputs.size()))
                    throw std::invalid_argument("Group: select a valid input condition");
                const bool dynamic = TestSequenceTaskKind::singleKind(group->kind) == TestSequenceTaskKind::Kind::Dynamic;
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
        const auto kind = TestSequenceTaskKind::fromName(payloads[i].task.name);
        const bool measurement = TestSequenceTaskKind::isMeasurement(payloads[i].task.name);
        const auto &operations = m_dependencies.instruments;
        QString error;
        if (kind == TestSequenceTaskKind::Kind::Relay && !operations.relay)
            error = "Relay operation is unavailable";
        else if (kind == TestSequenceTaskKind::Kind::WriteScope && !m_dependencies.prepareConfiguration)
            error = "Oscilloscope configuration is unavailable";
        else if (measurement) {
            if (!m_dependencies.strategy)
                error = "Strategy factory is unavailable";
            else if (kind != TestSequenceTaskKind::Kind::BrownOut) {
                try {
                    const auto *steady = std::get_if<TaskSettings::Steady>(&prepared[i].settings);
                    const auto *group = std::get_if<TaskSettings::Group>(&prepared[i].settings);
                    if (group)
                        steady = std::get_if<TaskSettings::Steady>(&group->member);
                    const auto single = TestSequenceTaskKind::singleKind(kind);
                    const TaskSettings::Measurement &config =
                        group ? group->measurement() : steady ? static_cast<const TaskSettings::Measurement &>(*steady)
                               : static_cast<const TaskSettings::Measurement &>(
                                     std::get<TaskSettings::Transient>(prepared[i].settings));
                    if (!config.inputs.valid(m_context.inputs.size(), m_context.dcInputs.size()))
                        error = "Select valid AC and/or DC input conditions";
                    else if ((config.inputs.ac >= 0 && !operations.input) ||
                             (config.inputs.dc >= 0 && !operations.dcGroup))
                        error = "Input operation is unavailable";
                    else if ((single == TestSequenceTaskKind::Kind::Dynamic)
                                 ? !operations.dynamic : !operations.load)
                        error = "Load operation is unavailable";
                    else if (steady && steady->autoPeriod && !m_dependencies.autoPeriod)
                        error = "Auto Period operation is unavailable";
                    else if ((single == TestSequenceTaskKind::Kind::TurnOn || single == TestSequenceTaskKind::Kind::OnShort ||
                              single == TestSequenceTaskKind::Kind::ShortOn) &&
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

    // Validate the whole batch before any relay, source or load can be switched.
    const int scopeTask = TestSequenceScopePolicy::firstScopeTask(payloads);
    if (!m_stopRequested.loadAcquire() && scopeTask >= 0) {
        const QString error =
            TestSequenceScopePolicy::unavailableReason(m_context.scope ? m_context.scope->model() : QString{});
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
            const bool measures = TestSequenceTaskKind::isMeasurement(p.task.name);
            emit taskResultChanged(i, measures ? QStringLiteral("NA") : QString{});
            m_actionResults.clear();
            m_attemptResult = makeRecord(i, attempt + 1);
            m_attemptActive = true;
            try {
                m_attemptResult.loadCondition = singleLoadSource(prepared[i], m_context);
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
            emit measurementReady(i, m_attemptResult, pass);
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
    if (!m_unreleasedOutputs.isEmpty()) {
        emit logMessage("Output state is UNKNOWN. Use ManualControl OFF to recover before the next Run.");
        emit outputsUncertain(m_context.instrumentSetup, m_unreleasedOutputs.values(), m_shortCircuit);
    }
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
    using Kind = TestSequenceTaskKind::Kind;
    const auto &cfg = task.settings;
    switch (task.kind) {
    case Kind::BrownOut:
        return executeBrownOut(std::get<BrownOut::Settings>(cfg));
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
