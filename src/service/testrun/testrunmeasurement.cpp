#include "testrunservice.h"
#include "transientrelayconditions.h"
#include "../measurement/measurementformat.h"
#include "strategywaveformcapture.h"
#include "oscilloscope.h"
#include "transientstrategy.h"
#include "measurementresultmapping.h"
#include "../operations/configuredinputs.h"
#include "testsequenceconditionmapping.h"
#include <QScopeGuard>
#include <QDir>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <stdexcept>

namespace
{
InputSelection selectedInputs(const TestSequenceExecutionContext &context, const TaskSettings::Measurement &settings)
{
    const auto inputs = settings.inputs;
    if (!inputs.valid(context.inputs.size(), context.dcInputs.size()))
        throw std::runtime_error("Selected input row is out of range");
    return inputs;
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
    const auto single = TestSequenceTaskKind::singleKind(cfg.kind);
    const bool dynamic = single == TestSequenceTaskKind::Kind::Dynamic;
    const QString groupName = TestSequenceTaskKind::name(cfg.kind);
    auto capture = prepareStrategyCapture(cfg.measurement(), groupName);
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
                    return executeTransient(TestSequenceTaskKind::name(single), settings, &condition, capture);
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
    publishStrategyCapture(capture);
    m_actionResults.prepend(QString("%1: %2/%3 conditions completed")
                                .arg(groupName).arg(completed).arg(cfg.loads.size()));
    m_attemptResult.summary = m_actionResults.join('\n');
    complete = complete && !m_cleanupFailed && !m_stopRequested.loadAcquire();
    emit measurementReady(m_currentTaskIndex, m_attemptResult, complete);
    return complete;
}

bool TestRunService::executeSteadyTest(const TaskSettings::Steady &cfg, bool dynamic,
                                      TestSequenceConditionResult *condition,
                                      const std::shared_ptr<StrategyWaveformCapture> &groupCapture)
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
    const auto input = selectedInputs(m_context, cfg);
    const int loadIndex = cfg.load;
    const int loadCount = dynamic ? m_context.dynamics.size() : m_context.loads.size();
    const int delayMs = cfg.settleMs;
    if (loadIndex < 0 || loadIndex >= loadCount || delayMs < 0) {
        return failTask(name + ": invalid load condition or settle time");
    }
    auto capture = condition ? groupCapture : prepareStrategyCapture(cfg, strategyName);
    auto publish = qScopeGuard([&] { if (!condition) publishStrategyCapture(capture); });
    bool outputsAttempted = false;
    auto checkStop = [&] {
        if (m_stopRequested.loadAcquire())
            throw std::runtime_error("Stopped by user");
    };
    try {
        checkStop();
        TransientContext search;
        search.target = TaskSettings::serialized(cfg.searchDirection);
        ScopeMeasurementRequest request{strategyName, *scope, search, {}};
        if (capture)
            request.capture =
                [capture, instrument = m_context.scope,
                 source = condition ? condition->condition : TestSequenceConditionSource{cfg.load, dynamic ? m_context.dynamics[cfg.load].label : m_context.loads[cfg.load].label, dynamic ? LoadConditionKind::Dynamic : LoadConditionKind::Static}](const ScopeCaptureRecord &record) {
                    capture->record(instrument, record, source);
                };
        auto strategy = m_dependencies.strategy(request);
        if (!strategy)
            throw std::runtime_error("Missing oscilloscope strategy");
        outputsAttempted = true;
        if (!prepareSteadyOutputs(cfg, dynamic, input, name))
            return false;
        prepareSteadyScope(cfg, name, request, !capture);
        emit logMessage(name + ": oscilloscope -> " + strategy->name());
        auto result = strategy->execute(m_stopRequested);
        if (!condition) {
            publishStrategyCapture(capture);
            publish.dismiss();
        }
        if (result.cleanupFailed)
            m_cleanupFailed = true;
        if (condition) {
            condition->error = result.error;
            condition->trials = result.trials;
            condition->channels = representativeChannels(result.trials);
        } else {
            m_attemptResult.error = result.error;
            m_attemptResult.trials = result.trials;
            m_attemptResult.channels = representativeChannels(result.trials);
            m_attemptResult.summary = m_actionResults.join("\n");
            emit measurementReady(m_currentTaskIndex, m_attemptResult, false);
        }
        checkStop();
        if (!result.success || result.cleanupFailed)
            throw std::runtime_error(result.error.toStdString());
        for (const auto &ch : representativeChannels(result.trials))
            emit logMessage(QString("    CH%1 Max=%2 Min=%3 RMS=%4 Mean=%5 [%6, record %7]")
                                .arg(ch.channel)
                                .arg(formatOscMeasurement(ch.maximum), formatOscMeasurement(ch.minimum),
                                     formatOscMeasurement(ch.rms), formatOscMeasurement(ch.mean))
                                .arg(measurementDirectionName(ch.direction)).arg(ch.sourceSequence));
        checkStop();
        const bool cleaned = cleanupSteadyOutputs(input, loadIndex, dynamic);
        outputsAttempted = false;
        checkStop();
        if (!cleaned) return false;
        if (!condition) emit measurementReady(m_currentTaskIndex, m_attemptResult, true);
        emit logMessage(name + ": done (outputs OFF)");
        return true;
    } catch (const std::exception &ex) {
        failTask(name + ": " + QString::fromUtf8(ex.what()));
    } catch (...) {
        failTask(name + ": unexpected execution error");
    }
    if (outputsAttempted)
        cleanupSteadyOutputs(input, loadIndex, dynamic);
    return false;
}

