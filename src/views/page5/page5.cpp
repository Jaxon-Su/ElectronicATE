#include "page5.h"
#include "page5viewmodel.h"
#include "page5style.h"
#include <QHBoxLayout>
#include <QDebug>

static constexpr int BTN_W = 18;

Page5::Page5(Page5ViewModel *viewModel, QWidget *parent) : QWidget(parent), m_viewModel(viewModel)
{
    setStyleSheet(Page5Style::MAIN);
    setupUi();
    setupConnections();
}

void Page5::setupUi()
{
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(0);

    m_leftPanel = new Page5LeftPanel(this);
    m_centerPanel = new Page5CenterPanel(m_viewModel, this);
    m_rightPanel = new Page5RightPanel(m_viewModel, this);

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

//  setupConnections
void Page5::setupConnections()
{
    // 左側收折/展開
    connect(m_leftPanel, &SidePanel::expandedChanged, this, [this](bool expanded) {
        const QList<int> cur = m_mainSplitter->sizes();
        const int total = cur[0] + cur[1];
        if (!expanded)
            m_mainSplitter->setSizes({BTN_W, total - BTN_W, cur[2]});
        else {
            const int leftW = qMin(220 + SidePanel::BTN_W, total - 100);
            m_mainSplitter->setSizes({leftW, total - leftW, cur[2]});
        }
    });

    // 右側收折/展開
    connect(m_rightPanel, &SidePanel::expandedChanged, this, [this](bool expanded) {
        const QList<int> cur = m_mainSplitter->sizes();
        const int total = cur[1] + cur[2];
        if (!expanded)
            m_mainSplitter->setSizes({cur[0], total - BTN_W, BTN_W});
        else {
            const int rightW = qMin(260 + SidePanel::BTN_W, total - 100);
            m_mainSplitter->setSizes({cur[0], total - rightW, rightW});
        }
    });

    // 左側雙擊 Task → 中間面板新增列
    connect(m_leftPanel, &Page5LeftPanel::taskDoubleClicked, m_centerPanel, &Page5CenterPanel::addDutTestRow);

    if (!m_viewModel)
        return;

    // DUT Test 雙向連接
    connect(m_centerPanel, &Page5CenterPanel::dutRowsChanged, m_viewModel, &Page5ViewModel::onDutRowsChanged);
    connect(m_viewModel, &Page5ViewModel::dutTableChanged, m_centerPanel, &Page5CenterPanel::loadDutRows);

    // RunPanel Run/Stop
    auto *rp = m_centerPanel->runPanel();

    // Run：預取 UI 設定 → 交給 ViewModel 執行（View 不碰 Worker）
    connect(rp, &Page5RunPanel::runRequested, this, [this](const QVector<RunTask> &tasks) {
        m_centerPanel->runPanel()->resetTaskRows();
        const QVector<TaskPayload> payloads = m_viewModel->taskPayloads(tasks);
        if (!m_viewModel->startExecution(payloads) && !m_viewModel->isRunning())
            m_centerPanel->runPanel()->setExecutionRunning(false);
    });

    connect(m_viewModel, &Page5ViewModel::runningChanged, rp, &Page5RunPanel::setExecutionRunning);

    // Stop：委託 ViewModel
    connect(rp, &Page5RunPanel::stopRequested, m_viewModel, &Page5ViewModel::stopExecution);

    // runningChanged → 鎖定/解鎖所有子面板
    connect(m_viewModel, &Page5ViewModel::runningChanged, m_centerPanel, &Page5CenterPanel::setLocked);
    connect(m_viewModel, &Page5ViewModel::runningChanged, m_leftPanel, &Page5LeftPanel::setLocked);
    connect(m_viewModel, &Page5ViewModel::runningChanged, this, [this](bool running) {
        if (running) {
            m_centerPanel->clearResults();
            m_centerPanel->switchToRunTab();
        }
        m_rightPanel->setEnabled(!running);
        for (int i = 1; i < m_mainSplitter->count(); ++i)
            m_mainSplitter->handle(i)->setEnabled(!running);
    });

    // ViewModel 轉發 Worker 輸出 → View UI
    connect(m_viewModel, &Page5ViewModel::taskStatusChanged, this,
            [this](int idx, TaskStatus status) { m_centerPanel->runPanel()->setTaskStatus(idx, status); });
    connect(m_viewModel, &Page5ViewModel::taskRetryCountChanged, this,
            [this](int idx, int attempt) { m_centerPanel->runPanel()->setRetryCount(idx, attempt); });
    connect(m_viewModel, &Page5ViewModel::taskResultChanged, m_centerPanel, &Page5CenterPanel::setTaskResult);
    connect(m_viewModel, &Page5ViewModel::logMessage, m_centerPanel, &Page5CenterPanel::appendLog);

    connect(m_viewModel, &Page5ViewModel::inputTableChanged, m_centerPanel, &Page5CenterPanel::clearResults);
    connect(m_viewModel, &Page5ViewModel::dcTableChanged, m_centerPanel, &Page5CenterPanel::clearResults);
    connect(m_viewModel, &Page5ViewModel::loadTableChanged, m_centerPanel, &Page5CenterPanel::clearResults);
    connect(m_viewModel, &Page5ViewModel::dynamicTableChanged, m_centerPanel,
            &Page5CenterPanel::clearResults);
    connect(m_viewModel, &Page5ViewModel::relayTableChanged, m_centerPanel, &Page5CenterPanel::clearResults);

    // 初始同步
    m_viewModel->broadcastAllData();
}
