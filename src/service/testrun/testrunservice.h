#pragma once
#include "page5resultrecord.h"
#include "testrundependencies.h"
#include "tasksettings.h"
#include <QObject>
#include <QVector>
#include <QAtomicInt>
#include <functional>
#include <memory>
#include "instrumentactions.h"
#include "instrumentoperationresult.h"
#include "page5taskpayload.h"
#include "taskstatus.h"

class IOscilloscopeMeasureStrategy;
class WorstWaveformCapture;
struct OscMeasureResult;

class TestRunService : public QObject
{
    Q_OBJECT

  public:
    explicit TestRunService(TestRunDependencies dependencies, QObject *parent = nullptr);
    void prepareRun() { m_stopRequested.storeRelease(0); }
    ~TestRunService() override = default;

  public slots:
    void startTasks(const QVector<TaskPayload> &payloads, const Page5ExecutionContext &context);
    void stop();

  signals:
    void taskStatusChanged(int runPanelIndex, TaskStatus status);
    void retryCountChanged(int runPanelIndex, int attempt);
    void logMessage(const QString &msg);
    void taskResultChanged(int runPanelIndex, const QString &result);
    void measurementReady(int runPanelIndex, const Page5ResultRecord &record, bool complete);
    void resultRecordsReady(const QVector<Page5ResultRecord> &records);
    void finished();

  private:
    std::shared_ptr<WorstWaveformCapture> prepareStrategyCapture(const TaskSettings::Measurement &cfg,
                                                                 const QString &name);
    void publishStrategyCapture(const std::shared_ptr<WorstWaveformCapture> &capture);
    void recordCleanupFailure(int taskIndex, const QString &error);
    bool failTask(const QString &error);
    bool executeTask(const TaskSettings::PreparedTask &task);

    bool executeCapture(const QString &format, const TaskSettings::Capture &cfg);
    bool executeDelay(int totalMs);
    bool executeWriteOscilloscope(const TaskSettings::WriteScope &cfg);
    bool executeTransient(const QString &name, const TaskSettings::Transient &cfg,
                          Page5ConditionResult *condition = nullptr,
                          const std::shared_ptr<WorstWaveformCapture> &groupCapture = {});
    bool executeTurnOn(const TaskSettings::Transient &cfg);
    bool executeTurnOff(const TaskSettings::Transient &cfg);
    bool executeRelay(int relayIndex);
    InstrumentOperationResult runSelectedInput(int index, InputAction action);
    bool cleanupSteadyOutputs(int input, int load, bool dynamic, int taskIndex = -1);
    QVector<std::function<void()>> m_pendingCleanups;
    bool executeSteadyTest(const TaskSettings::Steady &cfg, bool dynamic,
                           Page5ConditionResult *condition = nullptr,
                           const std::shared_ptr<WorstWaveformCapture> &groupCapture = {});
    bool executeStaticTest(const TaskSettings::Steady &cfg);
    bool executeDynamicTest(const TaskSettings::Steady &cfg);
    bool executeGroupTest(const TaskSettings::Group &cfg);
    bool executeTurnOnThenShort(const TaskSettings::Transient &cfg);
    bool executeShortThenTurnOn(const TaskSettings::Transient &cfg);

    TestRunDependencies m_dependencies;
    Page5ExecutionContext m_context;
    QAtomicInt m_stopRequested = 0;
    bool m_cleanupFailed = false;
    bool m_attemptActive = false;
    int m_currentTaskIndex = -1;
    QStringList m_actionResults;
    Page5ResultRecord m_attemptResult;
    QVector<Page5ResultRecord> m_runResults;
};
