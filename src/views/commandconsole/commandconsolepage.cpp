#include "commandconsolepage.h"
#include "chroma63600quickdialog.h"
#include "modbusrequesteditor.h"
#include "commandconsoleviewmodel.h"
#include "commandconsolemodel.h"
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
#include <QMenu>
#include <QSignalBlocker>
#include <memory>

CommandConsolePage::CommandConsolePage(CommandConsoleViewModel* viewModel, QWidget* parent) : QWidget(parent), m_viewModel(viewModel)
{
    setupUI();
    setupConnections();
    updateConnectionUI(false);
    updateAddressHistory();
    updateCommandHistory();
    updateModeUI();
}

void CommandConsolePage::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // 連線區域
    QGroupBox* connectionGroup = new QGroupBox(tr("連線設定"), this);
    QGridLayout* connLayout = new QGridLayout(connectionGroup);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->setObjectName("consoleMode");
    m_modeCombo->addItems({"SCPI", "Modbus RTU"});
    m_modeCombo->setToolTip(tr("切換通訊模式前，請先 Disconnect"));

    // 地址輸入
    QLabel* addrLabel = new QLabel(tr("Address:"), this);
    m_addressCombo = new QComboBox(this);
    m_addressCombo->setObjectName("consoleAddress");
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
    QLabel* timeoutLabel = new QLabel(tr("Timeout (ms):"), this);
    m_timeoutSpin = new QSpinBox(this);
    m_timeoutSpin->setRange(CommandConsoleModel::MIN_TIMEOUT, CommandConsoleModel::MAX_TIMEOUT);
    m_timeoutSpin->setValue(CommandConsoleModel::DEFAULT_TIMEOUT);
    m_timeoutSpin->setSingleStep(100);

    connLayout->addWidget(new QLabel(tr("Mode:"), this), 0, 0);
    connLayout->addWidget(m_modeCombo, 0, 1);
    connLayout->addWidget(addrLabel, 1, 0);
    connLayout->addWidget(m_addressCombo, 1, 1, 1, 3);
    connLayout->addWidget(m_connectBtn, 1, 4);
    connLayout->addWidget(m_disconnectBtn, 1, 5);
    connLayout->addWidget(m_statusLabel, 2, 1, 1, 2);
    connLayout->addWidget(timeoutLabel, 2, 3);
    connLayout->addWidget(m_timeoutSpin, 2, 4);

    // 指令區域
    QGroupBox* commandGroup = new QGroupBox(tr("指令輸入"), this);
    m_scpiGroup = commandGroup;
    QGridLayout* cmdLayout = new QGridLayout(commandGroup);

    // 多行指令輸入框（支援自動換行）
    QLabel* cmdLabel = new QLabel(tr("Command:"), this);
    m_commandEdit = new QPlainTextEdit(this);
    m_commandEdit->setPlaceholderText(tr("輸入 SCPI 指令，Ex: *IDN?多行指令以換行分隔，Ctrl+Enter 發送"));
    m_commandEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth); // 自動換行
    m_commandEdit->setMinimumHeight(96);
    m_commandEdit->setFont(QFont("Consolas", 10));
    m_commandEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_commandEdit, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        std::unique_ptr<QMenu> menu(m_commandEdit->createStandardContextMenu());
        menu->addSeparator();
        auto *clear = menu->addAction(tr("Clear"));
        clear->setEnabled(!m_commandEdit->isReadOnly() && !m_commandEdit->toPlainText().isEmpty());
        connect(clear, &QAction::triggered, m_commandEdit, &QPlainTextEdit::clear);
        menu->exec(m_commandEdit->viewport()->mapToGlobal(position));
    });

    // Ctrl+Enter 發送（Enter 本身用於換行）
    auto* sendShortcut = new QShortcut(QKeySequence("Ctrl+Return"), m_commandEdit);
    connect(sendShortcut, &QShortcut::activated, this, &CommandConsolePage::onSendClicked);

    // 歷史下拉選單（唯讀，選取後填入輸入框）
    QLabel* histLabel = new QLabel(tr("History:"), this);
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
    QHBoxLayout* quickBtnLayout = new QHBoxLayout;
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
    auto *chroma63600 = new QPushButton("63600…", this);
    chroma63600->setObjectName("chroma63600QuickCommands");
    chroma63600->setStyleSheet(quickBtnStyle);
    chroma63600->setToolTip(tr("填入 Channel、電流斜率與 Von 設定指令"));
    quickBtnLayout->addWidget(chroma63600);
    connect(chroma63600, &QPushButton::clicked, this, [this] {
        Chroma63600QuickDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const QString existing = m_commandEdit->toPlainText();
        m_commandEdit->setPlainText(existing +
            (existing.isEmpty() || existing.endsWith('\n') ? QString() : QString("\n")) + dialog.commands());
        m_commandEdit->moveCursor(QTextCursor::End);
    });
    quickBtnLayout->addStretch();

    cmdLayout->addWidget(cmdLabel, 0, 0, Qt::AlignTop);
    cmdLayout->addWidget(m_commandEdit, 0, 1);
    auto *commandButtons = new QVBoxLayout;
    commandButtons->addWidget(m_sendBtn);
    auto *clearCommand = new QPushButton(tr("Clear"), this);
    clearCommand->setObjectName("clearCommandButton");
    clearCommand->setFixedWidth(80);
    clearCommand->setToolTip(tr("清除目前輸入的指令"));
    connect(clearCommand, &QPushButton::clicked, m_commandEdit, &QPlainTextEdit::clear);
    commandButtons->addWidget(clearCommand);
    commandButtons->addStretch();
    cmdLayout->addLayout(commandButtons, 0, 2);
    cmdLayout->addWidget(histLabel, 1, 0);
    cmdLayout->addWidget(m_commandHistory, 1, 1, 1, 2);
    cmdLayout->addLayout(quickBtnLayout, 2, 0, 1, 3);

    auto *modbusGroup = new QGroupBox(tr("Modbus RTU 讀寫"), this);
    m_modbusGroup = modbusGroup;
    auto *modbusLayout = new QVBoxLayout(modbusGroup);
    m_modbusEditor = new ModbusRequestEditor(modbusGroup);
    modbusLayout->addWidget(m_modbusEditor);
    auto *modbusShortcut = new QShortcut(QKeySequence("Ctrl+Return"), m_modbusEditor);
    modbusShortcut->setObjectName("modbusSendShortcut");
    modbusShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(modbusShortcut, &QShortcut::activated, this, &CommandConsolePage::onSendClicked);

    // 日誌區域
    QGroupBox* logGroup = new QGroupBox(tr("通訊記錄"), this);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);

    m_logView = new QTextEdit(this);
    m_logView->setObjectName("consoleLog");
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

    QHBoxLayout* logBtnLayout = new QHBoxLayout;
    logBtnLayout->addStretch();
    logBtnLayout->addWidget(m_clearBtn);

    logLayout->addWidget(m_logView);
    logLayout->addLayout(logBtnLayout);

    // 主佈局
    mainLayout->addWidget(connectionGroup);
    mainLayout->addWidget(commandGroup);
    mainLayout->addWidget(modbusGroup);
    mainLayout->addWidget(logGroup, 1); // 讓日誌區域擴展

    setLayout(mainLayout);
}

