#include "testrunservice.h"
#include "worstwaveformcapture.h"
#include "oscilloscope.h"
#include "transientstrategy.h"
#include "groupmeasurementresults.h"
#include <QDir>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <stdexcept>

namespace
{
int selectedInput(const Page5ExecutionContext &context, const TaskSettings::Measurement &settings)
{
    const int index = settings.input;
    if (index < 0 || index >= context.inputs.size() + context.dcInputs.size())
        throw std::runtime_error("Selected input row is out of range");
    return index;
}
} // namespace

//  doDelay: interruptible sleep helper
static bool doDelay(int ms, QAtomicInt &stop)
{
    const int chunk = 100;
    int elapsed = 0;
    while (elapsed < ms) {
        if (stop.loadAcquire())
            return false;
        const int wait = qMin(chunk, ms - elapsed);
        QThread::msleep(static_cast<unsigned long>(wait));
        elapsed += wait;
    }
    return true;
}

bool TestRunService::executeTurnOn(const TaskSettings::Transient &cfg)
{
    return executeTransient("Turn on", cfg);
}

bool TestRunService::executeTurnOff(const TaskSettings::Transient &cfg)
{
    return executeTransient("Turn off", cfg);
}

bool TestRunService::executeStaticTest(const TaskSettings::Steady &cfg)
{
    return executeSteadyTest(cfg, false);
}

bool TestRunService::executeDynamicTest(const TaskSettings::Steady &cfg)
{
    return executeSteadyTest(cfg, true);
}

bool TestRunService::executeGroupTest(const TaskSettings::Group &cfg)
{
    const auto single = Page5TaskKind::singleKind(cfg.kind);
    const bool dynamic = single == Page5TaskKind::Kind::Dynamic;
    const QString groupName = Page5TaskKind::name(cfg.kind);
    auto capture = prepareStrategyCapture(cfg.measurement(), groupName);
    const auto cleanupStart = m_pendingCleanups.size();
    for (int index : cfg.loads)
        m_attemptResult.conditions.append({{index, dynamic ? m_context.dynamics[index].label : m_context.loads[index].label,
                                           dynamic ? LoadConditionKind::Dynamic : LoadConditionKind::Static},
                                          "Skipped", {}, {}});
    bool complete = true;
    int completed = 0;
    for (qsizetype i = 0; i < m_attemptResult.conditions.size(); ++i) {
        if (m_stopRequested.loadAcquire()) {
            complete = false;
            break;
        }
        auto condition = m_attemptResult.conditions[i];
        auto member = cfg.member;
        std::visit([&](auto &settings) { settings.load = condition.condition.index; }, member);
        emit logMessage(QString("%1: %2/%3, %4 row %5 (%6)").arg(groupName)
                            .arg(completed + 1).arg(cfg.loads.size()).arg(condition.condition.typeName())
                            .arg(condition.condition.index + 1).arg(condition.condition.label));
        bool pass = false;
        try {
            pass = std::visit([&](const auto &settings) {
                using Settings = std::decay_t<decltype(settings)>;
                if constexpr (std::is_same_v<Settings, TaskSettings::Steady>)
                    return executeSteadyTest(settings, dynamic, &condition, capture);
                else
                    return executeTransient(Page5TaskKind::name(single), settings, &condition, capture);
            }, member);
        } catch (const std::exception &error) {
            failTask(groupName + ": " + QString::fromUtf8(error.what()));
        } catch (...) {
            failTask(groupName + ": unexpected member execution error");
        }
        pass = pass && !m_stopRequested.loadAcquire() && !m_cleanupFailed;
        condition.status = m_stopRequested.loadAcquire() ? "Cancelled" : pass ? "Pass" : "Fail";
        if (!pass && condition.error.isEmpty())
            condition.error = m_attemptResult.error;
        m_attemptResult.conditions[i] = condition;
        accumulateGroupCondition(m_attemptResult, condition);
        if (pass)
            ++completed;
        m_attemptResult.summary = QString("%1: %2/%3 conditions completed")
                                      .arg(groupName).arg(completed).arg(cfg.loads.size());
        emit measurementReady(m_currentTaskIndex, m_attemptResult, false);
        if (!pass) {
            complete = false;
            break;
        }
    }
    if (!complete) {
        // Release successful members before a whole-group retry; retain earlier tasks' cleanup.
        while (m_pendingCleanups.size() > cleanupStart)
            m_pendingCleanups.takeLast()();
    }
    publishStrategyCapture(capture);
    m_actionResults.prepend(QString("%1: %2/%3 conditions completed")
                                .arg(groupName).arg(completed).arg(cfg.loads.size()));
    m_attemptResult.summary = m_actionResults.join('\n');
    complete = complete && !m_cleanupFailed && !m_stopRequested.loadAcquire();
    emit measurementReady(m_currentTaskIndex, m_attemptResult, complete);
    return complete;
}