// Supply/load preparation is shared by Single, Group members and retries.
// false means OFF failed and was already recorded; exceptions use the caller's cleanup path.
bool TestRunService::prepareSteadyOutputs(const TaskSettings::Steady &cfg, bool dynamic,
                                         InputSelection input, const QString &name)
{
    const int loadIndex = cfg.load;
    auto checkStop = [&] {
        if (m_stopRequested.loadAcquire())
            throw std::runtime_error("Stopped by user");
    };
    auto checkResult = [](const InstrumentOperationResult &result) {
        if (!result.success)
            throw std::runtime_error(result.errorMessage.toStdString());
    };
    auto load = [&](LoadAction action) {
        if (!dynamic)
            return m_dependencies.instruments.load(
                m_context.instrumentSetup, m_context.loads, loadIndex, m_context.loadMeta,
                action, m_context.loadSync.enabled);
        const bool dirty = m_context.loadSync.dirty;
        auto result = m_dependencies.instruments.dynamic(
            m_context.instrumentSetup, m_context.dynamics, loadIndex, m_context.dynamicMeta,
            action == LoadAction::LoadOn ? DynamicLoadAction::LoadOn : DynamicLoadAction::Change,
            m_context.loadSync.enabled, dirty);
        // Preparation does not start/restore the complete sync plan. Consume only after ON succeeds.
        if (result.success && action == LoadAction::LoadOn)
            m_context.loadSync.dirty = false;
        return result;
    };
    auto wait = [&](const QString &phase, int milliseconds) {
        emit logMessage(QString("%1: %2 for %3 ms").arg(name, phase).arg(milliseconds));
        if (!doDelay(milliseconds, m_stopRequested))
            throw std::runtime_error("Stopped by user");
        checkStop();
    };
    // Confirm all sources and the shared load OFF, including on the first task/retry.
    // Attempt remaining OFF operations even when another one fails.
    const bool selectedOff = cleanupSteadyOutputs(input, loadIndex, dynamic, false);
    const bool unselectedOff = turnOffUnselectedInputs(input);
    if (!selectedOff || !unselectedOff) return false;
    wait("power-off wait", cfg.powerOffMs);
    checkResult(runSelectedInputs(input, InputAction::Change));
    checkStop();
    checkResult(load(LoadAction::Change));
    checkStop();
    checkResult(runSelectedInputs(input, InputAction::PowerOn));
    checkStop();
    wait("startup wait", cfg.startupMs);
    checkResult(load(LoadAction::LoadOn));
    checkStop();
    wait("settling", cfg.settleMs);
    return true;
}