void CommandConsolePage::setupConnections()
{
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_viewModel->setMode(index == 1 ? ConsoleMode::ModbusRtu : ConsoleMode::Scpi);
        updateModeUI();
    });
    connect(m_viewModel, &CommandConsoleViewModel::modeChanged, this, &CommandConsolePage::updateModeUI);
    connect(m_viewModel, &CommandConsoleViewModel::modbusSlaveSelected, this,
            [this](int slave) { m_modbusEditor->setSlave(slave); });
    connect(m_modbusEditor->sendButton(), &QPushButton::clicked, this, &CommandConsolePage::onSendClicked);
    connect(m_viewModel, &CommandConsoleViewModel::modbusTransmitting, this, [this](const QByteArray &tx) {
        appendLog("[TX Hex] " + QString::fromLatin1(tx.toHex(' ').toUpper()), "#00ff00");
    });
    connect(m_viewModel, &CommandConsoleViewModel::modbusTraffic, this, [this](const QByteArray &, const QByteArray &rx) {
        appendLog("[RX Hex] " + (rx.isEmpty() ? tr("無資料") : QString::fromLatin1(rx.toHex(' ').toUpper())), "#00bfff");
    });
    // 按鈕連接
    connect(m_connectBtn, &QPushButton::clicked, this, &CommandConsolePage::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &CommandConsolePage::onDisconnectClicked);
    connect(m_sendBtn, &QPushButton::clicked, this, &CommandConsolePage::onSendClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &CommandConsolePage::onClearClicked);

    // 快速指令按鈕
    connect(m_idnBtn, &QPushButton::clicked, this, &CommandConsolePage::onIDNClicked);
    connect(m_rstBtn, &QPushButton::clicked, this, &CommandConsolePage::onRSTClicked);
    connect(m_clsBtn, &QPushButton::clicked, this, &CommandConsolePage::onCLSClicked);
    connect(m_opcBtn, &QPushButton::clicked, this, &CommandConsolePage::onOPCClicked);

    // Timeout 設定
    connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_viewModel,
            &CommandConsoleViewModel::setTimeout);

    // 歷史選單選取後填入輸入框
    connect(m_commandHistory, &QComboBox::currentTextChanged, this, &CommandConsolePage::onCommandHistorySelected);

    // ViewModel 信號
    connect(m_viewModel, &CommandConsoleViewModel::controlActiveChanged, this,
            [this] { updateConnectionUI(m_viewModel->isConnected()); });
    connect(m_viewModel, &CommandConsoleViewModel::connectionStatusChanged, this, &CommandConsolePage::onConnectionStatusChanged);
    connect(m_viewModel, &CommandConsoleViewModel::responseReceived, this, &CommandConsolePage::onResponseReceived);
    connect(m_viewModel, &CommandConsoleViewModel::errorOccurred, this, &CommandConsolePage::onErrorOccurred);
    connect(m_viewModel, &CommandConsoleViewModel::historyUpdated, this, &CommandConsolePage::onHistoryUpdated);
    connect(m_viewModel, &CommandConsoleViewModel::commandSent, this,
            [this](const QString& cmd) { appendLog(tr("[TX] %1").arg(cmd), "#00ff00"); });
}