bool TestRunService::executeSteadyTest(const TaskSettings::Steady &cfg, bool dynamic,
                                      Page5ConditionResult *condition,
                                      const std::shared_ptr<WorstWaveformCapture> &groupCapture)
{
    const QString strategyName = dynamic ? "Dynamic Test" : "Static Test";
    const QString name = condition ? QString("%1 / %2 row %3 (%4)").arg(strategyName, condition->condition.typeName())
                                        .arg(condition->condition.index + 1).arg(condition->condition.label)
                                   : strategyName;
    Oscilloscope *scope = m_context.scope;
    // Reject invalid prerequisites before enabling any output.
    if (!scope) {
        return failTask(name + ": no oscilloscope available");
    }
    const int input = selectedInput(m_context, cfg);
    const int loadIndex = cfg.load;
    const int loadCount = dynamic ? m_context.dynamics.size() : m_context.loads.size();
    const int delayMs = cfg.settleMs;
    if (loadIndex < 0 || loadIndex >= loadCount || delayMs < 0) {
        return failTask(name + ": invalid load condition or settle time");
    }
    bool inputAttempted = false;
    auto checkStop = [&] {
        if (m_stopRequested.loadAcquire())
            throw std::runtime_error("Stopped by user");
    };
    auto checkResult = [](const InstrumentOperationResult &result) {
        if (!result.success)
            throw std::runtime_error(result.errorMessage.toStdString());
    };
    auto load = [&](bool on) {
        if (!dynamic)
            return m_dependencies.instruments.load(
                m_context.page1, m_context.loads, loadIndex, m_context.loadMeta,
                on ? LoadAction::LoadOn : LoadAction::LoadOff, m_context.loadSync.enabled);
        const bool dirty = m_context.loadSync.dirty;
        auto result = m_dependencies.instruments.dynamic(
            m_context.page1, m_context.dynamics, loadIndex, m_context.dynamicMeta,
            on ? DynamicLoadAction::LoadOn : DynamicLoadAction::LoadOff, m_context.loadSync.enabled, dirty);
        // A rejected operation must not consume the pending synchronization update.
        if (result.success)
            m_context.loadSync.dirty = false;
        return result;
    };
    try {
        checkStop();
        TransientContext search;
        search.target = TaskSettings::serialized(cfg.searchDirection);
        std::unique_ptr<IOscilloscopeMeasureStrategy> strategy(m_dependencies.strategy(strategyName, &search));
        if (!strategy)
            throw std::runtime_error("Missing oscilloscope strategy");
        auto capture = condition ? groupCapture : prepareStrategyCapture(cfg, name);
        if (capture)
            strategy->setCaptureObserver(
                [capture, instrument = m_context.scope,
                 source = condition ? condition->condition : Page5ConditionSource{}](const QVector<OscChannelMeasure> &record) {
                    capture->record(instrument, record, source);
                });
        inputAttempted = true; // Even a failed command may have partially enabled the output.
        checkResult(runSelectedInput(input, InputAction::PowerOn));
        checkStop();
        checkResult(load(true));
        checkStop();
        emit logMessage(QString("%1: settling for %2 ms").arg(name).arg(delayMs));
        if (!doDelay(delayMs, m_stopRequested))
            throw std::runtime_error("Stopped by user");
        checkStop();
        QString startupError;
        if (!scope->prepareSteadyAcquisition(m_stopRequested, startupError))
            throw std::runtime_error(startupError.toStdString());
        checkStop();
        if (cfg.autoPeriod) {
            emit logMessage(name + ": Auto Period after settling (target 5 cycles, accept 3-8)");
            QString error;
            if (!m_dependencies.autoPeriod(scope, m_stopRequested, error))
                throw std::runtime_error(error.toStdString());
            checkStop();
        }
        emit logMessage(name + ": oscilloscope -> " + strategy->name());
        auto result = strategy->execute(scope, m_stopRequested);
        if (result.cleanupFailed)
            m_cleanupFailed = true;
        if (condition) {
            condition->error = result.errorMessage;
            for (const auto &ch : result.channels)
                condition->channels.append({ch.channel, {}, ch.maxVal, ch.minVal, ch.rms, ch.mean});
        } else {
            publishStrategyCapture(capture);
            m_attemptResult.error = result.errorMessage;
            for (const auto &ch : result.channels)
                m_attemptResult.channels.append({ch.channel, QString{}, ch.maxVal, ch.minVal, ch.rms, ch.mean});
            m_attemptResult.summary = m_actionResults.join("\n");
            emit measurementReady(m_currentTaskIndex, m_attemptResult, result.success && !result.cleanupFailed);
        }
        checkStop();
        if (!result.success || result.cleanupFailed)
            throw std::runtime_error(result.errorMessage.toStdString());
        for (const auto &ch : result.channels)
            emit logMessage(QString("    CH%1 Max=%2 Min=%3 RMS=%4 Mean=%5")
                                .arg(ch.channel)
                                .arg(formatOscMeasurement(ch.maxVal), formatOscMeasurement(ch.minVal),
                                     formatOscMeasurement(ch.rms), formatOscMeasurement(ch.mean)));
        checkStop();
        emit logMessage(name + ": done");
        m_pendingCleanups.append([this, input, loadIndex, dynamic, taskIndex = m_currentTaskIndex] {
            if (!cleanupSteadyOutputs(input, loadIndex, dynamic, taskIndex))
                emit taskStatusChanged(taskIndex, TaskStatus::Fail);
        });
        return true; // Keep outputs between tasks, but release them at Run completion/Stop.
    } catch (const std::exception &ex) {
        failTask(name + ": " + QString::fromUtf8(ex.what()));
    } catch (...) {
        failTask(name + ": unexpected execution error");
    }
    if (inputAttempted)
        cleanupSteadyOutputs(input, loadIndex, dynamic);
    return false;
}