// Configure acquisition/optional timebase before the strategy captures its AUTO reference.
void TestRunService::prepareSteadyScope(const TaskSettings::Steady &cfg, const QString &name,
                                       const ScopeMeasurementRequest &request, bool allowInitialAuto)
{
    auto *scope = m_context.scope;
    auto checkStop = [&] {
        if (m_stopRequested.loadAcquire())
            throw std::runtime_error("Stopped by user");
    };
    QString startupError;
    if (!scope->prepareSteadyAcquisition(m_stopRequested, startupError, allowInitialAuto))
        throw std::runtime_error(startupError.toStdString());
    checkStop();
    if (cfg.autoPeriod) {
        emit logMessage(name + ": Auto Period after settling (target 5 cycles, accept 3-8)");
        QString error;
        if (!m_dependencies.autoPeriod(scope, m_stopRequested, error, request.capture))
            throw std::runtime_error(error.toStdString());
        checkStop();
    }
}

InstrumentOperationResult TestRunService::runSelectedInputs(InputSelection inputs, InputAction action)
{
    if (!inputs.valid(m_context.inputs.size(), m_context.dcInputs.size()))
        return {false, "Selected AC/DC input condition is out of range"};
    const bool turningOn = action == InputAction::PowerOn;
    QStringList errors;
    auto operate = [&](const QString &label, const auto &operation) {
        try {
            if (turningOn && m_stopRequested.loadAcquire())
                throw std::runtime_error("Stopped by user");
            const auto result = operation();
            if (!result.success) errors << label + ": " + result.errorMessage;
        } catch (const std::exception &e) {
            errors << label + ": " + QString::fromUtf8(e.what());
        } catch (...) {
            errors << label + ": unexpected input operation error";
        }
    };
    // Defined order, not hardware-synchronous switching. OFF always attempts both,
    // including after cancellation or an exception from the first source.
    if (inputs.ac >= 0)
        operate("AC Input", [&] {
            return m_dependencies.instruments.input(m_context.instrumentSetup, m_context.inputs[inputs.ac], action);
        });
    if (inputs.dc >= 0 && (!turningOn || errors.isEmpty()))
        operate("DC Input", [&] {
            return m_dependencies.instruments.dcGroup(m_context.instrumentSetup, m_context.dcInputs[inputs.dc], action);
        });
    return {errors.isEmpty(), errors.join(';')};
}

bool TestRunService::turnOffUnselectedInputs(InputSelection inputs)
{
    bool success = true;
    auto off = [&](const QString &name, const auto &operation) {
        QString error;
        try {
            const auto result = operation();
            if (result.success) return;
            error = result.errorMessage;
        } catch (const std::exception &e) { error = QString::fromUtf8(e.what()); }
          catch (...) { error = "unexpected OFF error"; }
        success = false;
        m_cleanupFailed = true;
        failTask(name + " None → OFF failed: " + error);
    };
    if (inputs.ac < 0 && hasConfiguredAc(m_context.instrumentSetup))
        off("AC", [&] { return m_dependencies.instruments.input(m_context.instrumentSetup, {}, InputAction::PowerOff); });
    if (inputs.dc < 0 && hasConfiguredDc(m_context.instrumentSetup))
        off("DC", [&] { return m_dependencies.instruments.dcGroup(m_context.instrumentSetup, {}, InputAction::PowerOff); });
    return success;
}

