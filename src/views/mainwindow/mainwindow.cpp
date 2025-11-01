#include "mainwindow.h"
#include "mainwindowmodel.h"
#include "mainwindowviewmodel.h"
#include "messageservice.h"
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

    // 設定中央 Widget
    setCentralWidget(m_viewModel->getMainWidget());

    setupMessageService();
}

MainWindow::~MainWindow()
{
    // Qt 父子關係會自動清理
}

void MainWindow::setupUI()
{
    // 設定視窗大小
    resize(1180, 720);

    // MainWindow title and Icon
    setWindowTitle("Automatic test system");
    setWindowIcon(QIcon(":/images/oscilloscope.png"));
}

void MainWindow::setupMenuBar()
{
    // Menu 內容設定
    QMenuBar* bar = menuBar();
    menuBar()->setStyleSheet("QMenuBar { background-color: #2E3440; color: white; }");

    QMenu* fileMenu = menuBar()->addMenu("檔案");
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
        this,
        "另存新檔",
        QDir::homePath(),
        "XML Files (*.xml)"
        );

    if (!fileName.isEmpty() && m_viewModel) {
        m_viewModel->onSaveDialogAccepted(fileName);
    }
}

void MainWindow::onRequestLoadDialog()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "載入設定檔",
        QDir::homePath(),
        "XML Files (*.xml)"
        );

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
