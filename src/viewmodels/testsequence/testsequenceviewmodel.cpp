#include "../../infrastructure/worker/testrunsession.h"
#include "taskresultformatter.h"
#include "testsequencetaskkind.h"
#include "testsequencetaskdefaults.h"
#include "testsequenceexcelreport.h"
#include <algorithm>
#include "testsequencescopepolicy.h"
#include "oscilloscope.h"
#include "testsequenceconditionmapping.h"
#include "messageservice.h"
#include "testsequenceviewmodel.h"
#include "testsequencemodel.h"
#include "testsequencetaskpayload.h"
#include "itestconditionprovider.h"
#include <QMetaObject>
#include <QDebug>

TestSequenceViewModel::TestSequenceViewModel(TestSequenceModel *model, QObject *parent)
    : TestSequenceViewModel(model, makeTestRunSession(), parent)
{
}

TestSequenceViewModel::TestSequenceViewModel(TestSequenceModel *model, std::unique_ptr<TestRunSession> session, QObject *parent)
    : QObject(parent), m_session(std::move(session)), m_model(model)
{
    if (!m_session)
        throw std::invalid_argument("Missing test run session");
    connect(m_model, &TestSequenceModel::configLoaded, this, &TestSequenceViewModel::onConfigLoaded);
    connect(m_session.get(), &TestRunSession::runningChanged, this, &TestSequenceViewModel::setRunning);
    connect(m_session.get(), &TestRunSession::shutdownFinished, this, [this] {
        m_shutdownComplete = true;
        emit shutdownFinished();
    });
    connect(m_session.get(), &TestRunSession::taskStatusChanged, this, &TestSequenceViewModel::taskStatusChanged);
    connect(m_session.get(), &TestRunSession::retryCountChanged, this,
            &TestSequenceViewModel::taskRetryCountChanged);
    connect(m_session.get(), &TestRunSession::taskResultChanged, this, &TestSequenceViewModel::taskResultChanged);
    connect(m_session.get(), &TestRunSession::logMessage, this, &TestSequenceViewModel::logMessage);
    connect(m_session.get(), &TestRunSession::measurementReady, this,
            [this](int index, const TestSequenceResultRecord &record, bool complete) {
                m_resultsModel.updateRecord(record);
                emit taskResultChanged(index, TaskResultFormatter::measurement(record, complete));
            });
    connect(m_session.get(), &TestRunSession::outputsUncertain, this,
            [this](const InstrumentSetupConfig &config, const QVector<TableKind> &outputs,
                   const ShortCircuitProtection &protection) {
                m_outputRecoveryRequired = true;
                emit outputsUncertain(config, outputs, protection);
            });
    connect(m_session.get(), &TestRunSession::resultRecordsReady, this,
            [this](const QVector<TestSequenceResultRecord> &records) {
                m_resultHistory += records;
                for (const auto &record : records) m_resultsModel.updateRecord(record);
                emit resultRecordsReady(records);
            });
    connect(m_session.get(), &TestRunSession::reportCompleted, this,
            [this](bool success, const QString &path, const QString &error) {
                if (success)
                    emit logMessage("Excel report saved: " + path);
                else {
                    emit logMessage("Excel report failed: " + error);
                    MessageService::instance().showWarning(
                        "Excel Report",
                        error + "\nResults remain in memory. Check the report folder before the next Run.");
                }
            });
}
TestSequenceViewModel::~TestSequenceViewModel() { m_session.reset(); }

//  資料存取：委託到 ITestConditionProvider
//  m_conditionProvider 有效 → 直接讀（即時、無副本）
//  m_conditionProvider 無效 → fallback 到 TestSequenceModel（XML 還原路徑）
const InstrumentSetupConfig &TestSequenceViewModel::instrumentSetupConfig() const { return m_model->instrumentSetupConfig; }

const QVector<InputRow> &TestSequenceViewModel::inputRows() const
{
    return m_conditionProvider ? m_conditionProvider->inputRows() : m_model->inputRows;
}

const QVector<DcRow> &TestSequenceViewModel::dcSourceRows(int source) const
{
    static const QVector<DcRow> empty;
    return m_conditionProvider ? m_conditionProvider->dcSourceRows(source) : empty;
}

const QVector<QString> &TestSequenceViewModel::dcNames() const
{
    static const QVector<QString> empty;
    return m_conditionProvider ? m_conditionProvider->dcNames() : empty;
}

const LoadMetaRow &TestSequenceViewModel::loadMeta() const
{
    return m_conditionProvider ? m_conditionProvider->loadMeta() : m_model->loadMeta;
}

const QVector<LoadDataRow> &TestSequenceViewModel::loadRows() const
{
    return m_conditionProvider ? m_conditionProvider->loadRows() : m_model->loadRows;
}

const DynamicMetaRow &TestSequenceViewModel::dynamicMeta() const
{
    return m_conditionProvider ? m_conditionProvider->dynamicMeta() : m_model->dynamicMeta;
}

