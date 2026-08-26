#include "page5viewmodel.h"
#include "page5model.h"
#include "page5taskpayload.h"
#include "page5testworker.h"
#include "page2viewmodel.h"
#include <QThread>
#include <QMetaObject>
#include <QDebug>

Page5ViewModel::Page5ViewModel(Page5Model *model, QObject *parent)
    : QObject{parent}
    , m_model(model)
{
    connect(m_model, &Page5Model::configLoaded,
            this,    &Page5ViewModel::onConfigLoaded);

    // ── 建立 Worker 與執行緒 ──────────────────────────────
    qRegisterMetaType<QVector<TaskPayload>>("QVector<TaskPayload>");
    qRegisterMetaType<Page5RunPanel::TaskStatus>("Page5RunPanel::TaskStatus");

    m_worker       = new Page5TestWorker(this);
    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);
    m_workerThread->start();

    // 示波器量測策略由 AppService 注入（透過 setOscStrategy）

    // Worker 輸出 → ViewModel signal 轉發（queued，自動跨執行緒）
    connect(m_worker, &Page5TestWorker::taskStatusChanged,
            this,     &Page5ViewModel::taskStatusChanged);
    connect(m_worker, &Page5TestWorker::retryCountChanged,
            this,     &Page5ViewModel::taskRetryCountChanged);
    connect(m_worker, &Page5TestWorker::logMessage,
            this,     &Page5ViewModel::logMessage);
    connect(m_worker, &Page5TestWorker::finished,
            this,     [this]() { setRunning(false); });
}

Page5ViewModel::~Page5ViewModel()
{
    if (m_worker)
        m_worker->stop();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    delete m_worker;
}

// ─────────────────────────────────────────────
//  資料存取：委託到 Page2ViewModel
//  m_page2ViewModel 有效 → 直接讀（即時、無副本）
//  m_page2ViewModel 無效 → fallback 到 Page5Model（XML 還原路徑）
// ─────────────────────────────────────────────
const Page1Config& Page5ViewModel::page1Config() const
{
    return m_model->page1Config;
}

const QVector<InputRow>& Page5ViewModel::inputRows() const
{
    return m_page2ViewModel ? m_page2ViewModel->inputRows()
                            : m_model->inputRows;
}

const LoadMetaRow& Page5ViewModel::loadMeta() const
{
    return m_page2ViewModel ? m_page2ViewModel->loadMeta()
                            : m_model->loadMeta;
}

const QVector<LoadDataRow>& Page5ViewModel::loadRows() const
{
    return m_page2ViewModel ? m_page2ViewModel->loadRows()
                            : m_model->loadRows;
}

const DynamicMetaRow& Page5ViewModel::dynamicMeta() const
{
    return m_page2ViewModel ? m_page2ViewModel->dynamicMeta()
                            : m_model->dynamicMeta;
}

const QVector<DynamicDataRow>& Page5ViewModel::dynamicRows() const
{
    return m_page2ViewModel ? m_page2ViewModel->dynamicRows()
                            : m_model->dynamicRows;
}

const QVector<RelayDataRow>& Page5ViewModel::relayRows() const
{
    return m_page2ViewModel ? m_page2ViewModel->relayRows()
                            : m_model->relayRows;
}

const QVector<DutRowData>& Page5ViewModel::dutRows() const
{
    return m_model->dutRows;   // DUT Test 是 Page5 自有資料，永遠讀 Page5Model
}

// ─────────────────────────────────────────────
//  XML 序列化
//  writeXml：只寫 Page5 自有的 DutTable（Page2 資料由 Page2VM 負責存）
//  loadXml ：只讀 DutTable，Page2 資料在 Page2VM 載入後由 signal 更新
// ─────────────────────────────────────────────
void Page5ViewModel::writeXml(QXmlStreamWriter& writer) const
{
    m_model->writeXml(writer);
}

void Page5ViewModel::loadXml(QXmlStreamReader& reader)
{
    m_model->loadXml(reader);
}

// ─────────────────────────────────────────────
//  XML 載入完成 → 廣播所有 UI 刷新
// ─────────────────────────────────────────────
void Page5ViewModel::onConfigLoaded()
{
    broadcastAllData();
}

void Page5ViewModel::broadcastAllData()
{
    emit inputTableChanged();
    emit loadTableChanged();
    emit dynamicTableChanged();
    emit relayTableChanged();
    emit dutTableChanged();
}

// ─────────────────────────────────────────────
//  Slot：Page1 配置更新
// ─────────────────────────────────────────────
void Page5ViewModel::onPage1ConfigChanged(const Page1Config &cfg)
{
    m_model->page1Config = cfg;
    m_hasPage1Config = true;
}

// ─────────────────────────────────────────────
//  Slots：Page2 資料變更
//  ★ 委託架構：不再複製資料到 Page5Model
//     資料由 m_page2ViewModel 即時提供
//     這些 slot 只負責通知 UI 刷新
// ─────────────────────────────────────────────
void Page5ViewModel::onInputDataChanged(const QVector<InputRow>&)
{
    emit inputTableChanged();
}

void Page5ViewModel::onLoadMetaChanged(const LoadMetaRow&)
{
    emit loadTableChanged();
}

void Page5ViewModel::onLoadRowsChanged(const QVector<LoadDataRow>&)
{
    emit loadTableChanged();
}

void Page5ViewModel::onDynamicMetaChanged(const DynamicMetaRow&)
{
    emit dynamicTableChanged();
}

void Page5ViewModel::onDynamicRowsChanged(const QVector<DynamicDataRow>&)
{
    emit dynamicTableChanged();
}

void Page5ViewModel::onRelayRowsChanged(const QVector<RelayDataRow>&)
{
    emit relayTableChanged();
}

// ─────────────────────────────────────────────
//  Slot：DUT Test 表格變更（Page5 自有資料）
// ─────────────────────────────────────────────
void Page5ViewModel::onDutRowsChanged(const QVector<DutRowData>& rows)
{
    m_model->dutRows = rows;
}

// ─────────────────────────────────────────────
//  startExecution：由 View 呼叫
//  payloads 已在主執行緒預取，直接交給 Worker
// ─────────────────────────────────────────────
void Page5ViewModel::startExecution(const QVector<TaskPayload>& payloads)
{
    setRunning(true);
    QMetaObject::invokeMethod(m_worker, "startTasks",
                              Qt::QueuedConnection,
                              Q_ARG(QVector<TaskPayload>, payloads));
}

// ─────────────────────────────────────────────
//  stopExecution：由 View 呼叫
// ─────────────────────────────────────────────
void Page5ViewModel::stopExecution()
{
    m_worker->stop();
    setRunning(false);
}

// ─────────────────────────────────────────────
//  setRunning（private）：唯一狀態更新點
//  Worker finished() → setRunning(false) 解鎖 UI
// ─────────────────────────────────────────────
void Page5ViewModel::setRunning(bool running)
{
    if (m_isRunning == running) return;
    m_isRunning = running;
    emit runningChanged(running);
}