InstrumentOperationResult TestRunService::runSelectedInput(int index, InputAction action)
{
    if (index < 0 || index >= m_context.inputs.size() + m_context.dcInputs.size())
        return {false, "Selected input row is out of range"};
    if (index < m_context.inputs.size())
        return m_dependencies.instruments.input(m_context.page1, m_context.inputs[index], action);
    return m_dependencies.instruments.dcGroup(m_context.page1,
                                              m_context.dcInputs[index - m_context.inputs.size()], action);
}

bool TestRunService::cleanupSteadyOutputs(int input, int loadIndex, bool dynamic, int taskIndex)
{
    bool success = true;
    auto cleanup = [&](const QString &label, const auto &action) {
        QString error;
        try {
            const auto result = action();
            if (result.success)
                return;
            error = result.errorMessage;
        } catch (const std::exception &ex) {
            error = QString::fromUtf8(ex.what());
        } catch (...) {
            error = "unexpected cleanup exception";
        }
        m_cleanupFailed = true;
        success = false;
        recordCleanupFailure(taskIndex < 0 ? m_currentTaskIndex : taskIndex,
                             "Cleanup failed: " + label + " - " + error);
    };
    cleanup("Scope STOP", [&] {
        if (!m_context.scope)
            return InstrumentOperationResult{};
        QString error;
        const bool ok = m_context.scope->stopAcquisition(error);
        return InstrumentOperationResult{ok, error};
    });
    cleanup("Input OFF", [&] { return runSelectedInput(input, InputAction::PowerOff); });
    cleanup("Load OFF", [&] {
        return dynamic
                   ? m_dependencies.instruments.dynamic(m_context.page1, m_context.dynamics, loadIndex,
                                                        m_context.dynamicMeta, DynamicLoadAction::LoadOff,
                                                        m_context.loadSync.enabled, m_context.loadSync.dirty)
                   : m_dependencies.instruments.load(m_context.page1, m_context.loads, loadIndex,
                                                     m_context.loadMeta, LoadAction::LoadOff,
                                                     m_context.loadSync.enabled);
    });
    return success;
}