const QVector<DynamicDataRow> &TestSequenceViewModel::dynamicRows() const
{
    return m_conditionProvider ? m_conditionProvider->dynamicRows() : m_model->dynamicRows;
}

const QVector<RelayDataRow> &TestSequenceViewModel::relayRows() const
{
    return m_conditionProvider ? m_conditionProvider->relayRows() : m_model->relayRows;
}

const QVector<DutRowData> &TestSequenceViewModel::dutRows() const
{
    return m_model->tasks(); // DUT Test 是 TestSequence 自有資料，永遠讀 TestSequenceModel
}

//  XML 序列化
//  writeXml：只寫 TestSequence 自有的 DutTable（TestConditions 資料由 TestConditionsVM 負責存）
//  loadXml ：只讀 DutTable，TestConditions 資料在 TestConditionsVM 載入後由 signal 更新
void TestSequenceViewModel::writeXml(QXmlStreamWriter &writer) const { m_model->writeXml(writer); }

void TestSequenceViewModel::loadXml(QXmlStreamReader &reader) { m_model->loadXml(reader); }

std::function<void()> TestSequenceViewModel::captureXmlRollback()
{
    // XML loads deliberately allocate fresh UIDs. A failed transaction must
    // instead restore the identities still held by the table, Run panel and editors.
    return [model = m_model, rows = m_model->tasks(), name = m_model->reportName,
            directory = m_model->reportDirectory] {
        model->replaceTasks(rows);
        model->reportName = name;
        model->reportDirectory = directory;
    };
}

//  XML 載入完成 → 廣播所有 UI 刷新
void TestSequenceViewModel::onConfigLoaded() { broadcastAllData(); }

void TestSequenceViewModel::broadcastAllData()
{
    emit inputTableChanged();
    emit dcTableChanged();
    emit loadTableChanged();
    emit dynamicTableChanged();
    emit relayTableChanged();
    emit dutTableChanged();
    emit reportFileChanged();
}

//  Slot：InstrumentSetup 配置更新
void TestSequenceViewModel::onInstrumentSetupConfigChanged(const InstrumentSetupConfig &cfg)
{
    m_model->instrumentSetupConfig = cfg;
    m_hasInstrumentSetupConfig = true;
    emit dcTableChanged();
}

//  Slots：TestConditions 資料變更
// 委託架構：不再複製資料到 TestSequenceModel
//     資料由 m_conditionProvider 即時提供
//     這些 slot 只負責通知 UI 刷新
void TestSequenceViewModel::onInputDataChanged(const QVector<InputRow> &) { emit inputTableChanged(); }

void TestSequenceViewModel::onLoadMetaChanged(const LoadMetaRow &) { emit loadTableChanged(); }

void TestSequenceViewModel::onLoadRowsChanged(const QVector<LoadDataRow> &) { emit loadTableChanged(); }

void TestSequenceViewModel::onDynamicMetaChanged(const DynamicMetaRow &) { emit dynamicTableChanged(); }

void TestSequenceViewModel::onDynamicRowsChanged(const QVector<DynamicDataRow> &) { emit dynamicTableChanged(); }

void TestSequenceViewModel::onRelayRowsChanged(const QVector<RelayDataRow> &) { emit relayTableChanged(); }

//  Slot：DUT Test 表格變更（TestSequence 自有資料）
void TestSequenceViewModel::onDutRowsChanged(const QVector<DutRowData> &rows)
{
    if (!m_isRunning)
        m_model->replaceTasks(rows);
}

//  startExecution：由 View 呼叫
//  payloads 已在主執行緒預取，直接交給 Worker
TestSequenceExecutionContext TestSequenceViewModel::executionContext() const
{
    TestSequenceExecutionContext context{
        instrumentSetupConfig(), inputRows(),    loadMeta(),
        loadRows(),    dynamicMeta(),  dynamicRows(),
        relayRows(),   oscilloscope(), m_loadSyncProvider ? m_loadSyncProvider() : LoadSyncSettings{}};
    int count = dcNames().size();
    for (int source = 0; source < 3; ++source)
        count = qMax(count, int(dcSourceRows(source).size()));
    for (int row = 0; row < count; ++row) {
        DcGroup group;
        for (int source = 0; source < 3; ++source)
            group[source] = dcSourceRows(source).value(row);
        context.dcInputs.append(group);
        context.dcLabels.append(dcNames().value(row));
    }
    context.reportDirectory = reportDirectory();
    return context;
}

