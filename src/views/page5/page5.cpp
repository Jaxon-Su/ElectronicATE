#include "page5.h"
#include "page5viewmodel.h"
#include "page5style.h"
#include <QHBoxLayout>
#include <QDebug>

static constexpr int BTN_W = 18;

Page5::Page5(Page5ViewModel* viewModel, QWidget* parent)
    : QWidget(parent)
    , m_viewModel(viewModel)
{
    setStyleSheet(Page5Style::MAIN);
    setupUi();
    setupConnections();
}

// ─────────────────────────────────────────────
//  setupUi：與原版相同
// ─────────────────────────────────────────────
void Page5::setupUi()
{
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(0);

    m_leftPanel   = new Page5LeftPanel(this);
    m_centerPanel = new Page5CenterPanel(m_viewModel, this);
    m_rightPanel  = new Page5RightPanel(m_viewModel, this);

    m_centerPanel->setTaskList(m_leftPanel->taskNames());

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_leftPanel);
    m_mainSplitter->addWidget(m_centerPanel);
    m_mainSplitter->addWidget(m_rightPanel);

    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    m_mainSplitter->setSizes({220 + SidePanel::BTN_W, 824, 260 + SidePanel::BTN_W});
    m_mainSplitter->setChildrenCollapsible(true);
    m_mainSplitter->setStyleSheet(R"(
        QSplitter::handle { background-color: #b0b8c8; }
        QSplitter::handle:hover { background-color: #3399ff; }
        QSplitter::handle:horizontal { width: 5px; }
    )");

    rootLayout->addWidget(m_mainSplitter);
}

// ─────────────────────────────────────────────
//  collectPayloads
//  在主執行緒從 CenterPanel 預取設定
//  Worker 不需持有任何 UI 指標
// ─────────────────────────────────────────────
QVector<TaskPayload> Page5::collectPayloads(const QVector<RunTask>& tasks) const
{
    QVector<TaskPayload> payloads;
    payloads.reserve(tasks.size());
    for (const auto& t : tasks) {
        const int uid = t.dutUid;
        QVariantMap cfg;
        if      (t.name == "Delay")              cfg = m_centerPanel->delaySettings(uid);
        else if (t.name == "Write Oscilloscope") cfg = m_centerPanel->oscSettings(uid);
        else if (t.name == "Turn on")            cfg = m_centerPanel->turnOnSettings(uid);
        else if (t.name == "Turn off")           cfg = m_centerPanel->turnOffSettings(uid);
        else if (t.name == "Relay")              cfg = m_centerPanel->relaySettings(uid);

        else if (t.name == "Static Test") cfg = m_centerPanel->staticSettings(uid);
        else if (t.name == "Dynamic Test") cfg = m_centerPanel->dynamicSettings(uid);
        else if (t.name == "Short then turn on") cfg = m_centerPanel->shortOnSettings(uid);
        else if (t.name == "Turn on then short") cfg = m_centerPanel->onShortSettings(uid);

        const DutRowData row = m_centerPanel->dutRowForUid(uid);
        payloads.append({ t, cfg, row.ext, row.retry.toInt(), row.report });
    }
    return payloads;
}

// ─────────────────────────────────────────────
//  setupConnections
// ─────────────────────────────────────────────
void Page5::setupConnections()
{
    // ── 左側收折/展開 ──
    connect(m_leftPanel, &SidePanel::expandedChanged,
            this, [this](bool expanded) {
                const QList<int> cur   = m_mainSplitter->sizes();
                const int        total = cur[0] + cur[1];
                if (!expanded)
                    m_mainSplitter->setSizes({BTN_W, total - BTN_W, cur[2]});
                else {
                    const int leftW = qMin(220 + SidePanel::BTN_W, total - 100);
                    m_mainSplitter->setSizes({leftW, total - leftW, cur[2]});
                }
            });

    // ── 右側收折/展開 ──
    connect(m_rightPanel, &SidePanel::expandedChanged,
            this, [this](bool expanded) {
                const QList<int> cur   = m_mainSplitter->sizes();
                const int        total = cur[1] + cur[2];
                if (!expanded)
                    m_mainSplitter->setSizes({cur[0], total - BTN_W, BTN_W});
                else {
                    const int rightW = qMin(260 + SidePanel::BTN_W, total - 100);
                    m_mainSplitter->setSizes({cur[0], total - rightW, rightW});
                }
            });

    // ── 左側雙擊 Task → 中間面板新增列 ──
    connect(m_leftPanel,   &Page5LeftPanel::taskDoubleClicked,
            m_centerPanel, &Page5CenterPanel::addDutTestRow);

    if (!m_viewModel) return;

    // ── ViewModel 資料更新 → 右側面板刷新 ──
    connect(m_viewModel, &Page5ViewModel::inputTableChanged,
            m_rightPanel, &Page5RightPanel::refreshInputTable);
    connect(m_viewModel, &Page5ViewModel::loadTableChanged,
            m_rightPanel, &Page5RightPanel::refreshLoadTable);
    connect(m_viewModel, &Page5ViewModel::dynamicTableChanged,
            m_rightPanel, &Page5RightPanel::refreshDynamicTable);
    connect(m_viewModel, &Page5ViewModel::relayTableChanged,
            m_rightPanel, &Page5RightPanel::refreshRelayTable);

    // ── DUT Test 雙向連接 ──
    connect(m_centerPanel, &Page5CenterPanel::dutRowsChanged,
            m_viewModel,   &Page5ViewModel::onDutRowsChanged);
    connect(m_viewModel,   &Page5ViewModel::dutTableChanged,
            m_centerPanel, &Page5CenterPanel::loadDutRows);

    // ── RunPanel Run/Stop ────────────────────────────────
    auto* rp = m_centerPanel->runPanel();

    // Run：預取 UI 設定 → 交給 ViewModel 執行（View 不碰 Worker）
    connect(rp, &Page5RunPanel::runRequested,
            this, [this](const QVector<RunTask>& tasks) {
                m_centerPanel->runPanel()->resetTaskRows();
                const QVector<TaskPayload> payloads = collectPayloads(tasks);
                if (!m_viewModel->startExecution(payloads) && !m_viewModel->isRunning())
                    m_centerPanel->runPanel()->setExecutionRunning(false);
            });

    connect(m_viewModel, &Page5ViewModel::runningChanged, rp, &Page5RunPanel::setExecutionRunning);

    // Stop：委託 ViewModel
    connect(rp, &Page5RunPanel::stopRequested,
            m_viewModel, &Page5ViewModel::stopExecution);

    // ── runningChanged → 鎖定/解鎖所有子面板 ────────────
    connect(m_viewModel, &Page5ViewModel::runningChanged,
            m_centerPanel, &Page5CenterPanel::setLocked);
    connect(m_viewModel, &Page5ViewModel::runningChanged,
            m_leftPanel,   &Page5LeftPanel::setLocked);
    connect(m_viewModel, &Page5ViewModel::runningChanged,
            this, [this](bool running) {
                if (running)
                    m_centerPanel->switchToRunTab();
                m_rightPanel->setEnabled(!running);
                for (int i = 1; i < m_mainSplitter->count(); ++i)
                    m_mainSplitter->handle(i)->setEnabled(!running);
            });

    // ── ViewModel 轉發 Worker 輸出 → View UI ─────────────
    connect(m_viewModel, &Page5ViewModel::taskStatusChanged,
            this, [this](int idx, Page5RunPanel::TaskStatus status) {
                m_centerPanel->runPanel()->setTaskStatus(idx, status);
            });
    connect(m_viewModel, &Page5ViewModel::taskRetryCountChanged,
            this, [this](int idx, int attempt) {
                m_centerPanel->runPanel()->setRetryCount(idx, attempt);
            });
    connect(m_viewModel, &Page5ViewModel::logMessage,
            m_centerPanel, &Page5CenterPanel::appendLog);

    // ── 初始同步 ──
    m_viewModel->broadcastAllData();
}
