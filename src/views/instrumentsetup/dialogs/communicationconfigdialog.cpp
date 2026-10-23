#include "communicationconfigdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QSignalBlocker>

namespace {
QString selectedPort(const QComboBox* combo)
{
    // Editable text can differ from the item still stored at currentIndex().
    return combo->currentText().section(" - ", 0, 0).trimmed();
}
void selectStoredValue(QComboBox* combo, const QString& value)
{
    int index = combo->findData(value);
    if (index < 0) {
        // Preserve unknown XML values as repairable drafts, not a valid default.
        combo->addItem(value, value);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index);
}
}

CommunicationConfigDialog::CommunicationConfigDialog(QWidget* parent)
    : CommunicationConfigDialog(makeConnectionTestService(), parent) {}

CommunicationConfigDialog::CommunicationConfigDialog(std::unique_ptr<ConnectionTestService> test, QWidget* parent)
    : QDialog(parent), m_connectionTest(std::move(test))
{
    setWindowTitle(tr("通訊設定 / Communication Settings"));
    setMinimumWidth(450);
    setMinimumHeight(350);
    setupUI();
    connect(m_connectionTest.get(), &ConnectionTestService::completed, this,
            [this](const ConnectionTestResult &result) {
        m_btnTest->setEnabled(true);
        // A response belongs to the address captured when Test was pressed.
        if (result.resource != getConfig().toResourceString() || result.timeoutMs != getConfig().timeout) {
            m_lblStatus->clear();
            return;
        }
        if (!result.error.isEmpty() || !result.opened) {
            m_lblStatus->setText(tr("❌ 連線失敗\n%1").arg(result.error.isEmpty() ? result.resource : result.error));
            m_lblStatus->setStyleSheet("color: red;");
        } else if (result.transportOnly) {
            m_lblStatus->setText(tr("串口已開啟，未驗證設備回應"));
            m_lblStatus->setStyleSheet("color: #CC7700;");
        } else if (!result.identity.isEmpty()) {
            m_lblStatus->setText(tr("✓ 連線成功\n%1").arg(result.identity.left(80)));
            m_lblStatus->setStyleSheet("color: green;");
        } else {
            m_lblStatus->setText(tr("⚠ 連線成功但 *IDN? 無回應\n(部分儀器不支援此指令)"));
            m_lblStatus->setStyleSheet("color: #CC7700;");
        }
    });
}

void CommunicationConfigDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 協議選擇區域
    auto* protocolGroup = new QGroupBox(tr("協議類型 / Protocol Type"));
    auto* protocolLayout = new QHBoxLayout(protocolGroup);

    m_cmbProtocol = new QComboBox;
    m_cmbProtocol->setObjectName("communicationProtocol");
    m_cmbProtocol->addItem("GPIB (IEEE-488)", static_cast<int>(ProtocolType::GPIB));
    m_cmbProtocol->addItem("TCP/IP (VXI-11)", static_cast<int>(ProtocolType::TCP_VXI11));
    m_cmbProtocol->addItem("TCP/IP (Socket)", static_cast<int>(ProtocolType::TCP_Socket));
    m_cmbProtocol->addItem("Serial (RS-232/RS-485)", static_cast<int>(ProtocolType::Serial));
    m_cmbProtocol->addItem("Modbus RTU", static_cast<int>(ProtocolType::Modbus_RTU));
    // m_cmbProtocol->addItem("Modbus TCP",                static_cast<int>(ProtocolType::Modbus_TCP));
    // m_cmbProtocol->addItem("USB (USBTMC)",              static_cast<int>(ProtocolType::USB));

    protocolLayout->addWidget(m_cmbProtocol);
    mainLayout->addWidget(protocolGroup);

    // 參數配置區域（堆疊頁面）
    m_stackedWidget = new QStackedWidget;

    createGpibPage();      // Index 0
    createTcpPage();       // Index 1
    createSerialPage();    // Index 2
    createModbusTcpPage(); // Index 3
    createUsbPage();       // Index 4

    // 建立協議到頁面的映射
    m_protocolPageMap[ProtocolType::GPIB] = 0;
    m_protocolPageMap[ProtocolType::TCP_VXI11] = 1;
    m_protocolPageMap[ProtocolType::TCP_Socket] = 1;
    m_protocolPageMap[ProtocolType::TCP_HiSLIP] = 1;
    m_protocolPageMap[ProtocolType::Serial] = 2;
    m_protocolPageMap[ProtocolType::Modbus_RTU] = 2;
    m_protocolPageMap[ProtocolType::Modbus_TCP] = 3;
    m_protocolPageMap[ProtocolType::USB] = 4;

    mainLayout->addWidget(m_stackedWidget, 1);

    // 測試連線區域
    auto* testGroup = new QGroupBox(tr("連線測試 / Connection Test"));
    auto* testLayout = new QHBoxLayout(testGroup);

    m_btnTest = new QPushButton(tr("測試連線"));
    m_btnTest->setFixedWidth(100);
    m_lblStatus = new QLabel;
    m_lblStatus->setObjectName("connectionTestStatus");
    m_lblStatus->setWordWrap(true);

    testLayout->addWidget(m_btnTest);
    testLayout->addWidget(m_lblStatus, 1);
    mainLayout->addWidget(testGroup);

    // 按鈕區域
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(m_buttonBox);

    // 連接信號
    connect(m_cmbProtocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CommunicationConfigDialog::onProtocolChanged);
    connect(m_btnTest, &QPushButton::clicked, this, &CommunicationConfigDialog::onTestConnection);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this] {
        if (!getConfig().isValid()) {
            m_lblStatus->setText(tr("配置無效，請確認完整串口名稱（例如 COM4）及通訊參數。"));
            m_lblStatus->setStyleSheet("color: red;");
            return;
        }
        accept();
    });
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 初始顯示第一個頁面
    m_stackedWidget->setCurrentIndex(0);
}

void CommunicationConfigDialog::createGpibPage()
{
    m_pageGpib = new QWidget;
    auto* layout = new QFormLayout(m_pageGpib);
    layout->setSpacing(10);

    m_spnGpibBoard = new QSpinBox;
    m_spnGpibBoard->setRange(0, 15);
    m_spnGpibBoard->setValue(0);
    m_spnGpibBoard->setToolTip(tr("GPIB 介面卡編號 (通常為 0)"));
    layout->addRow(tr("Board 編號："), m_spnGpibBoard);

    m_spnGpibAddress = new QSpinBox;
    m_spnGpibAddress->setRange(0, 30);
    m_spnGpibAddress->setValue(1);
    m_spnGpibAddress->setToolTip(tr("儀器的 GPIB 主地址 (0-30)"));
    layout->addRow(tr("主地址 (Primary)："), m_spnGpibAddress);

    m_spnGpibSecondary = new QSpinBox;
    m_spnGpibSecondary->setRange(0, 31);
    m_spnGpibSecondary->setValue(0);
    m_spnGpibSecondary->setToolTip(tr("次地址 (0 表示不使用)"));
    layout->addRow(tr("次地址 (Secondary)："), m_spnGpibSecondary);

    // 預覽標籤
    auto* lblPreview = new QLabel;
    lblPreview->setStyleSheet("color: gray; font-style: italic;");
    layout->addRow(tr("資源字串："), lblPreview);

    // 動態更新預覽
    auto updatePreview = [this, lblPreview]() {
        CommunicationConfig cfg;
        cfg.protocol = ProtocolType::GPIB;
        cfg.gpibBoard = m_spnGpibBoard->value();
        cfg.gpibAddress = m_spnGpibAddress->value();
        cfg.gpibSecondary = m_spnGpibSecondary->value();
        lblPreview->setText(cfg.toResourceString());
    };

    connect(m_spnGpibBoard, QOverload<int>::of(&QSpinBox::valueChanged), updatePreview);
    connect(m_spnGpibAddress, QOverload<int>::of(&QSpinBox::valueChanged), updatePreview);
    connect(m_spnGpibSecondary, QOverload<int>::of(&QSpinBox::valueChanged), updatePreview);
    updatePreview();

    m_stackedWidget->addWidget(m_pageGpib);
}

