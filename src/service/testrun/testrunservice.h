#pragma once
#include "testsequenceresultrecord.h"
#include "testrundependencies.h"
#include "tasksettings.h"
#include <QObject>
#include <QVector>
#include <QAtomicInt>
#include <functional>
#include <QSet>
#include <memory>
#include "instrumentactions.h"
#include "instrumentoperationresult.h"
#include "testsequencetaskpayload.h"
#include "taskstatus.h"

class StrategyWaveformCapture;

class TestRunService : public QObject
{
    Q_OBJECT

  public:
    explicit TestRunService(TestRunDependencies dependencies, QObject *parent = nullptr);
    void prepareRun() { m_stopRequested.storeRelease(0); }
    ~TestRunService() override = default;

  public slots:
    void startTasks(const QVector<TaskPayload> &payloads, const TestSequenceExecutionContext &context);
    void stop();

  signals:
    void taskStatusChanged(int runPanelIndex, TaskStatus status);
    void retryCountChanged(int runPanelIndex, int attempt);
    void logMessage(const QString &msg);
    void taskResultChanged(int runPanelIndex, const QString &result);
    void measurementReady(int runPanelIndex, const TestSequenceResultRecord &record, bool complete);
    void resultRecordsReady(const QVector<TestSequenceResultRecord> &records);
    void outputsUncertain(const InstrumentSetupConfig &config, const QVector<TableKind> &outputs,
                          const ShortCircuitProtection &protection = {});
    void finished();

  private:
    VerticalScaleOptions verticalScaleOptions(bool margin);
    bool restoreVerticalScales();
    QMap<int, double> m_originalScales;
    std::shared_ptr<StrategyWaveformCapture> prepareStrategyCapture(bool enabled, const CaptureOptions &settings,
                                                                   InputSelection inputs, const QString &name);
    std::shared_ptr<StrategyWaveformCapture> prepareStrategyCapture(const TaskSettings::Measurement &cfg,
                                                                   const QString &name) {
        return prepareStrategyCapture(cfg.captureEnabled, cfg.captureSettings, cfg.inputs, name);
    }
    void publishStrategyCapture(const std::shared_ptr<StrategyWaveformCapture> &capture);
    void recordCleanupFailure(int taskIndex, const QString &error);
    bool failTask(const QString &error);
    bool executeTask(const TaskSettings::PreparedTask &task);
    bool executeBrownOut(const BrownOut::Settings &settings);
    QMap<int, BrownOutIoFactory> m_brownOutFactories;

    bool executeDelay(int totalMs);
    bool executeWriteOscilloscope(const TaskSettings::WriteScope &cfg);
    bool executeTransient(const QString &name, const TaskSettings::Transient &cfg,
                          TestSequenceConditionResult *condition = nullptr,
                          const std::shared_ptr<StrategyWaveformCapture> &groupCapture = {});
    bool executeTurnOn(const TaskSettings::Transient &cfg);
    bool executeTurnOff(const TaskSettings::Transient &cfg);
    bool executeRelay(int relayIndex);
    InstrumentOperationResult runSelectedInputs(InputSelection inputs, InputAction action);
    bool turnOffUnselectedInputs(InputSelection inputs);
    InstrumentOperationResult trackOutput(TableKind kind, bool on,
                                         const std::function<InstrumentOperationResult()> &operation);
    QSet<TableKind> m_unreleasedOutputs;
    ShortCircuitProtection m_shortCircuit;
    bool prepareSteadyOutputs(const TaskSettings::Steady &cfg, bool dynamic,
                               InputSelection inputs, const QString &name);
    bool prepareMeasurementTrigger(const TaskSettings::Measurement &cfg, const QString &name);
    void prepareSteadyScope(const TaskSettings::Steady &cfg, const QString &name,
                            const ScopeMeasurementRequest &request, bool allowInitialAuto);
    bool cleanupSteadyOutputs(InputSelection inputs, int load, bool dynamic, bool stopScope = true);
    QVector<std::function<void()>> m_pendingCleanups;
    bool executeSteadyTest(const TaskSettings::Steady &cfg, bool dynamic,
                           TestSequenceConditionResult *condition = nullptr,
                           const std::shared_ptr<StrategyWaveformCapture> &groupCapture = {});
    bool executeStaticTest(const TaskSettings::Steady &cfg);
    bool executeDynamicTest(const TaskSettings::Steady &cfg);
    bool executeGroupTest(const TaskSettings::Group &cfg);
    bool executeTurnOnThenShort(const TaskSettings::Transient &cfg);
    bool executeShortThenTurnOn(const TaskSettings::Transient &cfg);

    TestRunDependencies m_dependencies;
    TestSequenceExecutionContext m_context;
    QAtomicInt m_stopRequested = 0;
    bool m_cleanupFailed = false;
    bool m_attemptActive = false;
    int m_currentTaskIndex = -1;
    QStringList m_actionResults;
    TestSequenceResultRecord m_attemptResult;
    QVector<TestSequenceResultRecord> m_runResults;
};