bool TestRunService::cleanupSteadyOutputs(InputSelection input, int loadIndex, bool dynamic, bool stopScope)
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
        recordCleanupFailure(m_currentTaskIndex, "Cleanup failed: " + label + " - " + error);
    };
    if (stopScope) cleanup("Scope STOP", [&] {
        if (!m_context.scope)
            return InstrumentOperationResult{};
        QString error;
        const bool ok = m_context.scope->stopAcquisition(error);
        return InstrumentOperationResult{ok, error};
    });
    cleanup("Load OFF", [&] {
        return dynamic
                   ? m_dependencies.instruments.dynamic(m_context.instrumentSetup, m_context.dynamics, loadIndex,
                                                        m_context.dynamicMeta, DynamicLoadAction::LoadOff,
                                                        m_context.loadSync.enabled, m_context.loadSync.dirty)
                   : m_dependencies.instruments.load(m_context.instrumentSetup, m_context.loads, loadIndex,
                                                     m_context.loadMeta, LoadAction::LoadOff,
                                                     m_context.loadSync.enabled);
    });
    cleanup("Input OFF", [&] { return runSelectedInputs(input, InputAction::PowerOff); });
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
                                      TestSequenceConditionResult *condition,
                                      const std::shared_ptr<StrategyWaveformCapture> &groupCapture)
{
    if (!m_context.scope) {
        return failTask(name + ": no oscilloscope available");
    }
    const auto input = selectedInputs(m_context, cfg);
    const int load = cfg.load;
    if (load < 0 || load >= m_context.loads.size()) {
        return failTask(name + ": select a valid load condition");
    }
    const auto masks = transientRelayConditions(m_context, TestSequenceTaskKind::fromName(name), cfg);
    const auto &shortMask = masks.shortMask;
    const auto &dischargeMask = masks.dischargeMask;
    const bool needsShort = masks.needsShort, needsDischarge = masks.needsDischarge;
    TransientContext context;
    context.target = TaskSettings::serialized(cfg.searchDirection);
    context.edge = QStringLiteral("AUTO");
    context.timeoutMs = cfg.triggerTimeoutMs;
    context.phaseTimeoutMs = cfg.phaseTimeoutMs;
    context.settleMs = cfg.settleMs;
    context.dischargeMs = cfg.dischargeMs;
    context.trialsPerLevel = cfg.trialsPerLevel;
    const ShortCircuitProtection protection{shortMask, dischargeMask, context.powerOffMs, context.dischargeMs, true};
    auto protect = [this, needsShort, protection] {
        if (needsShort && !m_shortCircuit.active()) m_shortCircuit = protection;
        if (needsShort) m_unreleasedOutputs.insert(TableKind::Relay);
    };
    context.power = [this, input, protect](bool on, QString &error) {
        protect();
        const auto result = runSelectedInputs(input, on ? InputAction::PowerOn : InputAction::PowerOff);
        error = result.errorMessage;
        return result.success;
    };
    context.load = [this, load](bool on, QString &error) {
        const auto result = m_dependencies.instruments.load(
            m_context.instrumentSetup, m_context.loads, load, m_context.loadMeta,
            on ? LoadAction::LoadOn : LoadAction::LoadOff, m_context.loadSync.enabled);
        error = result.errorMessage;
        return result.success;
    };
    context.relays = [this, shortMask, dischargeMask, needsShort, protect,
                      needsDischarge](bool shorted, bool discharge, QString &error) {
        if (!needsShort && !needsDischarge)
            return true;
        protect();
        if (needsShort && shorted) m_shortCircuit.shortMayBeOn = true;
        RelayDataRow row;
        row.values.fill("OFF", shortMask.size());
        for (int i = 0; i < row.values.size(); ++i)
            if ((shorted && shortMask[i] == "ON") || (discharge && dischargeMask[i] == "ON"))
                row.values[i] = "ON";
        // RelayOn applies the whole row (ON and OFF); never use RelayOff to release only discharge.
        const auto result = m_dependencies.instruments.relay(m_context.instrumentSetup, {row}, 0, RelayAction::RelayOn);
        error = result.errorMessage;
        if (needsShort && result.success && !shorted && !discharge) m_shortCircuit = {};
        return result.success;
    };
    context.log = [this, name](const QString &text) { emit logMessage(name + ": " + text); };
    auto capture = condition ? groupCapture : prepareStrategyCapture(cfg, name);
    auto publish = qScopeGuard([&] { if (!condition) publishStrategyCapture(capture); });
    ScopeMeasurementRequest request{name, *m_context.scope, context, {}};
    if (capture)
        request.capture =
            [capture, instrument = m_context.scope,
             source = condition ? condition->condition : TestSequenceConditionSource{cfg.load, m_context.loads[cfg.load].label, LoadConditionKind::Static}](const ScopeCaptureRecord &record) {
                capture->record(instrument, record, source);
            };
    auto strategy = m_dependencies.strategy(request);
    if (!strategy) {
        return failTask(name + ": missing strategy");
    }
    // A Turn-on strategy may operate the load/relays before its first power(true).
    auto retainShort = qScopeGuard([&] {
        if (m_shortCircuit.active()) {
            m_cleanupFailed = true;
            m_unreleasedOutputs.insert(TableKind::Relay);
        }
    });
    if (needsShort) protect(); // Also retain the relay if None -> OFF fails before strategy entry.
    if (!turnOffUnselectedInputs(input)) return false;
    auto result = strategy->execute(m_stopRequested);
    if (needsShort && m_shortCircuit.active()) {
        result.success = false;
        result.cleanupFailed = true;
        if (!result.error.isEmpty()) result.error += "; ";
        result.error += "Short circuit retained: confirm Input OFF and discharge before Relay OFF";
    }
    if (!condition) {
        publishStrategyCapture(capture);
        publish.dismiss();
    }
    if (result.cleanupFailed)
        m_cleanupFailed = true;
    if (condition) {
        condition->error = result.error;
        condition->trials = result.trials;
        condition->channels = representativeChannels(result.trials);
        if (!result.success || result.cleanupFailed)
            failTask(name + ": " + result.error);
    } else {
        m_attemptResult.error = result.error;
        m_attemptResult.trials = result.trials;
        m_attemptResult.channels = representativeChannels(result.trials);
        m_attemptResult.summary = m_actionResults.join("\n");
        emit measurementReady(m_currentTaskIndex, m_attemptResult, result.success && !result.cleanupFailed);
    }
    for (const auto &ch : representativeChannels(result.trials))
        emit logMessage(QString("  %1 CH%2 record %3: Max=%4 Min=%5 RMS=%6 Mean=%7")
                            .arg(measurementDirectionName(ch.direction))
                            .arg(ch.channel)
                            .arg(ch.sourceSequence)
                            .arg(formatOscMeasurement(ch.maximum), formatOscMeasurement(ch.minimum),
                                 formatOscMeasurement(ch.rms), formatOscMeasurement(ch.mean)));
    emit logMessage(name + (result.success ? ": done" : ": failed — " + result.error));
    return result.success && !result.cleanupFailed;
}

