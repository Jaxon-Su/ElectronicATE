#include "mainwindow.h"
#include "mainwindowmodel.h"
#include "mainwindowviewmodel.h"
#include "messageservice.h"
#include "mainpagetabs.h"
#include "page1.h"
#include "page2.h"
#include "page3.h"
#include "page4.h"
#include "page5.h"
#include "page5viewmodel.h"
#include "page3viewmodel.h"
#include "page4viewmodel.h"
#include "../../ui/controlpagelock.h"
#include "../../ui/manualcontrolclose.h"
#include "../page1/dialogs/commscandialog.h"
#include "../../viewmodels/page1/commscanviewmodel.h"
#include "../../infrastructure/discovery/commscanprocess.h"
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QCloseEvent>
#include <QTimer>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_model(nullptr), m_viewModel(nullptr)
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
    resize(1400, 760);

    // MainWindow title and Icon
    setWindowTitle("Automatic test system - v0.1.0-alpha.1");
    setWindowIcon(QIcon(":/images/oscilloscope.png"));

    // 取得 ViewModel
    auto *p1VM = m_viewModel->page1ViewModel();
    auto *p2VM = m_viewModel->page2ViewModel();
    auto *p3VM = m_viewModel->page3ViewModel();
    auto *p4VM = m_viewModel->page4ViewModel();
    auto *p5VM = m_viewModel->page5ViewModel();

    // 建立 Pages（只在這裡創建一次）
    m_page1 = new Page1(p1VM, this);
    m_page2 = new Page2(p2VM, this);
    m_page3 = new Page3(p3VM, this);
    m_page4 = new Page4(p4VM, this);
    m_page5 = new Page5(p5VM, this);

    // 建立 TabWidget 並設置頁面
    m_pageTabs = new MainPageTabs(this);
    m_pageTabs->setPage1(m_page1);
    m_pageTabs->setPage2(m_page2);
    m_pageTabs->setPage3(m_page3);
    m_pageTabs->setPage4(m_page4);
    m_pageTabs->setPage5(m_page5);

    setCentralWidget(m_pageTabs);
}

void MainWindow::setupMenuBar()
{
    // Menu 內容設定
    QMenuBar *bar = menuBar();
    bar->setStyleSheet("QMenuBar { background-color: #2E3440; color: white; }");

    QMenu *fileMenu = bar->addMenu("檔案");
    QAction *openAct = fileMenu->addAction("開啟");
    QAction *saveAct = fileMenu->addAction("儲存");
    QAction *saveAsAct = fileMenu->addAction("另存新檔...");
    QAction *exitAct = fileMenu->addAction("離開");

    QMenu *helpMenu = bar->addMenu("幫助");
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
    if (!m_viewModel)
        return;

    connect(m_page1, &Page1::scanCommRequested, this, [this] {
        auto *p3 = m_viewModel->page3ViewModel();
        if (p3->isConfigurationBusy() || p3->hasActiveControl() ||
            m_viewModel->page4ViewModel()->isControlActive() || m_viewModel->page5ViewModel()->isRunning())
            return;
        CommScanProcess process(QCoreApplication::applicationFilePath());
        CommScanViewModel viewModel(&process);
        CommScanDialog dialog(&viewModel, this);
        dialog.exec();
    });

    // ViewModel 請求對話框信號
    connect(m_viewModel, &MainWindowViewModel::requestSaveDialog, this, &MainWindow::onRequestSaveDialog);
    connect(m_viewModel, &MainWindowViewModel::requestLoadDialog, this, &MainWindow::onRequestLoadDialog);
    connect(m_viewModel, &MainWindowViewModel::showMessage, this, &MainWindow::onShowMessage);

    connect(m_viewModel, &MainWindowViewModel::controlStateChanged, this, [this] {
        updateControlPageLock();
        if (m_closeRequested)
            QTimer::singleShot(0, this, &QWidget::close);
    });
    connect(m_viewModel->page5ViewModel(), &Page5ViewModel::shutdownFinished, this, [this] {
        if (m_closeRequested)
            QTimer::singleShot(0, this, &QWidget::close);
    });
    updateControlPageLock();
}