// 按鈕事件

void CommandConsolePage::onConnectClicked()
{
    QString address = m_addressCombo->currentText().trimmed();
    if (address.isEmpty()) {
        appendLog(tr("[錯誤] 請輸入有效的地址"), "red");
        return;
    }

    appendLog(tr("[資訊] 正在連線到 %1...").arg(address), "yellow");
    m_viewModel->connectToAddress(address);
}

void CommandConsolePage::onDisconnectClicked()
{
    m_viewModel->disconnect();
    appendLog(tr("[資訊] 已斷開連線"), "yellow");
}

void CommandConsolePage::onSendClicked()
{
    if (!m_viewModel->isConnected() || m_viewModel->isOperationPending())
        return;

    if (m_viewModel->mode() == ConsoleMode::ModbusRtu) {
        m_viewModel->sendModbusRequest(m_modbusEditor->slave(), m_modbusEditor->function(),
                                     m_modbusEditor->address(), m_modbusEditor->count(), m_modbusEditor->values());
        return;
    }

    // toPlainText() 保留換行；trimmed() 去掉首尾空白
    QString command = m_commandEdit->toPlainText().trimmed();
    if (command.isEmpty()) {
        return;
    }

    m_viewModel->sendCommand(command);

    // 清空輸入框
    m_commandEdit->clear();
}

void CommandConsolePage::onClearClicked()
{
    m_logView->clear();
}

void CommandConsolePage::onCommandHistorySelected(const QString& command)
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

void CommandConsolePage::onIDNClicked()
{
    m_viewModel->sendIDN();
}

void CommandConsolePage::onRSTClicked()
{
    m_viewModel->sendRST();
}

void CommandConsolePage::onCLSClicked()
{
    m_viewModel->sendCLS();
}

void CommandConsolePage::onOPCClicked()
{
    m_viewModel->sendOPC();
}

// ViewModel 事件

void CommandConsolePage::onConnectionStatusChanged(ConnectionStatus status, const QString& message)
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