std::shared_ptr<StrategyWaveformCapture>
TestRunService::prepareStrategyCapture(bool enabled, const CaptureOptions &settings, InputSelection inputs, const QString &name)
{
    if (!enabled)
        return {};
    const auto labels = TestSequenceConditions::inputOptions(m_context);
    return std::make_shared<StrategyWaveformCapture>(
        settings, m_context.reportDirectory,
        m_stopRequested,
        inputs.ac < 0 ? "AC_None" : QString("AC%1_%2").arg(inputs.ac + 1).arg(labels.ac.value(inputs.ac)),
        inputs.dc < 0 ? "DC_None" : QString("DC%1_%2").arg(inputs.dc + 1).arg(labels.dc.value(inputs.dc)), name,
        StrategyWaveformCapture::Attempt{m_attemptResult.taskIndex + 1,
                                        m_attemptResult.attempt, m_attemptResult.startedAt});
}

void TestRunService::publishStrategyCapture(const std::shared_ptr<StrategyWaveformCapture> &capture)
{
    if (!capture)
        return;
    m_attemptResult.files += capture->files();
    m_attemptResult.files.removeDuplicates();
    const auto summary = capture->summary();
    if (!summary.isEmpty()) {
        m_actionResults << summary;
        emit logMessage(summary);
    }
}
