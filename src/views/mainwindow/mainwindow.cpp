#include "mainwindow.h"
#include "mainwindowmodel.h"
#include "mainwindowviewmodel.h"
#include "messageservice.h"
#include "tablewidget.h"
#include "page1.h"
#include "page2.h"
#include "page3.h"
#include "page4.h"
#include "page5.h"
#include "page5viewmodel.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_model(nullptr)
    , m_viewModel(nullptr)
{
    // 初始化 Model 和 ViewModel
    m_model = new MainWindowModel(this);
    m_viewModel = new MainWindowViewModel(m_model, this);

    setupUI();
    setupMenuBar();
    setupConnections();
    setupMessageService();
}

MainWindow::~MainWindow()
{
    // Qt 父子關係會自動清理
}

void MainWindow::setupUI()
{
    // 設定視窗大小
    resize(1400, 720);

    // MainWindow title and Icon
    setWindowTitle("Automatic test system");
    setWindowIcon(QIcon(":/images/oscilloscope.png"));

    // 取得 ViewModel
    auto* p1VM = m_viewModel->page1ViewModel();
    auto* p2VM = m_viewModel->page2ViewModel();
    auto* p3VM = m_viewModel->page3ViewModel();
    auto* p4VM = m_viewModel->page4ViewModel();
    auto* p5VM = m_viewModel->page5ViewModel();

    // 建立 Pages（只在這裡創建一次）
    m_page1 = new Page1(p1VM, this);
    m_page2 = new Page2(p2VM, this);
    m_page3 = new Page3(p3VM, this);
    m_page4 = new Page4(p4VM, this);
    m_page5 = new Page5(p5VM, this);

    // 建立 TabWidget 並設置頁面
    m_tableWidget = new TableWidget(this);
    m_tableWidget->setPage1(m_page1);
    m_tableWidget->setPage2(m_page2);
    m_tableWidget->setPage3(m_page3);
    m_tableWidget->setPage4(m_page4);
    m_tableWidget->setPage5(m_page5);

    setCentralWidget(m_tableWidget);
}

void MainWindow::setupMenuBar()
{
    // Menu 內容設定
    QMenuBar* bar = menuBar();
    bar->setStyleSheet("QMenuBar { background-color: #2E3440; color: white; }");

    QMenu* fileMenu = bar->addMenu("檔案");
    QAction* openAct = fileMenu->addAction("開啟");
    QAction* saveAct = fileMenu->addAction("儲存");
    QAction* saveAsAct = fileMenu->addAction("另存新檔...");
    QAction* exitAct = fileMenu->addAction("離開");

    QMenu* helpMenu = bar->addMenu("幫助");
    helpMenu->addAction("使用說明");
    helpMenu->addAction("關於");

    // 連接動作到 slots
    connect(openAct, &QAction::triggered, this, &MainWindow::onLoadConfig);
    connect(saveAct, &QAction::triggered, this, &MainWindow::onSaveConfig);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveConfigAs);
    connect(exitAct, &QAction::triggered, this, &MainWindow::close);
}

void MainWindow::setupConnections()
{
    if (!m_viewModel) return;

    // ViewModel 請求對話框信號
    connect(m_viewModel, &MainWindowViewModel::requestSaveDialog,
            this, &MainWindow::onRequestSaveDialog);
    connect(m_viewModel, &MainWindowViewModel::requestLoadDialog,
            this, &MainWindow::onRequestLoadDialog);
    connect(m_viewModel, &MainWindowViewModel::showMessage,
            this, &MainWindow::onShowMessage);

    // ── Page5 執行中 → 鎖定其他所有 tab ─────────────────
    // Page5 是第 5 個 tab（index 4），鎖定時只保留它可點擊
    if (auto* p5vm = m_viewModel->page5ViewModel()) {
        connect(p5vm, &Page5ViewModel::runningChanged,
                this, [this](bool running) {
                    constexpr int PAGE5_TAB_INDEX = 4;
                    if (m_tableWidget)
                        m_tableWidget->setOtherTabsLocked(running, PAGE5_TAB_INDEX);
                    // MenuBar 鎖定：執行中禁止開啟檔案/儲存等操作
                    menuBar()->setEnabled(!running);
                });
    }
}

void MainWindow::setupMessageService()
{
    // 連接警告信號
    connect(&MessageService::instance(), &MessageService::warningRequested,
            this, [this](const QString& title, const QString& message) {
                QMessageBox::warning(this, title, message);
            });

    // 連接錯誤信號
    connect(&MessageService::instance(), &MessageService::errorRequested,
            this, [this](const QString& title, const QString& message) {
                QMessageBox::critical(this, title, message);
            });

    // 連接資訊信號
    connect(&MessageService::instance(), &MessageService::infoRequested,
            this, [this](const QString& title, const QString& message) {
                QMessageBox::information(this, title, message);
            });
}

void MainWindow::onSaveConfig()
{
    if (m_page1) m_page1->syncUIToViewModel();
    if (m_page2) m_page2->syncUIToViewModel();
    if (m_page3) m_page3->syncUIToViewModel();

    if (m_viewModel) {
        m_viewModel->saveConfig();
    }
}

void MainWindow::onSaveConfigAs()
{
    if (m_viewModel) {
        m_viewModel->saveConfigAs();
    }
}

void MainWindow::onLoadConfig()
{
    if (m_viewModel) {
        m_viewModel->loadConfig();
    }
}

void MainWindow::onRequestSaveDialog()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "另存新檔", QDir::homePath(), "XML Files (*.xml)");

    if (!fileName.isEmpty() && m_viewModel) {
        if (m_page1) m_page1->syncUIToViewModel();
        if (m_page2) m_page2->syncUIToViewModel();
        if (m_page3) m_page3->syncUIToViewModel();

        m_viewModel->onSaveDialogAccepted(fileName);
    }
}

void MainWindow::onRequestLoadDialog()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "載入設定檔",
        QDir::homePath(),
        "XML Files (*.xml)");

    if (!fileName.isEmpty() && m_viewModel) {
        m_viewModel->onLoadDialogAccepted(fileName);
    }
}

void MainWindow::onShowMessage(const QString& title, const QString& message, int type)
{
    switch (type) {
    case 0: // Information
        QMessageBox::information(this, title, message);
        break;
    case 1: // Warning
        QMessageBox::warning(this, title, message);
        break;
    case 2: // Critical
        QMessageBox::critical(this, title, message);
        break;
    default:
        QMessageBox::information(this, title, message);
        break;
    }
}