bool TestRunService::executeTurnOnThenShort(const TaskSettings::Transient &cfg)
{
    return executeTransient("Turn on then short", cfg);
}

bool TestRunService::executeShortThenTurnOn(const TaskSettings::Transient &cfg)
{
    return executeTransient("Short then turn on", cfg);
}

bool TestRunService::executeTransient(const QString &name, const TaskSettings::Transient &cfg,
                                      Page5ConditionResult *condition,
                                      const std::shared_ptr<WorstWaveformCapture> &groupCapture)
{
    if (!m_context.scope) {
        return failTask(name + ": no oscilloscope available");
    }
    const auto input = selectedInput(m_context, cfg);
    const int load = cfg.load;
    if (load < 0 || load >= m_context.loads.size()) {
        return failTask(name + ": select a valid load condition");
    }
    const bool needsShort = name.contains("short", Qt::CaseInsensitive);
    const bool needsDischarge = name == "Turn on" || needsShort;
    QVector<QString> shortMask(m_context.page1.relayOutputs, "OFF");
    QVector<QString> dischargeMask(shortMask);
    auto readMask = [&](int index, bool required, QVector<QString> &mask) {
        if (!required)
            return true;
        if (index < 0 || index >= m_context.relays.size())
            return false;
        const auto &values = m_context.relays[index].values;
        if (values.size() != mask.size())
            return false;
        bool hasOn = false;
        for (int i = 0; i < values.size(); ++i) {
            mask[i] = values[i].trimmed().toUpper();
            if (mask[i] != "ON" && mask[i] != "OFF")
                return false;
            if (mask[i] != "ON")
                continue;
            hasOn = true;
            bool mapped = false;
            for (const auto &inst : m_context.page1.instruments) {
                if (inst.type != "Relay" || !inst.enabled || inst.getResourceString().isEmpty())
                    continue;
                for (const auto &channel : inst.channels)
                    if (channel.index == i + 1)
                        mapped = true;
            }
            if (!mapped)
                return false;
        }
        return hasOn;
    };
    if (!readMask(cfg.relay, needsShort, shortMask) ||
        !readMask(cfg.discharge, needsDischarge, dischargeMask)) {
        return failTask(name + ": select relay conditions with mapped ON outputs (short/discharge)");
    }
    for (int i = 0; i < shortMask.size(); ++i) {
        if (shortMask[i] == "ON" && dischargeMask[i] == "ON") {
            return failTask(name + ": short and discharge must use different relay outputs");
        }
    }
    TransientContext context;
    context.target = TaskSettings::serialized(cfg.searchDirection);
    context.edge = QStringLiteral("AUTO");
    context.timeoutMs = cfg.triggerTimeoutMs;
    context.phaseTimeoutMs = cfg.phaseTimeoutMs;
    context.settleMs = cfg.settleMs;
    context.dischargeMs = cfg.dischargeMs;
    context.trialsPerLevel = cfg.trialsPerLevel;
    context.power = [this, input](bool on, QString &error) {
        const auto result = runSelectedInput(input, on ? InputAction::PowerOn : InputAction::PowerOff);
        error = result.errorMessage;
        return result.success;
    };
    context.load = [this, load](bool on, QString &error) {
        const auto result = m_dependencies.instruments.load(
            m_context.page1, m_context.loads, load, m_context.loadMeta,
            on ? LoadAction::LoadOn : LoadAction::LoadOff, m_context.loadSync.enabled);
        error = result.errorMessage;
        return result.success;
    };
    context.relays = [this, shortMask, dischargeMask, needsShort,
                      needsDischarge](bool shorted, bool discharge, QString &error) {
        if (!needsShort && !needsDischarge)
            return true;
        RelayDataRow row;
        row.values.fill("OFF", shortMask.size());
        for (int i = 0; i < row.values.size(); ++i)
            if ((shorted && shortMask[i] == "ON") || (discharge && dischargeMask[i] == "ON"))
                row.values[i] = "ON";
        // RelayOn applies the whole row (ON and OFF); never use RelayOff to release only discharge.
        const auto result = m_dependencies.instruments.relay(m_context.page1, {row}, 0, RelayAction::RelayOn);
        error = result.errorMessage;
        return result.success;
    };
    context.log = [this, name](const QString &text) { emit logMessage(name + ": " + text); };
    std::unique_ptr<IOscilloscopeMeasureStrategy> strategy(m_dependencies.strategy(name, &context));
    if (!strategy) {
        return failTask(name + ": missing strategy");
    }
    auto capture = condition ? groupCapture : prepareStrategyCapture(cfg, name);
    if (capture)
        strategy->setCaptureObserver(
            [capture, instrument = m_context.scope,
             source = condition ? condition->condition : Page5ConditionSource{}](const QVector<OscChannelMeasure> &record) {
                capture->record(instrument, record, source);
            });
    auto result = strategy->execute(m_context.scope, m_stopRequested);
    if (result.cleanupFailed)
        m_cleanupFailed = true;
    if (condition) {
        condition->error = result.errorMessage;
        for (const auto &ch : result.channels)
            condition->channels.append({ch.channel, {}, ch.maxVal, ch.minVal, ch.rms, ch.mean});
        if (!result.success || result.cleanupFailed)
            failTask(name + ": " + result.errorMessage);
    } else {
        publishStrategyCapture(capture);
        m_attemptResult.error = result.errorMessage;
        for (const auto &ch : result.channels)
            m_attemptResult.channels.append({ch.channel, QString{}, ch.maxVal, ch.minVal, ch.rms, ch.mean});
        m_attemptResult.summary = m_actionResults.join("\n");
        emit measurementReady(m_currentTaskIndex, m_attemptResult, result.success && !result.cleanupFailed);
    }
    for (const auto &ch : result.channels)
        emit logMessage(QString("  CH%1 Max=%2 Min=%3 RMS=%4 Mean=%5")
                            .arg(ch.channel)
                            .arg(formatOscMeasurement(ch.maxVal), formatOscMeasurement(ch.minVal),
                                 formatOscMeasurement(ch.rms), formatOscMeasurement(ch.mean)));
    emit logMessage(name + (result.success ? ": done" : ": failed — " + result.errorMessage));
    return result.success && !result.cleanupFailed;
}

std::shared_ptr<WorstWaveformCapture>
TestRunService::prepareStrategyCapture(const TaskSettings::Measurement &cfg, const QString &name)
{
    if (!cfg.captureEnabled)
        return {};
    return std::make_shared<WorstWaveformCapture>(
        cfg.captureSettings, TaskSettings::serialized(cfg.searchDirection), m_context.reportDirectory,
        QString("Task %1 / %2 / Attempt %3")
            .arg(m_currentTaskIndex + 1)
            .arg(name)
            .arg(m_attemptResult.attempt),
        m_stopRequested);
}

void TestRunService::publishStrategyCapture(const std::shared_ptr<WorstWaveformCapture> &capture)
{
    if (!capture)
        return;
    m_attemptResult.files += capture->files();
    const auto summary = capture->summary();
    if (!summary.isEmpty()) {
        m_actionResults << summary;
        emit logMessage(summary);
    }
}
