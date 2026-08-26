#include "page4.h"
#include "page4viewmodel.h"
#include "page4model.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QScrollBar>
#include <QShortcut>

Page4::Page4(Page4ViewModel *viewModel, QWidget *parent)
    : QWidget(parent)
    , m_viewModel(viewModel)
{
    setupUI();
    setupConnections();
    updateConnectionUI(false);
    updateAddressHistory();
    updateCommandHistory();
}

void Page4::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // ==================== 連線區域 ====================
    QGroupBox *connectionGroup = new QGroupBox(tr("連線設定"), this);
    QGridLayout *connLayout = new QGridLayout(connectionGroup);

    // 地址輸入
    QLabel *addrLabel = new QLabel(tr("Address:"), this);
    m_addressCombo = new QComboBox(this);
    m_addressCombo->setEditable(true);
    m_addressCombo->setMinimumWidth(300);
    m_addressCombo->lineEdit()->setPlaceholderText(
        tr("例: GPIB0::30::INSTR 或 TCPIP::192.168.1.100::5025::SOCKET"));

    // 連線按鈕
    m_connectBtn = new QPushButton(tr("Connect"), this);
    m_connectBtn->setFixedWidth(100);
    m_connectBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #4CAF50;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #45a049; }
        QPushButton:pressed { background-color: #3d8b40; }
        QPushButton:disabled { background-color: #cccccc; }
    )");

    m_disconnectBtn = new QPushButton(tr("Disconnect"), this);
    m_disconnectBtn->setFixedWidth(100);
    m_disconnectBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #f44336;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #da190b; }
        QPushButton:pressed { background-color: #c1170a; }
        QPushButton:disabled { background-color: #cccccc; }
    )");

    // 狀態標籤
    m_statusLabel = new QLabel(tr("● 未連線"), this);
    m_statusLabel->setStyleSheet("color: gray; font-weight: bold;");

    // Timeout 設定
    QLabel *timeoutLabel = new QLabel(tr("Timeout (ms):"), this);
    m_timeoutSpin = new QSpinBox(this);
    m_timeoutSpin->setRange(Page4Model::MIN_TIMEOUT, Page4Model::MAX_TIMEOUT);
    m_timeoutSpin->setValue(Page4Model::DEFAULT_TIMEOUT);
    m_timeoutSpin->setSingleStep(100);

    connLayout->addWidget(addrLabel, 0, 0);
    connLayout->addWidget(m_addressCombo, 0, 1, 1, 3);
    connLayout->addWidget(m_connectBtn, 0, 4);
    connLayout->addWidget(m_disconnectBtn, 0, 5);
    connLayout->addWidget(m_statusLabel, 1, 1, 1, 2);
    connLayout->addWidget(timeoutLabel, 1, 3);
    connLayout->addWidget(m_timeoutSpin, 1, 4);

    // ==================== 指令區域 ====================
    QGroupBox *commandGroup = new QGroupBox(tr("指令輸入"), this);
    QGridLayout *cmdLayout = new QGridLayout(commandGroup);

    // ── 多行指令輸入框（支援自動換行）──────────────────────────────
    QLabel *cmdLabel = new QLabel(tr("Command:"), this);
    m_commandEdit = new QPlainTextEdit(this);
    m_commandEdit->setPlaceholderText(tr("輸入 SCPI 指令，Ex: *IDN?多行指令以換行分隔，Ctrl+Enter 發送"));
    m_commandEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);  // 自動換行
    m_commandEdit->setMinimumHeight(96);
    m_commandEdit->setFont(QFont("Consolas", 10));

    // Ctrl+Enter 發送（Enter 本身用於換行）
    auto *sendShortcut = new QShortcut(QKeySequence("Ctrl+Return"), m_commandEdit);
    connect(sendShortcut, &QShortcut::activated, this, &Page4::onSendClicked);

    // ── 歷史下拉選單（唯讀，選取後填入輸入框）────────────────────
    QLabel *histLabel = new QLabel(tr("History:"), this);
    m_commandHistory = new QComboBox(this);
    m_commandHistory->setToolTip(tr("選取後自動填入上方輸入框"));

    m_sendBtn = new QPushButton(tr("Send\n(Ctrl+↵)"), this);
    m_sendBtn->setFixedWidth(80);
    m_sendBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2196F3;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #1976D2; }
        QPushButton:pressed { background-color: #1565C0; }
        QPushButton:disabled { background-color: #cccccc; }
    )");

    // 常用指令按鈕
    QHBoxLayout *quickBtnLayout = new QHBoxLayout;
    quickBtnLayout->setSpacing(8);

    m_idnBtn = new QPushButton("*IDN?", this);
    m_rstBtn = new QPushButton("*RST", this);
    m_clsBtn = new QPushButton("*CLS", this);
    m_opcBtn = new QPushButton("*OPC?", this);

    QString quickBtnStyle = R"(
        QPushButton {
            background-color: #607D8B;
            color: white;
            border: none;
            padding: 4px 10px;
            border-radius: 3px;
        }
        QPushButton:hover { background-color: #546E7A; }
        QPushButton:pressed { background-color: #455A64; }
        QPushButton:disabled { background-color: #cccccc; }
    )";

    m_idnBtn->setStyleSheet(quickBtnStyle);
    m_rstBtn->setStyleSheet(quickBtnStyle);
    m_clsBtn->setStyleSheet(quickBtnStyle);
    m_opcBtn->setStyleSheet(quickBtnStyle);

    m_idnBtn->setToolTip(tr("查詢儀器識別碼"));
    m_rstBtn->setToolTip(tr("重置儀器"));
    m_clsBtn->setToolTip(tr("清除狀態暫存器"));
    m_opcBtn->setToolTip(tr("查詢操作完成"));

    quickBtnLayout->addWidget(new QLabel(tr("快速指令:"), this));
    quickBtnLayout->addWidget(m_idnBtn);
    quickBtnLayout->addWidget(m_rstBtn);
    quickBtnLayout->addWidget(m_clsBtn);
    quickBtnLayout->addWidget(m_opcBtn);
    quickBtnLayout->addStretch();

    cmdLayout->addWidget(cmdLabel,         0, 0, Qt::AlignTop);
    cmdLayout->addWidget(m_commandEdit,    0, 1);
    cmdLayout->addWidget(m_sendBtn,        0, 2, Qt::AlignTop);
    cmdLayout->addWidget(histLabel,        1, 0);
    cmdLayout->addWidget(m_commandHistory, 1, 1, 1, 2);
    cmdLayout->addLayout(quickBtnLayout,   2, 0, 1, 3);

    // ==================== 日誌區域 ====================
    QGroupBox *logGroup = new QGroupBox(tr("通訊記錄"), this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);

    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Consolas", 10));
    m_logView->setStyleSheet(R"(
        QTextEdit {
            background-color: #1e1e1e;
            color: #d4d4d4;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
        }
    )");

    m_clearBtn = new QPushButton(tr("Clear Log"), this);
    m_clearBtn->setFixedWidth(100);

    QHBoxLayout *logBtnLayout = new QHBoxLayout;
    logBtnLayout->addStretch();
    logBtnLayout->addWidget(m_clearBtn);

    logLayout->addWidget(m_logView);
    logLayout->addLayout(logBtnLayout);

    // ==================== 主佈局 ====================
    mainLayout->addWidget(connectionGroup);
    mainLayout->addWidget(commandGroup);
    mainLayout->addWidget(logGroup, 1);  // 讓日誌區域擴展

    setLayout(mainLayout);
}

void Page4::setupConnections()
{
    // 按鈕連接
    connect(m_connectBtn, &QPushButton::clicked,
            this, &Page4::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked,
            this, &Page4::onDisconnectClicked);
    connect(m_sendBtn, &QPushButton::clicked,
            this, &Page4::onSendClicked);
    connect(m_clearBtn, &QPushButton::clicked,
            this, &Page4::onClearClicked);

    // 快速指令按鈕
    connect(m_idnBtn, &QPushButton::clicked, this, &Page4::onIDNClicked);
    connect(m_rstBtn, &QPushButton::clicked, this, &Page4::onRSTClicked);
    connect(m_clsBtn, &QPushButton::clicked, this, &Page4::onCLSClicked);
    connect(m_opcBtn, &QPushButton::clicked, this, &Page4::onOPCClicked);

    // Timeout 設定
    connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_viewModel, &Page4ViewModel::setTimeout);

    // 歷史選單選取後填入輸入框
    connect(m_commandHistory, &QComboBox::currentTextChanged,
            this, &Page4::onCommandHistorySelected);

    // ViewModel 信號
    connect(m_viewModel, &Page4ViewModel::connectionStatusChanged,
            this, &Page4::onConnectionStatusChanged);
    connect(m_viewModel, &Page4ViewModel::responseReceived,
            this, &Page4::onResponseReceived);
    connect(m_viewModel, &Page4ViewModel::errorOccurred,
            this, &Page4::onErrorOccurred);
    connect(m_viewModel, &Page4ViewModel::historyUpdated,
            this, &Page4::onHistoryUpdated);
    connect(m_viewModel, &Page4ViewModel::commandSent,
            this, [this](const QString &cmd) {
                appendLog(tr("[TX] %1").arg(cmd), "#00ff00");
            });
}

// ==================== 按鈕事件 ====================

void Page4::onConnectClicked()
{
    QString address = m_addressCombo->currentText().trimmed();
    if (address.isEmpty()) {
        appendLog(tr("[錯誤] 請輸入有效的地址"), "red");
        return;
    }

    appendLog(tr("[資訊] 正在連線到 %1...").arg(address), "yellow");
    m_viewModel->connectToAddress(address);
}

void Page4::onDisconnectClicked()
{
    m_viewModel->disconnect();
    appendLog(tr("[資訊] 已斷開連線"), "yellow");
}

void Page4::onSendClicked()
{
    // toPlainText() 保留換行；trimmed() 去掉首尾空白
    QString command = m_commandEdit->toPlainText().trimmed();
    if (command.isEmpty()) {
        return;
    }

    m_viewModel->sendCommand(command);

    // 清空輸入框
    m_commandEdit->clear();
}

void Page4::onClearClicked()
{
    m_logView->clear();
}

void Page4::onCommandHistorySelected(const QString &command)
{
    // 歷史下拉選單選取後，將指令填入多行輸入框（不自動發送）
    if (!command.isEmpty()) {
        m_commandEdit->setPlainText(command);
        m_commandEdit->setFocus();
        // 將游標移到末尾，方便繼續編輯
        QTextCursor cursor = m_commandEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_commandEdit->setTextCursor(cursor);
    }
}

void Page4::onIDNClicked()
{
    m_viewModel->sendIDN();
}

void Page4::onRSTClicked()
{
    m_viewModel->sendRST();
}

void Page4::onCLSClicked()
{
    m_viewModel->sendCLS();
}

void Page4::onOPCClicked()
{
    m_viewModel->sendOPC();
}

// ==================== ViewModel 事件 ====================

void Page4::onConnectionStatusChanged(ConnectionStatus status, const QString &message)
{
    switch (status) {
    case ConnectionStatus::Disconnected:
        m_statusLabel->setText(tr("● 未連線"));
        m_statusLabel->setStyleSheet("color: gray; font-weight: bold;");
        updateConnectionUI(false);
        break;

    case ConnectionStatus::Connecting:
        m_statusLabel->setText(tr("● 連線中..."));
        m_statusLabel->setStyleSheet("color: orange; font-weight: bold;");
        break;

    case ConnectionStatus::Connected:
        m_statusLabel->setText(tr("● 已連線"));
        m_statusLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
        updateConnectionUI(true);
        appendLog(tr("[資訊] %1").arg(message), "#00ff00");
        break;

    case ConnectionStatus::Error:
        m_statusLabel->setText(tr("● 連線錯誤"));
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
        updateConnectionUI(false);
        break;
    }
}

void Page4::onResponseReceived(const QString &command, const QString &response)
{
    Q_UNUSED(command)
    appendLog(tr("[RX] %1").arg(response), "#00bfff");
}

void Page4::onErrorOccurred(const QString &error)
{
    appendLog(tr("[錯誤] %1").arg(error), "red");
}

void Page4::onHistoryUpdated()
{
    updateAddressHistory();
    updateCommandHistory();
}

// ==================== UI 更新 ====================

void Page4::updateConnectionUI(bool connected)
{
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_addressCombo->setEnabled(!connected);

    m_sendBtn->setEnabled(connected);
    m_commandEdit->setEnabled(connected);
    m_commandHistory->setEnabled(connected);
    m_idnBtn->setEnabled(connected);
    m_rstBtn->setEnabled(connected);
    m_clsBtn->setEnabled(connected);
    m_opcBtn->setEnabled(connected);
}

void Page4::appendLog(const QString &text, const QString &color)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString html = QString("<span style='color: gray;'>[%1]</span> "
                           "<span style='color: %2;'>%3</span>")
                       .arg(timestamp, color, text.toHtmlEscaped());

    m_logView->append(html);

    // 自動滾動到底部
    QScrollBar *scrollBar = m_logView->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void Page4::updateAddressHistory()
{
    QString current = m_addressCombo->currentText();
    m_addressCombo->blockSignals(true);
    m_addressCombo->clear();

    QStringList history = m_viewModel->model()->addressHistory();
    m_addressCombo->addItems(history);

    // 如果之前有選擇且存在於列表中，恢復選擇；否則選擇第一個
    if (!current.isEmpty() && history.contains(current)) {
        m_addressCombo->setCurrentText(current);
    } else if (!history.isEmpty()) {
        m_addressCombo->setCurrentIndex(0);  // 選擇第一個
    }

    m_addressCombo->blockSignals(false);
}

void Page4::updateCommandHistory()
{
    m_commandHistory->blockSignals(true);
    m_commandHistory->clear();

    const QStringList history = m_viewModel->model()->commandHistory();
    m_commandHistory->addItems(history);

    // 預設顯示最近一筆（index 0）
    if (!history.isEmpty()) {
        m_commandHistory->setCurrentIndex(0);
    }

    m_commandHistory->blockSignals(false);
}