bool TestSequenceViewModel::startExecution(const QVector<TaskPayload> &payloads)
{
    if (m_shutdownRequested || !m_controlAllowed || m_isRunning || payloads.isEmpty())
        return false;
    if (m_outputRecoveryRequired) {
        if (!m_outputRecoveryCheck || !m_outputRecoveryCheck()) {
            MessageService::instance().showWarning("Output state UNKNOWN", tr("請先在 ManualControl 成功關閉未確認的輸出，再開始測試。"));
            return false;
        }
        m_outputRecoveryRequired = false;
    }
    auto context = executionContext();
    auto resolved = payloads;
    for (auto &payload : resolved) {
        QString error;
        if (!TestSequenceConditions::validate(payload, context, error)) {
            MessageService::instance().showWarning("Task Configuration", payload.task.name + ": " + error);
            return false;
        }
    }
    const bool writeRunReport =
        std::any_of(resolved.cbegin(), resolved.cend(), [](const auto &payload) { return payload.report; });
    const QString runReportName = reportName();
    const QString runReportDirectory = reportDirectory();
    if (writeRunReport) {
        QString error;
        if (!TestSequenceExcelReport::validate(runReportDirectory, runReportName, error)) {
            MessageService::instance().showWarning("Excel Report", error);
            return false;
        }
    }
    if (TestSequenceScopePolicy::firstScopeTask(resolved) >= 0) {
        QString model;
        for (const auto &instrument : instrumentSetupConfig().instruments)
            if (instrument.type == "Oscilloscope" && instrument.enabled)
                model = instrument.modelName;
        const QString unavailable = TestSequenceScopePolicy::unavailableReason(model);
        if (!unavailable.isEmpty()) {
            MessageService::instance().showWarning("Oscilloscope", unavailable);
            return false;
        }
        m_scopeLease = m_scopeProvider ? m_scopeProvider() : nullptr;
        if (!m_scopeLease) {
            MessageService::instance().showWarning("Oscilloscope",
                                                   tr("示波器尚未連線或正在使用中，無法開始測試。"));
            return false;
        }
        const QString actualUnavailable = TestSequenceScopePolicy::unavailableReason(m_scopeLease->model());
        if (!actualUnavailable.isEmpty()) {
            m_scopeLease.reset();
            MessageService::instance().showWarning("Oscilloscope", actualUnavailable);
            return false;
        }
    }
    const bool started =
        m_session->start(resolved, context, m_scopeLease, writeRunReport, runReportName, runReportDirectory);
    if (!started)
        m_scopeLease.reset();
    else
        m_resultsModel.beginRun();
    return started;
}
void TestSequenceViewModel::stopExecution() { m_session->stop(); }
void TestSequenceViewModel::requestShutdown()
{
    m_shutdownRequested = true;
    m_session->requestShutdown();
}

//  setRunning（private）：唯一狀態更新點
//  Worker finished() → setRunning(false) 解鎖 UI
void TestSequenceViewModel::setRunning(bool running)
{
    if (m_isRunning == running)
        return;
    if (!running)
        m_scopeLease.reset();
    m_isRunning = running;
    emit runningChanged(running);
}

void TestSequenceViewModel::validateXml(QXmlStreamReader &reader) const
{
    TestSequenceModel candidate;
    candidate.loadXml(reader);
}

void TestSequenceViewModel::publishXmlLoaded() { onConfigLoaded(); }

QString TestSequenceViewModel::reportName() const { return m_model->reportName; }
QString TestSequenceViewModel::reportDirectory() const { return m_model->reportDirectory; }
void TestSequenceViewModel::setReportFile(const QString &name, const QString &directory)
{
    if (m_isRunning)
        return;
    m_model->reportName = name;
    m_model->reportDirectory = directory;
}
void TestSequenceViewModel::setAllTasksReport(bool enabled)
{
    if (m_isRunning || m_shutdownRequested)
        return;
    auto rows = m_model->tasks();
    bool changed = false;
    for (auto &row : rows) {
        changed |= row.report != enabled;
        row.report = enabled;
    }
    if (!changed)
        return;
    m_model->replaceTasks(std::move(rows));
    emit dutTableChanged();
}
int TestSequenceViewModel::appendTask(const DutRowData &row)
{
    if (m_isRunning)
        return -1;
    auto draft = row;
    if (draft.settings.isEmpty())
        draft.settings = testSequenceDefaultSettings(draft.item);
    return m_model->appendTask(std::move(draft));
}
void TestSequenceViewModel::removeTask(int uid)
{
    if (!m_isRunning)
        m_model->removeTask(uid);
}
QVariantMap TestSequenceViewModel::taskSettings(int uid) const { return m_model->taskSettings(uid); }
QVariantMap TestSequenceViewModel::taskConfig(int uid, const QString &group) const
{
    return taskSettings(uid).value(group).toMap();
}
void TestSequenceViewModel::setTaskConfig(int uid, const QString &group, const QVariantMap &config)
{
    if (!m_isRunning)
        m_model->setTaskConfig(uid, group, config);
}
QVector<TaskPayload> TestSequenceViewModel::taskPayloads(const QVector<RunTask> &tasks) const
{
    QVector<TaskPayload> result;
    for (const auto &task : tasks) {
        const auto found =
            std::find_if(m_model->tasks().cbegin(), m_model->tasks().cend(),
                         [&](const auto &row) { return row.uid == task.dutUid && row.active; });
        if (found == m_model->tasks().cend() || found->item != task.name)
            return {};
        bool valid = false;
        const int retry = found->retry.toInt(&valid);
        result.append({task, found->settings.value(TestSequenceTaskKind::settingsGroup(found->item)).toMap(),
                       found->ext, valid ? retry : -1, found->report});
    }
    return result;
}