void MainWindow::setupMessageService()
{
    // 連接警告信號
    connect(
        &MessageService::instance(), &MessageService::warningRequested, this,
        [this](const QString &title, const QString &message) { QMessageBox::warning(this, title, message); });

    // 連接錯誤信號
    connect(&MessageService::instance(), &MessageService::errorRequested, this,
            [this](const QString &title, const QString &message) {
                QMessageBox::critical(this, title, message);
            });

    // 連接資訊信號
    connect(&MessageService::instance(), &MessageService::infoRequested, this,
            [this](const QString &title, const QString &message) {
                QMessageBox::information(this, title, message);
            });
}

void MainWindow::onSaveConfig()
{
    if (m_page1)
        m_page1->syncUIToViewModel();
    if (m_page2)
        m_page2->syncUIToViewModel();
    if (m_page3)
        m_page3->syncUIToViewModel();

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
    QString fileName = QFileDialog::getSaveFileName(this, "另存新檔", QDir::homePath(), "XML Files (*.xml)");

    if (!fileName.isEmpty() && m_viewModel) {
        if (m_page1)
            m_page1->syncUIToViewModel();
        if (m_page2)
            m_page2->syncUIToViewModel();
        if (m_page3)
            m_page3->syncUIToViewModel();

        m_viewModel->onSaveDialogAccepted(fileName);
    }
}

void MainWindow::onRequestLoadDialog()
{
    QString fileName =
        QFileDialog::getOpenFileName(this, "載入設定檔", QDir::homePath(), "XML Files (*.xml)");

    if (!fileName.isEmpty() && m_viewModel) {
        m_viewModel->onLoadDialogAccepted(fileName);
    }
}

void MainWindow::onShowMessage(const QString &title, const QString &message, int type)
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

void MainWindow::updateControlPageLock()
{
    auto *p3 = m_viewModel->page3ViewModel();
    auto *p4 = m_viewModel->page4ViewModel();
    auto *p5 = m_viewModel->page5ViewModel();
    const std::array<bool, 3> active{p3->hasActiveControl(), p4->isControlActive(), p5->isRunning()};
    applyControlPageLock(m_pageTabs->tabWidget(), active);
    m_page1->setCommScanEnabled(!active[0] && !active[1] && !active[2] && !p3->isConfigurationBusy());
    menuBar()->setEnabled(!active[0] && !active[1] && !active[2]);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    auto *p3 = m_viewModel->page3ViewModel();
    auto *p4 = m_viewModel->page4ViewModel();
    auto *p5 = m_viewModel->page5ViewModel();
    if (p5->isRunning()) {
        m_closeRequested = true;
        p5->requestShutdown();
        statusBar()->showMessage(tr("正在停止測試與保存報表，完成後自動關閉。"));
        event->ignore();
    } else if (p3->hasPendingControlOperation()) {
        statusBar()->showMessage(tr("請等待 Page3 背景操作結束，再關閉視窗。"));
        event->ignore();
    } else if (p3->hasActiveControl() && !m_outputExitConfirmed) {
        event->ignore();
        if (confirmActiveOutputExit(this, p3->hasUnknownOutput()) && !p3->hasPendingControlOperation()) {
            m_outputExitConfirmed = true;
            m_closeRequested = true;
            p3->setControlAllowed(false);
            centralWidget()->setEnabled(false);
            QTimer::singleShot(0, this, &QWidget::close);
        }
    } else if (p4->isControlActive()) {
        p4->disconnect();
        statusBar()->showMessage(tr("正在中止通訊並斷線，完成後請再關閉視窗。"));
        event->ignore();
    } else if (!p5->isShutdownComplete()) {
        m_closeRequested = true;
        p5->requestShutdown();
        event->ignore();
    } else
        QMainWindow::closeEvent(event);
}
