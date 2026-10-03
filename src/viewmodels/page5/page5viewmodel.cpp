#include "../../infrastructure/worker/testrunsession.h"
#include "taskresultformatter.h"
#include "page5taskkind.h"
#include "page5taskdefaults.h"
#include "page5excelreport.h"
#include <algorithm>
#include "page5scopepolicy.h"
#include "oscilloscope.h"
#include "page5conditionmapping.h"
#include "messageservice.h"
#include "page5viewmodel.h"
#include "page5model.h"
#include "page5taskpayload.h"
#include "itestconditionprovider.h"
#include <QMetaObject>
#include <QDebug>

Page5ViewModel::Page5ViewModel(Page5Model *model, QObject *parent)
    : Page5ViewModel(model, makeTestRunSession(), parent)
{
}

Page5ViewModel::Page5ViewModel(Page5Model *model, std::unique_ptr<TestRunSession> session, QObject *parent)
    : QObject(parent), m_session(std::move(session)), m_model(model)
{
    if (!m_session)
        throw std::invalid_argument("Missing test run session");
    connect(m_model, &Page5Model::configLoaded, this, &Page5ViewModel::onConfigLoaded);
    connect(m_session.get(), &TestRunSession::runningChanged, this, &Page5ViewModel::setRunning);
    connect(m_session.get(), &TestRunSession::shutdownFinished, this, [this] {
        m_shutdownComplete = true;
        emit shutdownFinished();
    });
    connect(m_session.get(), &TestRunSession::taskStatusChanged, this, &Page5ViewModel::taskStatusChanged);
    connect(m_session.get(), &TestRunSession::retryCountChanged, this,
            &Page5ViewModel::taskRetryCountChanged);
    connect(m_session.get(), &TestRunSession::taskResultChanged, this, &Page5ViewModel::taskResultChanged);
    connect(m_session.get(), &TestRunSession::logMessage, this, &Page5ViewModel::logMessage);
    connect(m_session.get(), &TestRunSession::measurementReady, this,
            [this](int index, const Page5ResultRecord &record, bool complete) {
                emit taskResultChanged(index, TaskResultFormatter::measurement(record, complete));
            });
    connect(m_session.get(), &TestRunSession::resultRecordsReady, this,
            [this](const QVector<Page5ResultRecord> &records) {
                m_resultHistory += records;
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
Page5ViewModel::~Page5ViewModel() { m_session.reset(); }

//  資料存取：委託到 ITestConditionProvider
//  m_conditionProvider 有效 → 直接讀（即時、無副本）
//  m_conditionProvider 無效 → fallback 到 Page5Model（XML 還原路徑）
const Page1Config &Page5ViewModel::page1Config() const { return m_model->page1Config; }

const QVector<InputRow> &Page5ViewModel::inputRows() const
{
    return m_conditionProvider ? m_conditionProvider->inputRows() : m_model->inputRows;
}

const QVector<DcRow> &Page5ViewModel::dcSourceRows(int source) const
{
    static const QVector<DcRow> empty;
    return m_conditionProvider ? m_conditionProvider->dcSourceRows(source) : empty;
}

const QVector<QString> &Page5ViewModel::dcNames() const
{
    static const QVector<QString> empty;
    return m_conditionProvider ? m_conditionProvider->dcNames() : empty;
}

const LoadMetaRow &Page5ViewModel::loadMeta() const
{
    return m_conditionProvider ? m_conditionProvider->loadMeta() : m_model->loadMeta;
}

const QVector<LoadDataRow> &Page5ViewModel::loadRows() const
{
    return m_conditionProvider ? m_conditionProvider->loadRows() : m_model->loadRows;
}

const DynamicMetaRow &Page5ViewModel::dynamicMeta() const
{
    return m_conditionProvider ? m_conditionProvider->dynamicMeta() : m_model->dynamicMeta;
}

const QVector<DynamicDataRow> &Page5ViewModel::dynamicRows() const
{
    return m_conditionProvider ? m_conditionProvider->dynamicRows() : m_model->dynamicRows;
}

const QVector<RelayDataRow> &Page5ViewModel::relayRows() const
{
    return m_conditionProvider ? m_conditionProvider->relayRows() : m_model->relayRows;
}

const QVector<DutRowData> &Page5ViewModel::dutRows() const
{
    return m_model->tasks(); // DUT Test 是 Page5 自有資料，永遠讀 Page5Model
}

//  XML 序列化
//  writeXml：只寫 Page5 自有的 DutTable（Page2 資料由 Page2VM 負責存）
//  loadXml ：只讀 DutTable，Page2 資料在 Page2VM 載入後由 signal 更新
void Page5ViewModel::writeXml(QXmlStreamWriter &writer) const { m_model->writeXml(writer); }

void Page5ViewModel::loadXml(QXmlStreamReader &reader) { m_model->loadXml(reader); }

//  XML 載入完成 → 廣播所有 UI 刷新
void Page5ViewModel::onConfigLoaded() { broadcastAllData(); }

void Page5ViewModel::broadcastAllData()
{
    emit inputTableChanged();
    emit dcTableChanged();
    emit loadTableChanged();
    emit dynamicTableChanged();
    emit relayTableChanged();
    emit dutTableChanged();
    emit reportFileChanged();
}

//  Slot：Page1 配置更新
void Page5ViewModel::onPage1ConfigChanged(const Page1Config &cfg)
{
    m_model->page1Config = cfg;
    m_hasPage1Config = true;
    emit dcTableChanged();
}

//  Slots：Page2 資料變更
// 委託架構：不再複製資料到 Page5Model
//     資料由 m_conditionProvider 即時提供
//     這些 slot 只負責通知 UI 刷新
void Page5ViewModel::onInputDataChanged(const QVector<InputRow> &) { emit inputTableChanged(); }

void Page5ViewModel::onLoadMetaChanged(const LoadMetaRow &) { emit loadTableChanged(); }

void Page5ViewModel::onLoadRowsChanged(const QVector<LoadDataRow> &) { emit loadTableChanged(); }

void Page5ViewModel::onDynamicMetaChanged(const DynamicMetaRow &) { emit dynamicTableChanged(); }

void Page5ViewModel::onDynamicRowsChanged(const QVector<DynamicDataRow> &) { emit dynamicTableChanged(); }

void Page5ViewModel::onRelayRowsChanged(const QVector<RelayDataRow> &) { emit relayTableChanged(); }

//  Slot：DUT Test 表格變更（Page5 自有資料）
void Page5ViewModel::onDutRowsChanged(const QVector<DutRowData> &rows)
{
    if (!m_isRunning)
        m_model->replaceTasks(rows);
}

//  startExecution：由 View 呼叫
//  payloads 已在主執行緒預取，直接交給 Worker
Page5ExecutionContext Page5ViewModel::executionContext() const
{
    Page5ExecutionContext context{
        page1Config(), inputRows(),    loadMeta(),
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

bool Page5ViewModel::startExecution(const QVector<TaskPayload> &payloads)
{
    if (m_shutdownRequested || !m_controlAllowed || m_isRunning || payloads.isEmpty())
        return false;
    auto context = executionContext();
    auto resolved = payloads;
    for (auto &payload : resolved) {
        QString error;
        if (!Page5Conditions::validate(payload, context, error)) {
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
        if (!Page5ExcelReport::validate(runReportDirectory, runReportName, error)) {
            MessageService::instance().showWarning("Excel Report", error);
            return false;
        }
    }
    if (Page5ScopePolicy::firstScopeTask(resolved) >= 0) {
        QString model;
        for (const auto &instrument : page1Config().instruments)
            if (instrument.type == "Oscilloscope" && instrument.enabled)
                model = instrument.modelName;
        const QString unavailable = Page5ScopePolicy::unavailableReason(model);
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
        const QString actualUnavailable = Page5ScopePolicy::unavailableReason(m_scopeLease->model());
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
    return started;
}
void Page5ViewModel::stopExecution() { m_session->stop(); }
void Page5ViewModel::requestShutdown()
{
    m_shutdownRequested = true;
    m_session->requestShutdown();
}

//  setRunning（private）：唯一狀態更新點
//  Worker finished() → setRunning(false) 解鎖 UI
void Page5ViewModel::setRunning(bool running)
{
    if (m_isRunning == running)
        return;
    if (!running)
        m_scopeLease.reset();
    m_isRunning = running;
    emit runningChanged(running);
}

void Page5ViewModel::validateXml(QXmlStreamReader &reader) const
{
    Page5Model candidate;
    candidate.loadXml(reader);
}

void Page5ViewModel::publishXmlLoaded() { onConfigLoaded(); }

QString Page5ViewModel::reportName() const { return m_model->reportName; }
QString Page5ViewModel::reportDirectory() const { return m_model->reportDirectory; }
void Page5ViewModel::setReportFile(const QString &name, const QString &directory)
{
    if (m_isRunning)
        return;
    m_model->reportName = name;
    m_model->reportDirectory = directory;
}
int Page5ViewModel::appendTask(const DutRowData &row)
{
    if (m_isRunning)
        return -1;
    auto draft = row;
    if (draft.settings.isEmpty())
        draft.settings = page5DefaultSettings(draft.item);
    return m_model->appendTask(std::move(draft));
}
void Page5ViewModel::removeTask(int uid)
{
    if (!m_isRunning)
        m_model->removeTask(uid);
}
QVariantMap Page5ViewModel::taskSettings(int uid) const { return m_model->taskSettings(uid); }
QVariantMap Page5ViewModel::taskConfig(int uid, const QString &group) const
{
    return taskSettings(uid).value(group).toMap();
}
void Page5ViewModel::setTaskConfig(int uid, const QString &group, const QVariantMap &config)
{
    if (!m_isRunning)
        m_model->setTaskConfig(uid, group, config);
}
QVector<TaskPayload> Page5ViewModel::taskPayloads(const QVector<RunTask> &tasks) const
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
        result.append({task, found->settings.value(Page5TaskKind::settingsGroup(found->item)).toMap(),
                       found->ext, valid ? retry : -1, found->report});
    }
    return result;
}