void CommunicationConfigDialog::createTcpPage()
{
    m_pageTcp = new QWidget;
    auto* layout = new QFormLayout(m_pageTcp);
    m_tcpForm = layout;
    layout->setSpacing(10);

    m_edtIpAddress = new QLineEdit;
    m_edtIpAddress->setPlaceholderText("192.168.1.100");
    m_edtIpAddress->setToolTip(tr("儀器的 IP 位址"));
    layout->addRow(tr("IP 位址："), m_edtIpAddress);

    m_spnPort = new QSpinBox;
    m_spnPort->setObjectName("tcpPort");
    m_spnPort->setRange(1, 65535);
    m_spnPort->setValue(5025);
    m_spnPort->setToolTip(tr("儀器的 TCP Socket 連接埠"));
    layout->addRow(tr("連接埠 (Port)："), m_spnPort);

    m_lblTcpHint = new QLabel;
    m_lblTcpHint->setObjectName("tcpHint");
    m_lblTcpHint->setStyleSheet("color: gray; font-size: 10px;");
    layout->addRow("", m_lblTcpHint);

    // 預覽標籤
    auto* lblPreview = new QLabel;
    lblPreview->setObjectName("tcpResourcePreview");
    lblPreview->setStyleSheet("color: gray; font-style: italic;");
    layout->addRow(tr("資源字串："), lblPreview);

    auto updatePreview = [this, lblPreview]() {
        CommunicationConfig cfg;
        cfg.protocol = static_cast<ProtocolType>(m_cmbProtocol->currentData().toInt());
        cfg.ipAddress = m_edtIpAddress->text();
        cfg.port = m_spnPort->value();
        lblPreview->setText(cfg.toResourceString());
        const bool socket = cfg.protocol == ProtocolType::TCP_Socket;
        m_tcpForm->setRowVisible(m_spnPort, socket);
        m_lblTcpHint->setText(socket
            ? tr("Socket 連接埠依儀器設定；Delta AC Source=8462, Chroma=3300")
            : cfg.protocol == ProtocolType::TCP_HiSLIP
                ? tr("HiSLIP：TCPIP0::儀器IP::hislip0::INSTR")
                : tr("VXI-11：TCPIP0::儀器IP::INSTR（連接埠由 VISA 自動處理）"));
    };

    connect(m_edtIpAddress, &QLineEdit::textChanged, updatePreview);
    connect(m_spnPort, QOverload<int>::of(&QSpinBox::valueChanged), updatePreview);
    connect(m_cmbProtocol, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    updatePreview();

    m_stackedWidget->addWidget(m_pageTcp);
}

void CommunicationConfigDialog::createSerialPage()
{
    m_pageSerial = new QWidget;
    auto* layout = new QFormLayout(m_pageSerial);
    m_serialForm = layout;
    layout->setSpacing(8);

    // 串口選擇 + 刷新按鈕
    auto* portLayout = new QHBoxLayout;
    m_cmbPortName = new QComboBox;
    m_cmbPortName->setObjectName("serialPort");
    m_cmbPortName->setEditable(true);
    m_cmbPortName->setMinimumWidth(200);
    populateSerialPorts(m_cmbPortName);

    m_btnRefreshPorts = new QPushButton(tr("🔄"));
    m_btnRefreshPorts->setFixedWidth(30);
    m_btnRefreshPorts->setToolTip(tr("重新掃描串口"));
    connect(m_btnRefreshPorts, &QPushButton::clicked, this, &CommunicationConfigDialog::onRefreshSerialPorts);

    portLayout->addWidget(m_cmbPortName, 1);
    portLayout->addWidget(m_btnRefreshPorts);
    layout->addRow(tr("串口："), portLayout);

    // 鮑率
    m_cmbBaudRate = new QComboBox;
    m_cmbBaudRate->setEditable(true);
    m_cmbBaudRate->addItems(
        {"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    m_cmbBaudRate->setCurrentText("9600");
    layout->addRow(tr("鮑率 (Baud)："), m_cmbBaudRate);

    // 資料位元
    m_cmbDataBits = new QComboBox;
    m_cmbDataBits->setObjectName("serialDataBits");
    m_cmbDataBits->addItems({"5", "6", "7", "8"});
    m_cmbDataBits->setCurrentText("8");
    layout->addRow(tr("資料位元："), m_cmbDataBits);

    // 校驗位
    m_cmbParity = new QComboBox;
    m_cmbParity->addItem(tr("無 (None)"), "N");
    m_cmbParity->addItem(tr("偶校驗 (Even)"), "E");
    m_cmbParity->addItem(tr("奇校驗 (Odd)"), "O");
    m_cmbParity->addItem(tr("Space"), "S");
    m_cmbParity->addItem(tr("Mark"), "M");
    layout->addRow(tr("校驗位："), m_cmbParity);

    // 停止位
    m_cmbStopBits = new QComboBox;
    m_cmbStopBits->addItem("1", "1");
    m_cmbStopBits->addItem("1.5", "1.5");
    m_cmbStopBits->addItem("2", "2");
    layout->addRow(tr("停止位："), m_cmbStopBits);

    // 流量控制
    m_cmbFlowControl = new QComboBox;
    m_cmbFlowControl->setObjectName("serialFlowControl");
    m_cmbFlowControl->addItem(tr("無 (None)"), "N");
    m_cmbFlowControl->addItem(tr("硬體 (RTS/CTS)"), "HW");
    m_cmbFlowControl->addItem(tr("軟體 (XON/XOFF)"), "SW");
    layout->addRow(tr("流量控制："), m_cmbFlowControl);

    m_spnModbusRtuSlaveId = new QSpinBox;
    m_spnModbusRtuSlaveId->setObjectName("modbusSlaveId");
    m_spnModbusRtuSlaveId->setRange(1, 247);
    m_spnModbusRtuSlaveId->setToolTip(tr("Modbus 從站地址 (1–247)"));
    layout->addRow(tr("Slave ID："), m_spnModbusRtuSlaveId);
    layout->setRowVisible(m_spnModbusRtuSlaveId, false);
    connect(m_spnModbusRtuSlaveId, &QSpinBox::valueChanged, this, [this](int value) {
        if (value >= 1 && value <= 247)
            m_spnModbusRtuSlaveId->setRange(1, 247);
    });

    // 預覽
    auto* lblPreview = new QLabel;
    m_lblSerialPreview = lblPreview;
    lblPreview->setStyleSheet("color: gray; font-style: italic;");
    layout->addRow(tr("資源字串："), lblPreview);

    auto updatePreview = [this, lblPreview]() {
        CommunicationConfig cfg;
        cfg.protocol = static_cast<ProtocolType>(m_cmbProtocol->currentData().toInt());
        cfg.portName = selectedPort(m_cmbPortName);
        cfg.baudRate = m_cmbBaudRate->currentText().toInt();
        cfg.dataBits = m_cmbDataBits->currentText().toInt();
        cfg.parity = m_cmbParity->currentData().toString();
        cfg.stopBits = m_cmbStopBits->currentData().toString();
        cfg.flowControl = cfg.protocol == ProtocolType::Modbus_RTU
                              ? "N" : m_cmbFlowControl->currentData().toString();
        cfg.slaveId = m_spnModbusRtuSlaveId->value();
        lblPreview->setText(cfg.toResourceString());
    };

    connect(m_cmbPortName, &QComboBox::currentTextChanged, updatePreview);
    connect(m_cmbBaudRate, &QComboBox::currentTextChanged, updatePreview);
    connect(m_cmbDataBits, &QComboBox::currentTextChanged, updatePreview);
    connect(m_cmbParity, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    connect(m_cmbStopBits, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    connect(m_cmbFlowControl, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    connect(m_spnModbusRtuSlaveId, &QSpinBox::valueChanged, updatePreview);
    connect(m_cmbProtocol, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    updatePreview();

    m_stackedWidget->addWidget(m_pageSerial);
}

void CommunicationConfigDialog::createModbusTcpPage()
{
    m_pageModbusTcp = new QWidget;
    auto* layout = new QFormLayout(m_pageModbusTcp);
    layout->setSpacing(10);

    // Slave ID
    m_spnModbusTcpSlaveId = new QSpinBox;
    m_spnModbusTcpSlaveId->setRange(1, 255);
    m_spnModbusTcpSlaveId->setValue(1);
    m_spnModbusTcpSlaveId->setToolTip(tr("Modbus Unit ID / Slave ID"));
    layout->addRow(tr("Slave ID："), m_spnModbusTcpSlaveId);

    // IP 位址
    m_edtModbusTcpIp = new QLineEdit;
    m_edtModbusTcpIp->setPlaceholderText("192.168.1.100");
    layout->addRow(tr("IP 位址："), m_edtModbusTcpIp);

    // 連接埠
    m_spnModbusTcpPort = new QSpinBox;
    m_spnModbusTcpPort->setRange(1, 65535);
    m_spnModbusTcpPort->setValue(502); // Modbus TCP 標準埠
    m_spnModbusTcpPort->setToolTip(tr("Modbus TCP 標準埠為 502"));
    layout->addRow(tr("連接埠："), m_spnModbusTcpPort);

    m_stackedWidget->addWidget(m_pageModbusTcp);
}

void CommunicationConfigDialog::createUsbPage()
{
    m_pageUsb = new QWidget;
    auto* layout = new QFormLayout(m_pageUsb);
    layout->setSpacing(10);

    m_edtVendorId = new QLineEdit;
    m_edtVendorId->setPlaceholderText("0699 (不含 0x 前綴)");
    m_edtVendorId->setToolTip(tr("製造商 ID，例如 Tektronix=0699, Keysight=0957"));
    layout->addRow(tr("Vendor ID："), m_edtVendorId);

    m_edtProductId = new QLineEdit;
    m_edtProductId->setPlaceholderText("0401");
    m_edtProductId->setToolTip(tr("產品 ID"));
    layout->addRow(tr("Product ID："), m_edtProductId);

    m_edtSerialNumber = new QLineEdit;
    m_edtSerialNumber->setPlaceholderText("C012345");
    m_edtSerialNumber->setToolTip(tr("儀器序號"));
    layout->addRow(tr("序號："), m_edtSerialNumber);

    // 常用 VID 說明
    auto* lblHint = new QLabel(tr("常用 Vendor ID：\n"
                                  "Tektronix: 0699\n"
                                  "Keysight/Agilent: 0957\n"
                                  "Rohde & Schwarz: 0AAD\n"
                                  "National Instruments: 3923"));
    lblHint->setStyleSheet("color: gray; font-size: 10px;");
    layout->addRow("", lblHint);

    m_stackedWidget->addWidget(m_pageUsb);
}

void CommunicationConfigDialog::populateSerialPorts(QComboBox* comboBox)
{
    const QString port = selectedPort(comboBox);
    comboBox->clear();

    for (const auto& info : QSerialPortInfo::availablePorts()) {
        QString displayText = QString("%1 - %2").arg(info.portName(), info.description());
        comboBox->addItem(displayText, info.portName());
    }

    // 嘗試恢復之前選擇
    if (!port.isEmpty()) {
        int index = comboBox->findData(port);
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
        } else {
            comboBox->setCurrentIndex(-1);
            comboBox->setEditText(port);
        }
    }
}

void CommunicationConfigDialog::onProtocolChanged(int index)
{
    auto protocol = static_cast<ProtocolType>(m_cmbProtocol->itemData(index).toInt());
    if (m_currentMainProtocol == ProtocolType::Serial)
        m_serialDataBits = m_cmbDataBits->currentText().toInt();
    if (protocol == ProtocolType::Serial || protocol == ProtocolType::Modbus_RTU) {
        const bool rtu = protocol == ProtocolType::Modbus_RTU;
        m_cmbDataBits->clear();
        m_cmbDataBits->addItems(rtu ? QStringList{"8"} : QStringList{"5", "6", "7", "8"});
        m_cmbDataBits->setCurrentText(QString::number(rtu ? 8 : m_serialDataBits));
        m_serialForm->setRowVisible(m_cmbFlowControl, !rtu);
        m_serialForm->setRowVisible(m_spnModbusRtuSlaveId, rtu);
    }
    int pageIndex = m_protocolPageMap.value(protocol, 0);
    m_stackedWidget->setCurrentIndex(pageIndex);
    m_currentMainProtocol = protocol;

    // 清除測試狀態
    m_lblStatus->clear();
}

void CommunicationConfigDialog::onRefreshSerialPorts()
{
    populateSerialPorts(m_cmbPortName);
}

void CommunicationConfigDialog::onTestConnection()
{
    const auto config = getConfig();
    const QString resource = config.toResourceString();
    if (!config.isValid()) {
        m_lblStatus->setText(tr("❌ 配置不完整"));
        m_lblStatus->setStyleSheet("color: red;");
        return;
    }
    if (!m_connectionTest->start(resource, config.timeout))
        return;
    m_lblStatus->setText(tr("測試中..."));
    m_lblStatus->setStyleSheet("color: orange;");
    m_btnTest->setEnabled(false);
}

void CommunicationConfigDialog::setConfig(const CommunicationConfig& config)
{
    m_timeout = config.timeout;
    // Keep an existing HiSLIP resource editable without silently changing its protocol.
    if (config.protocol == ProtocolType::TCP_HiSLIP &&
        m_cmbProtocol->findData(static_cast<int>(config.protocol)) < 0)
        m_cmbProtocol->addItem("TCP/IP (HiSLIP)", static_cast<int>(config.protocol));
    // 設定協議類型下拉選單
    for (int i = 0; i < m_cmbProtocol->count(); ++i) {
        if (m_cmbProtocol->itemData(i).toInt() == static_cast<int>(config.protocol)) {
            m_cmbProtocol->setCurrentIndex(i);
            break;
        }
    }

    // 根據協議類型設定各頁面
    switch (config.protocol) {
    case ProtocolType::GPIB:
        m_spnGpibBoard->setValue(config.gpibBoard);
        m_spnGpibAddress->setValue(config.gpibAddress);
        m_spnGpibSecondary->setValue(config.gpibSecondary);
        break;

    case ProtocolType::TCP_VXI11:
    case ProtocolType::TCP_Socket:
    case ProtocolType::TCP_HiSLIP:
        m_edtIpAddress->setText(config.ipAddress);
        m_spnPort->setValue(config.port);
        break;

    case ProtocolType::Serial:
    case ProtocolType::Modbus_RTU:
        m_cmbPortName->setEditText(config.portName);
        m_cmbBaudRate->setCurrentText(QString::number(config.baudRate));
        // Display invalid legacy values faithfully so OK/Connection Test can reject them.
        m_cmbDataBits->clear();
        m_cmbDataBits->addItems(config.protocol == ProtocolType::Modbus_RTU
                                   ? QStringList{"8"} : QStringList{"5", "6", "7", "8"});
        if (m_cmbDataBits->findText(QString::number(config.dataBits)) < 0)
            m_cmbDataBits->addItem(QString::number(config.dataBits));
        m_cmbDataBits->setCurrentText(QString::number(config.dataBits));
        if (config.protocol == ProtocolType::Serial)
            m_serialDataBits = config.dataBits;
        if (config.protocol == ProtocolType::Modbus_RTU) {
            const QSignalBlocker block(m_spnModbusRtuSlaveId);
            m_spnModbusRtuSlaveId->setRange(qMin(1, config.slaveId), qMax(247, config.slaveId));
            m_spnModbusRtuSlaveId->setValue(config.slaveId);
        }
        selectStoredValue(m_cmbParity, config.parity);
        selectStoredValue(m_cmbStopBits, config.stopBits);
        if (config.protocol == ProtocolType::Serial)
            selectStoredValue(m_cmbFlowControl, config.flowControl);
        break;

    case ProtocolType::Modbus_TCP:
        m_spnModbusTcpSlaveId->setValue(config.slaveId);
        m_edtModbusTcpIp->setText(config.ipAddress);
        m_spnModbusTcpPort->setValue(config.port);
        break;

    case ProtocolType::USB:
        m_edtVendorId->setText(config.vendorId);
        m_edtProductId->setText(config.productId);
        m_edtSerialNumber->setText(config.serialNumber);
        break;

    default:
        break;
    }
    m_lblSerialPreview->setText(getConfig().toResourceString());
}

CommunicationConfig CommunicationConfigDialog::getConfig() const
{
    CommunicationConfig config;
    config.timeout = m_timeout;

    config.protocol = static_cast<ProtocolType>(m_cmbProtocol->currentData().toInt());

    // 根據協議類型收集參數
    switch (config.protocol) {
    case ProtocolType::GPIB:
        config.gpibBoard = m_spnGpibBoard->value();
        config.gpibAddress = m_spnGpibAddress->value();
        config.gpibSecondary = m_spnGpibSecondary->value();
        break;

    case ProtocolType::TCP_VXI11:
    case ProtocolType::TCP_Socket:
    case ProtocolType::TCP_HiSLIP:
        config.ipAddress = m_edtIpAddress->text().trimmed();
        config.port = m_spnPort->value();
        break;

    case ProtocolType::Serial:
    case ProtocolType::Modbus_RTU:
        config.portName = selectedPort(m_cmbPortName);
        config.baudRate = m_cmbBaudRate->currentText().toInt();
        config.dataBits = m_cmbDataBits->currentText().toInt();
        config.parity = m_cmbParity->currentData().toString();
        config.stopBits = m_cmbStopBits->currentData().toString();
        config.flowControl = config.protocol == ProtocolType::Modbus_RTU
                                 ? "N" : m_cmbFlowControl->currentData().toString();
        config.slaveId = m_spnModbusRtuSlaveId->value();
        break;

    case ProtocolType::Modbus_TCP:
        config.slaveId = m_spnModbusTcpSlaveId->value();
        config.ipAddress = m_edtModbusTcpIp->text().trimmed();
        config.port = m_spnModbusTcpPort->value();
        break;

    case ProtocolType::USB:
        config.vendorId = m_edtVendorId->text().trimmed();
        config.productId = m_edtProductId->text().trimmed();
        config.serialNumber = m_edtSerialNumber->text().trimmed();
        break;

    default:
        break;
    }

    return config;
}

void CommunicationConfigDialog::setAvailableProtocols(const QList<ProtocolType>& protocols)
{
    // 記住當前選擇
    int currentData = m_cmbProtocol->currentData().toInt();

    m_cmbProtocol->clear();

    for (ProtocolType p : protocols) {
        QString name;
        switch (p) {
        case ProtocolType::GPIB:
            name = "GPIB (IEEE-488)";
            break;
        case ProtocolType::TCP_VXI11:
            name = "TCP/IP (VXI-11)";
            break;
        case ProtocolType::TCP_Socket:
            name = "TCP/IP (Socket)";
            break;
        case ProtocolType::TCP_HiSLIP:
            name = "TCP/IP (HiSLIP)";
            break;
        case ProtocolType::Serial:
            name = "Serial (RS-232/RS-485)";
            break;
        case ProtocolType::Modbus_RTU:
            name = "Modbus RTU";
            break;
        case ProtocolType::Modbus_TCP:
            name = "Modbus TCP";
            break;
        case ProtocolType::USB:
            name = "USB (USBTMC)";
            break;
        default:
            continue;
        }
        m_cmbProtocol->addItem(name, static_cast<int>(p));
    }

    // 嘗試恢復選擇
    for (int i = 0; i < m_cmbProtocol->count(); ++i) {
        if (m_cmbProtocol->itemData(i).toInt() == currentData) {
            m_cmbProtocol->setCurrentIndex(i);
            return;
        }
    }
}
