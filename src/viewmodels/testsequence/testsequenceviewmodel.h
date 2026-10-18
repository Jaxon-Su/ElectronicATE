#pragma once
#include "testsequenceresultrecord.h"
#include "testsequenceresultsmodel.h"
#include <QObject>
#include <functional>
#include <memory>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "instrumentsetupconfig.h"
#include "testconditionsconfig.h"
#include "dutrowdata.h"
#include "testsequencetaskpayload.h"
#include "taskstatus.h"
#include "ixmlserializable.h"

class TestSequenceModel;
class ITestConditionProvider;
class Oscilloscope;
class TestRunSession;

class TestSequenceViewModel : public QObject, public IXmlSerializable
{
    Q_OBJECT
  public:
    void setControlAllowed(bool allowed) { m_controlAllowed = allowed; }
    void setOutputRecoveryCheck(std::function<bool()> check) { m_outputRecoveryCheck = std::move(check); }
    explicit TestSequenceViewModel(TestSequenceModel *model, QObject *parent = nullptr);
    TestSequenceViewModel(TestSequenceModel *model, std::unique_ptr<TestRunSession> session, QObject *parent = nullptr);
    ~TestSequenceViewModel() override;

    // Borrowed provider; nullptr restores the model-backed fallback.
    void setConditionProvider(const ITestConditionProvider *provider) { m_conditionProvider = provider; }

    const InstrumentSetupConfig &instrumentSetupConfig() const;
    const QVector<InputRow> &inputRows() const;
    const QVector<DcRow> &dcSourceRows(int source) const;
    const QVector<QString> &dcNames() const;
    const LoadMetaRow &loadMeta() const;
    const QVector<LoadDataRow> &loadRows() const;
    const DynamicMetaRow &dynamicMeta() const;
    const QVector<DynamicDataRow> &dynamicRows() const;
    const QVector<RelayDataRow> &relayRows() const;
    const QVector<DutRowData> &dutRows() const;
    int appendTask(const DutRowData &row);
    void removeTask(int uid);
    QVariantMap taskSettings(int uid) const;
    QVariantMap taskConfig(int uid, const QString &group) const;
    void setTaskConfig(int uid, const QString &group, const QVariantMap &config);
    QVector<TaskPayload> taskPayloads(const QVector<RunTask> &tasks) const;

    void setLoadSyncProvider(std::function<LoadSyncSettings()> provider)
    {
        m_loadSyncProvider = std::move(provider);
    }

    Oscilloscope *oscilloscope() const { return m_scopeLease.get(); }
    void setOscilloscopeProvider(std::function<std::shared_ptr<Oscilloscope>()> provider)
    {
        m_scopeProvider = std::move(provider);
    }

    QString xmlTagName() const override { return "TestSequence"; }
    void writeXml(QXmlStreamWriter &writer) const override;
    void publishXmlLoaded() override;
    void validateXml(QXmlStreamReader &reader) const override;
    void loadXml(QXmlStreamReader &reader) override;
    std::function<void()> captureXmlRollback() override;

    TestSequenceExecutionContext executionContext() const;

    QString reportName() const;
    QString reportDirectory() const;
    void setReportFile(const QString &name, const QString &directory);
    void setAllTasksReport(bool enabled);
    void requestShutdown();
    bool isShutdownComplete() const { return m_shutdownComplete; }
    bool isRunning() const { return m_isRunning; }
    const QVector<TestSequenceResultRecord> &resultHistory() const { return m_resultHistory; }
    TestSequenceResultsModel *resultsModel() { return &m_resultsModel; }

  public slots:
    void onInstrumentSetupConfigChanged(const InstrumentSetupConfig &cfg);
    void onInputDataChanged(const QVector<InputRow> &);
    void onLoadMetaChanged(const LoadMetaRow &);
    void onLoadRowsChanged(const QVector<LoadDataRow> &);
    void onDynamicMetaChanged(const DynamicMetaRow &);
    void onDynamicRowsChanged(const QVector<DynamicDataRow> &);
    void onRelayRowsChanged(const QVector<RelayDataRow> &);
    void onDutRowsChanged(const QVector<DutRowData> &rows);

    // 執行控制：由 View 呼叫，Worker 生命週期由 ViewModel 管理
    bool startExecution(const QVector<TaskPayload> &payloads);
    void stopExecution();

    void broadcastAllData();
    void onConfigLoaded();

  signals:
    void outputsUncertain(const InstrumentSetupConfig &config, const QVector<TableKind> &outputs,
                          const ShortCircuitProtection &protection = {});
    void inputTableChanged();
    void dcTableChanged();
    void loadTableChanged();
    void dynamicTableChanged();
    void relayTableChanged();
    void dutTableChanged();
    void reportFileChanged();
    void runningChanged(bool running);
    void shutdownFinished();

    // Worker 輸出轉發給 View（queued 跨執行緒後再由 ViewModel 廣播）
    void taskStatusChanged(int runPanelIndex, TaskStatus status);
    void taskRetryCountChanged(int runPanelIndex, int attempt);
    void logMessage(const QString &msg);
    void taskResultChanged(int runPanelIndex, const QString &result);
    void resultRecordsReady(const QVector<TestSequenceResultRecord> &records);

  private slots:
    void setRunning(bool running);

  private:
    QVector<TestSequenceResultRecord> m_resultHistory;
    TestSequenceResultsModel m_resultsModel;
    std::unique_ptr<TestRunSession> m_session;
    bool m_shutdownRequested = false;
    bool m_shutdownComplete = false;
    bool m_controlAllowed = true;
    bool m_outputRecoveryRequired = false;
    std::function<bool()> m_outputRecoveryCheck;
    TestSequenceModel *m_model = nullptr;
    const ITestConditionProvider *m_conditionProvider = nullptr;
    std::function<LoadSyncSettings()> m_loadSyncProvider;
    std::function<std::shared_ptr<Oscilloscope>()> m_scopeProvider;
    std::shared_ptr<Oscilloscope> m_scopeLease;
    bool m_hasInstrumentSetupConfig = false;
    bool m_isRunning = false;
};