void CommandConsolePage::onResponseReceived(const QString& command, const QString& response)
{
    Q_UNUSED(command)
    const bool modbus = m_viewModel->mode() == ConsoleMode::ModbusRtu;
    appendLog((modbus ? tr("[Parsed]\n%1") : tr("[RX] %1")).arg(response), "#00bfff", modbus);
}

void CommandConsolePage::onErrorOccurred(const QString& error)
{
    appendLog(tr("[錯誤] %1").arg(error), "red");
}

void CommandConsolePage::onHistoryUpdated()
{
    updateAddressHistory();
    updateCommandHistory();
}

// UI 更新

void CommandConsolePage::updateConnectionUI(bool connected)
{
    const bool pending = m_viewModel && m_viewModel->isOperationPending();
    const bool active = m_viewModel && m_viewModel->isControlActive();
    m_connectBtn->setEnabled(!connected && !pending);
    m_disconnectBtn->setEnabled(active);
    m_addressCombo->setEnabled(!active);
    const bool canSend = connected && !pending;
    m_modeCombo->setEnabled(!active && !pending);
    m_modbusEditor->setEnabled(!pending);
    m_modbusEditor->sendButton()->setEnabled(canSend && m_viewModel->mode() == ConsoleMode::ModbusRtu);

    m_sendBtn->setEnabled(canSend);
    m_commandEdit->setEnabled(true);
    m_commandHistory->setEnabled(true);
    m_idnBtn->setEnabled(canSend);
    m_rstBtn->setEnabled(canSend);
    m_clsBtn->setEnabled(canSend);
    m_opcBtn->setEnabled(canSend);
}

void CommandConsolePage::updateModeUI()
{
    const bool modbus = m_viewModel->mode() == ConsoleMode::ModbusRtu;
    const QSignalBlocker blocker(m_modeCombo);
    m_modeCombo->setCurrentIndex(modbus ? 1 : 0);
    m_scpiGroup->setVisible(!modbus);
    m_modbusGroup->setVisible(modbus);
    m_addressCombo->lineEdit()->setPlaceholderText(modbus
        ? tr("例: MODBUS::COM3:9600:8:N:1::SLAVE:1")
        : tr("例: GPIB0::30::INSTR 或 TCPIP::192.168.1.100::5025::SOCKET"));
    m_addressCombo->setToolTip(modbus
        ? tr("可從下拉選單選取範例，再依設備修改：\n"
             "MODBUS::COM3:9600:8:N:1::SLAVE:1\n"
             "COM3 = 串口；9600 = 鮑率；8 = 資料位元\n"
             "N = 無校驗（E = 偶校驗、O = 奇校驗）；1 = 停止位元\n"
             "SLAVE:1 = 站號 1（可用 1–247）")
        : QString());
    updateAddressHistory();
    updateConnectionUI(m_viewModel->isConnected());
}

void CommandConsolePage::appendLog(const QString& text, const QString& color, bool multiline)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString escaped = text.toHtmlEscaped();
    if (multiline) escaped.replace('\n', "<br>");
    QString html = QString("<span style='color: gray;'>[%1]</span> "
                           "<span style='color: %2;'>%3</span>")
                       .arg(timestamp, color, escaped);

    m_logView->append(html);

    // 自動滾動到底部
    QScrollBar* scrollBar = m_logView->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void CommandConsolePage::updateAddressHistory()
{
    QString current = m_addressCombo->currentText();
    m_addressCombo->blockSignals(true);
    m_addressCombo->clear();

    QStringList addresses = m_viewModel->model()->addressHistory();
    if (m_viewModel->mode() == ConsoleMode::ModbusRtu) {
        // 範例僅供選取；實際連線成功後才由 Model 記入歷史。
        const QString example = QStringLiteral("MODBUS::COM3:9600:8:N:1::SLAVE:1");
        addresses.removeAll(example);
        addresses.prepend(example);
    }
    m_addressCombo->addItems(addresses);

    // 保留尚未連線的手動輸入；空白時才帶入預設項目。
    if (!current.trimmed().isEmpty()) {
        m_addressCombo->setCurrentText(current);
    } else if (!addresses.isEmpty()) {
        m_addressCombo->setCurrentIndex(0); // 選擇第一個
    }

    m_addressCombo->blockSignals(false);
}

void CommandConsolePage::updateCommandHistory()
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
