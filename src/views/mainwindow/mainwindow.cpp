#include "mainwindow.h"
#include "aboutdialog.h"
#include "applicationinfo.h"
#include "mainwindowmodel.h"
#include "mainwindowviewmodel.h"
#include "messageservice.h"
#include "mainpagetabs.h"
#include "instrumentsetuppage.h"
#include "testconditionspage.h"
#include "manualcontrolpage.h"
#include "commandconsolepage.h"
#include "testsequencepage.h"
#include "waveformocrpage.h"
#include "../wfmviewer/wfmviewerpage.h"
#include "waveformocrviewmodel.h"
#include "testsequenceviewmodel.h"
#include "manualcontrolviewmodel.h"
#include "commandconsoleviewmodel.h"
#include "../../ui/controlpagelock.h"
#include "../../ui/manualcontrolclose.h"
#include "../instrumentsetup/dialogs/commscandialog.h"
#include "../../viewmodels/instrumentsetup/commscanviewmodel.h"
#include "../../infrastructure/discovery/commscanprocess.h"
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QCloseEvent>
#include <QTimer>
#include <QStatusBar>
#include <QPushButton>

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
    setWindowTitle(QString("Automatic test system - v%1").arg(QString::fromUtf8(ApplicationInfo::Version)));
    setWindowIcon(QIcon(":/images/oscilloscope.png"));

    // 取得 ViewModel
    auto *instrumentSetupViewModel = m_viewModel->instrumentSetupViewModel();
    auto *testConditionsViewModel = m_viewModel->testConditionsViewModel();
    auto *manualControlViewModel = m_viewModel->manualControlViewModel();
    auto *commandConsoleViewModel = m_viewModel->commandConsoleViewModel();
    auto *testSequenceViewModel = m_viewModel->testSequenceViewModel();

    // 建立 Pages（只在這裡創建一次）
    m_instrumentSetup = new InstrumentSetupPage(instrumentSetupViewModel, this);
    m_testConditions = new TestConditionsPage(testConditionsViewModel, this);
    m_manualControl = new ManualControlPage(manualControlViewModel, this);
    m_commandConsole = new CommandConsolePage(commandConsoleViewModel, this);
    m_testSequence = new TestSequencePage(testSequenceViewModel, this);
    m_waveformOcr = new WaveformOcrPage(m_viewModel->waveformOcrViewModel(), this);
    m_wfmViewer = new WfmViewerPage(m_viewModel->wfmViewerViewModel(), this);

    // 建立 TabWidget 並設置頁面
    m_pageTabs = new MainPageTabs(this);
    m_pageTabs->setInstrumentSetup(m_instrumentSetup);
    m_pageTabs->setTestConditions(m_testConditions);
    m_pageTabs->setManualControl(m_manualControl);
    m_pageTabs->setCommandConsole(m_commandConsole);
    m_pageTabs->setTestSequence(m_testSequence);
    m_pageTabs->setWaveformOcr(m_waveformOcr);
    m_pageTabs->setWfmViewer(m_wfmViewer);

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
    // helpMenu->addAction("使用說明");
    auto *aboutAct = helpMenu->addAction("關於");
    aboutAct->setObjectName("aboutAction");
    connect(aboutAct, &QAction::triggered, this, [this] {
        AboutDialog dialog(this);
        dialog.exec();
    });

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

    connect(m_instrumentSetup, &InstrumentSetupPage::scanCommRequested, this, [this] {
        auto *manualControl = m_viewModel->manualControlViewModel();
        if (manualControl->isConfigurationBusy() || manualControl->hasActiveControl() ||
            m_viewModel->commandConsoleViewModel()->isControlActive() || m_viewModel->testSequenceViewModel()->isRunning())
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
            QTimer::singleShot(0, this, [this] { if (m_closeRequested) close(); });
    });
    connect(m_viewModel->testSequenceViewModel(), &TestSequenceViewModel::shutdownFinished, this, [this] {
        if (m_closeRequested)
            QTimer::singleShot(0, this, [this] { if (m_closeRequested) close(); });
    });
    connect(m_viewModel->waveformOcrViewModel(), &WaveformOcrViewModel::changed, this, [this] {
        auto *ocr = m_viewModel->waveformOcrViewModel();
        if (m_waveformOcrCloseState == WaveformOcrCloseState::Exporting && !ocr->busy()) {
            if (ocr->hasUnexportedResults()) {
                const auto error = ocr->message();
                cancelClose();
                statusBar()->showMessage(error);
                // Keep nested close events out of this modal error notification.
                m_waveformOcrCloseState = WaveformOcrCloseState::Prompting;
                QMessageBox::warning(this, tr("WaveformOcr 匯出未完成"),
                                     error + tr("\n結果已保留，視窗不會關閉。"));
                m_waveformOcrCloseState = WaveformOcrCloseState::Undecided;
                return;
            }
            m_waveformOcrCloseState = WaveformOcrCloseState::Undecided;
        }
        if (m_closeRequested && !ocr->busy())
            QTimer::singleShot(0, this, [this] { if (m_closeRequested) close(); });
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
    if (m_instrumentSetup)
        m_instrumentSetup->syncUIToViewModel();
    if (m_testConditions)
        m_testConditions->syncUIToViewModel();
    if (m_manualControl)
        m_manualControl->syncUIToViewModel();

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
        if (m_instrumentSetup)
            m_instrumentSetup->syncUIToViewModel();
        if (m_testConditions)
            m_testConditions->syncUIToViewModel();
        if (m_manualControl)
            m_manualControl->syncUIToViewModel();

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
    auto *manualControl = m_viewModel->manualControlViewModel();
    auto *commandConsole = m_viewModel->commandConsoleViewModel();
    auto *testSequence = m_viewModel->testSequenceViewModel();
    const std::array<bool, 3> active{manualControl->hasActiveControl(), commandConsole->isControlActive(), testSequence->isRunning()};
    applyControlPageLock(m_pageTabs->tabWidget(), active);
    m_instrumentSetup->setCommScanEnabled(!active[0] && !active[1] && !active[2] && !manualControl->isConfigurationBusy());
    menuBar()->setEnabled(centralWidget()->isEnabled() && !active[0] && !active[1] && !active[2]);
}

void MainWindow::disableClosingUi()
{
    centralWidget()->setEnabled(false);
    menuBar()->setEnabled(false);
}

void MainWindow::cancelClose()
{
    // Invalidate queued retries and the previous output-exit consent. Do not
    // clear output states/short protection or permanently shut down TestSequence.
    m_closeRequested = false;
    m_outputExitConfirmed = false;
    m_waveformOcrCloseState = WaveformOcrCloseState::Undecided;
    centralWidget()->setEnabled(true);
    m_viewModel->updateControlPermissions();
    statusBar()->clearMessage();
}

bool MainWindow::prepareWaveformOcrClose()
{
    auto *ocr = m_viewModel->waveformOcrViewModel();
    if (ocr->busy()) {
        m_closeRequested = true;
        disableClosingUi();
        ocr->stop();
        return false; // Wait for idle before deciding what to do with completed rows.
    }
    m_waveformOcr->finishEditing();
    if (!ocr->hasUnexportedResults() || m_waveformOcrCloseState == WaveformOcrCloseState::Discarded)
        return true;

    m_closeRequested = false;
    m_waveformOcrCloseState = WaveformOcrCloseState::Prompting;
    QMessageBox dialog(QMessageBox::Question, tr("WaveformOcr 尚未匯出"),
                       tr("WaveformOcr 有尚未匯出的辨識結果或修改。\n是否先匯出 Excel 再關閉？"),
                       QMessageBox::NoButton, this);
    dialog.setObjectName("waveformOcrCloseDialog");
    auto *save = dialog.addButton(tr("匯出"), QMessageBox::AcceptRole);
    auto *discard = dialog.addButton(tr("放棄"), QMessageBox::DestructiveRole);
    auto *cancel = dialog.addButton(tr("取消"), QMessageBox::RejectRole);
    save->setObjectName("waveformOcrCloseExport");
    discard->setObjectName("waveformOcrCloseDiscard");
    cancel->setObjectName("waveformOcrCloseCancel");
    dialog.setDefaultButton(cancel);
    dialog.setEscapeButton(cancel);
    dialog.exec();
    if (dialog.clickedButton() == discard) {
        m_waveformOcrCloseState = WaveformOcrCloseState::Discarded;
        return true;
    }
    if (dialog.clickedButton() != save) {
        cancelClose();
        return false;
    }

    QFileDialog file(this, tr("匯出 WaveformOcr 結果後關閉"));
    file.setObjectName("waveformOcrCloseExportDialog");
    file.setAcceptMode(QFileDialog::AcceptSave);
    file.setFileMode(QFileDialog::AnyFile);
    file.setNameFilter(tr("Excel (*.xlsx)"));
    file.setDefaultSuffix("xlsx");
    file.setOption(QFileDialog::DontConfirmOverwrite);
    file.selectFile(ocr->suggestedExportPath());
    if (file.exec() != QDialog::Accepted || file.selectedFiles().isEmpty()) {
        cancelClose();
        return false;
    }
    m_waveformOcrCloseState = WaveformOcrCloseState::Exporting;
    m_closeRequested = true;
    disableClosingUi();
    statusBar()->showMessage(tr("正在匯出 WaveformOcr 結果，成功後關閉。"));
    ocr->exportResults(file.selectedFiles().first());
    return false; // Only successful completion may resume the close sequence.
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_waveformOcrCloseState == WaveformOcrCloseState::Prompting ||
        m_waveformOcrCloseState == WaveformOcrCloseState::Exporting) {
        event->ignore(); // Repeated X must not cancel the export or open another dialog.
        return;
    }
    auto *manualControl = m_viewModel->manualControlViewModel();
    auto *commandConsole = m_viewModel->commandConsoleViewModel();
    auto *testSequence = m_viewModel->testSequenceViewModel();
    if (testSequence->isRunning()) {
        m_closeRequested = true;
        // Keep the worker reusable if cleanup leaves UNKNOWN and the user
        // returns to controls. Commit shutdown only after the output decision.
        testSequence->stopExecution();
        statusBar()->showMessage(tr("正在停止測試與保存報表，完成後自動關閉。"));
        event->ignore();
    } else if (manualControl->hasPendingControlOperation()) {
        statusBar()->showMessage(tr("請等待 ManualControl 背景操作結束，再關閉視窗。"));
        event->ignore();
    } else if (manualControl->hasActiveControl() && !m_outputExitConfirmed) {
        event->ignore();
        // Cancel queued close retries before entering the modal event loop.
        m_closeRequested = false;
        if (confirmActiveOutputExit(this, manualControl->hasUnknownOutput()) && !manualControl->hasPendingControlOperation()) {
            m_outputExitConfirmed = true;
            m_closeRequested = true;
            manualControl->setControlAllowed(false);
            disableClosingUi();
            QTimer::singleShot(0, this, &QWidget::close);
        }
    } else if (commandConsole->isControlActive()) {
        commandConsole->disconnect();
        statusBar()->showMessage(tr("正在中止通訊並斷線，完成後請再關閉視窗。"));
        event->ignore();
    } else if (!prepareWaveformOcrClose()) {
        event->ignore();
    } else if (!testSequence->isShutdownComplete()) {
        // WaveformOcr Cancel/export failure must leave this session reusable. Commit
        // shutdown only after the output decision AND the WaveformOcr decision.
        m_closeRequested = true;
        disableClosingUi();
        testSequence->requestShutdown();
        event->ignore();
    } else
        QMainWindow::closeEvent(event);
}
