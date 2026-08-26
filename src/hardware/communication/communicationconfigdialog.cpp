#include "communicationconfigdialog.h"
#include "communicationfactory.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QTimer>
#include <QThread>  // 用於 msleep

CommunicationConfigDialog::CommunicationConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("通訊設定 / Communication Settings"));
    setMinimumWidth(450);
    setMinimumHeight(350);
    setupUI();
}

void CommunicationConfigDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // ===== 協議選擇區域 =====
    auto* protocolGroup = new QGroupBox(tr("協議類型 / Protocol Type"));
    auto* protocolLayout = new QHBoxLayout(protocolGroup);

    m_cmbProtocol = new QComboBox;
    m_cmbProtocol->addItem("GPIB (IEEE-488)",           static_cast<int>(ProtocolType::GPIB));
    m_cmbProtocol->addItem("TCP/IP (VXI-11)",           static_cast<int>(ProtocolType::TCP_VXI11));
    m_cmbProtocol->addItem("Serial (RS-232/RS-485)",    static_cast<int>(ProtocolType::Serial));
    m_cmbProtocol->addItem("Modbus RTU",                static_cast<int>(ProtocolType::Modbus_RTU));
    // m_cmbProtocol->addItem("Modbus TCP",                static_cast<int>(ProtocolType::Modbus_TCP));
    // m_cmbProtocol->addItem("USB (USBTMC)",              static_cast<int>(ProtocolType::USB));

    protocolLayout->addWidget(m_cmbProtocol);
    mainLayout->addWidget(protocolGroup);

    // ===== 參數配置區域（堆疊頁面）=====
    m_stackedWidget = new QStackedWidget;

    createGpibPage();           // Index 0
    createTcpPage();            // Index 1
    createSerialPage();         // Index 2
    createModbusRtuPage();      // Index 3
    createModbusTcpPage();      // Index 4
    createUsbPage();            // Index 5

    // 建立協議到頁面的映射
    m_protocolPageMap[ProtocolType::GPIB]       = 0;
    m_protocolPageMap[ProtocolType::TCP_VXI11]  = 1;
    m_protocolPageMap[ProtocolType::Serial]     = 2;
    m_protocolPageMap[ProtocolType::Modbus_RTU] = 3;
    m_protocolPageMap[ProtocolType::Modbus_TCP] = 4;
    m_protocolPageMap[ProtocolType::USB]        = 5;

    mainLayout->addWidget(m_stackedWidget, 1);

    // ===== 測試連線區域 =====
    auto* testGroup = new QGroupBox(tr("連線測試 / Connection Test"));
    auto* testLayout = new QHBoxLayout(testGroup);

    m_btnTest = new QPushButton(tr("測試連線"));
    m_btnTest->setFixedWidth(100);
    m_lblStatus = new QLabel;
    m_lblStatus->setWordWrap(true);

    testLayout->addWidget(m_btnTest);
    testLayout->addWidget(m_lblStatus, 1);
    mainLayout->addWidget(testGroup);

    // ===== 按鈕區域 =====
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(m_buttonBox);

    // ===== 連接信號 =====
    connect(m_cmbProtocol, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CommunicationConfigDialog::onProtocolChanged);
    connect(m_btnTest, &QPushButton::clicked,
            this, &CommunicationConfigDialog::onTestConnection);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
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
    layout->setSpacing(10);

    m_edtIpAddress = new QLineEdit;
    m_edtIpAddress->setPlaceholderText("192.168.1.100");
    m_edtIpAddress->setToolTip(tr("儀器的 IP 位址"));
    layout->addRow(tr("IP 位址："), m_edtIpAddress);

    m_spnPort = new QSpinBox;
    m_spnPort->setRange(1, 65535);
    m_spnPort->setValue(5025);
    m_spnPort->setToolTip(tr("連接埠 (VXI-11 通常由 VISA 自動處理；保留此欄位供相容設定使用)"));
    layout->addRow(tr("連接埠 (Port)："), m_spnPort);

    // TCP 子協議選擇
    m_cmbTcpProtocol = new QComboBox;
    m_cmbTcpProtocol->addItem("VXI-11 (INSTR)", static_cast<int>(ProtocolType::TCP_VXI11));
    layout->addRow(tr("TCP 協議："), m_cmbTcpProtocol);

    // 常用埠說明
    auto* lblHint = new QLabel(tr(
        "示波器通訊請使用 VXI-11：TCPIP0::儀器IP::INSTR\n"
        "Delta AC Source=8462, Chroma=3300"));
    lblHint->setStyleSheet("color: gray; font-size: 10px;");
    layout->addRow("", lblHint);

    // 預覽標籤
    auto* lblPreview = new QLabel;
    lblPreview->setStyleSheet("color: gray; font-style: italic;");
    layout->addRow(tr("資源字串："), lblPreview);

    auto updatePreview = [this, lblPreview]() {
        CommunicationConfig cfg;
        cfg.protocol = static_cast<ProtocolType>(m_cmbTcpProtocol->currentData().toInt());
        cfg.ipAddress = m_edtIpAddress->text();
        cfg.port = m_spnPort->value();
        lblPreview->setText(cfg.toResourceString());
    };

    connect(m_edtIpAddress, &QLineEdit::textChanged, updatePreview);
    connect(m_spnPort, QOverload<int>::of(&QSpinBox::valueChanged), updatePreview);
    connect(m_cmbTcpProtocol, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    updatePreview();

    m_stackedWidget->addWidget(m_pageTcp);
}

void CommunicationConfigDialog::createSerialPage()
{
    m_pageSerial = new QWidget;
    auto* layout = new QFormLayout(m_pageSerial);
    layout->setSpacing(8);

    // 串口選擇 + 刷新按鈕
    auto* portLayout = new QHBoxLayout;
    m_cmbPortName = new QComboBox;
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
    m_cmbBaudRate->addItems({"1200", "2400", "4800", "9600", "19200", "38400",
                             "57600", "115200", "230400", "460800", "921600"});
    m_cmbBaudRate->setCurrentText("9600");
    layout->addRow(tr("鮑率 (Baud)："), m_cmbBaudRate);

    // 資料位元
    m_cmbDataBits = new QComboBox;
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
    m_cmbFlowControl->addItem(tr("無 (None)"), "N");
    m_cmbFlowControl->addItem(tr("硬體 (RTS/CTS)"), "HW");
    m_cmbFlowControl->addItem(tr("軟體 (XON/XOFF)"), "SW");
    layout->addRow(tr("流量控制："), m_cmbFlowControl);

    // 預覽
    auto* lblPreview = new QLabel;
    lblPreview->setStyleSheet("color: gray; font-style: italic;");
    layout->addRow(tr("資源字串："), lblPreview);

    auto updatePreview = [this, lblPreview]() {
        CommunicationConfig cfg;
        cfg.protocol = ProtocolType::Serial;
        cfg.portName = m_cmbPortName->currentData().isValid()
                           ? m_cmbPortName->currentData().toString()
                           : m_cmbPortName->currentText();
        cfg.baudRate = m_cmbBaudRate->currentText().toInt();
        cfg.dataBits = m_cmbDataBits->currentText().toInt();
        cfg.parity = m_cmbParity->currentData().toString();
        cfg.stopBits = m_cmbStopBits->currentData().toString();
        cfg.flowControl = m_cmbFlowControl->currentData().toString();
        lblPreview->setText(cfg.toResourceString());
    };

    connect(m_cmbPortName, &QComboBox::currentTextChanged, updatePreview);
    connect(m_cmbBaudRate, &QComboBox::currentTextChanged, updatePreview);
    connect(m_cmbDataBits, &QComboBox::currentTextChanged, updatePreview);
    connect(m_cmbParity, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    connect(m_cmbStopBits, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    connect(m_cmbFlowControl, QOverload<int>::of(&QComboBox::currentIndexChanged), updatePreview);
    updatePreview();

    m_stackedWidget->addWidget(m_pageSerial);
}

void CommunicationConfigDialog::createModbusRtuPage()
{
    m_pageModbusRtu = new QWidget;
    auto* layout = new QFormLayout(m_pageModbusRtu);
    layout->setSpacing(8);

    // Slave ID (放在最上面，最重要)
    m_spnModbusRtuSlaveId = new QSpinBox;
    m_spnModbusRtuSlaveId->setRange(1, 255);
    m_spnModbusRtuSlaveId->setValue(1);
    m_spnModbusRtuSlaveId->setToolTip(tr("Modbus 從站地址 (1-255)"));
    layout->addRow(tr("Slave ID："), m_spnModbusRtuSlaveId);

    // 串口選擇
    auto* portLayout = new QHBoxLayout;
    m_cmbModbusRtuPort = new QComboBox;
    m_cmbModbusRtuPort->setEditable(true);
    populateSerialPorts(m_cmbModbusRtuPort);

    m_btnRefreshModbusPorts = new QPushButton(tr("🔄"));
    m_btnRefreshModbusPorts->setFixedWidth(30);
    connect(m_btnRefreshModbusPorts, &QPushButton::clicked, [this]() {
        populateSerialPorts(m_cmbModbusRtuPort);
    });

    portLayout->addWidget(m_cmbModbusRtuPort, 1);
    portLayout->addWidget(m_btnRefreshModbusPorts);
    layout->addRow(tr("串口："), portLayout);

    // 鮑率
    m_cmbModbusRtuBaud = new QComboBox;
    m_cmbModbusRtuBaud->setEditable(true);
    m_cmbModbusRtuBaud->addItems({"9600", "19200", "38400", "57600", "115200"});
    m_cmbModbusRtuBaud->setCurrentText("9600");
    layout->addRow(tr("鮑率："), m_cmbModbusRtuBaud);

    // 資料位元
    m_cmbModbusRtuDataBits = new QComboBox;
    m_cmbModbusRtuDataBits->addItems({"7", "8"});
    m_cmbModbusRtuDataBits->setCurrentText("8");
    layout->addRow(tr("資料位元："), m_cmbModbusRtuDataBits);

    // 校驗位
    m_cmbModbusRtuParity = new QComboBox;
    m_cmbModbusRtuParity->addItem(tr("無 (None)"), "N");
    m_cmbModbusRtuParity->addItem(tr("偶校驗 (Even)"), "E");
    m_cmbModbusRtuParity->addItem(tr("奇校驗 (Odd)"), "O");
    layout->addRow(tr("校驗位："), m_cmbModbusRtuParity);

    // 停止位
    m_cmbModbusRtuStopBits = new QComboBox;
    m_cmbModbusRtuStopBits->addItem("1", "1");
    m_cmbModbusRtuStopBits->addItem("2", "2");
    layout->addRow(tr("停止位："), m_cmbModbusRtuStopBits);

    // 常用配置說明
    auto* lblHint = new QLabel(tr("常用：9600, 8, N, 1 或 9600, 8, E, 1"));
    lblHint->setStyleSheet("color: gray; font-size: 10px;");
    layout->addRow("", lblHint);

    m_stackedWidget->addWidget(m_pageModbusRtu);
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
    m_spnModbusTcpPort->setValue(502);  // Modbus TCP 標準埠
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
    auto* lblHint = new QLabel(tr(
        "常用 Vendor ID：\n"
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
    QString currentText = comboBox->currentText();
    comboBox->clear();

    for (const auto& info : QSerialPortInfo::availablePorts()) {
        QString displayText = QString("%1 - %2").arg(info.portName(), info.description());
        comboBox->addItem(displayText, info.portName());
    }

    // 嘗試恢復之前選擇
    if (!currentText.isEmpty()) {
        int index = comboBox->findText(currentText, Qt::MatchContains);
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
        } else {
            comboBox->setEditText(currentText);
        }
    }
}

void CommunicationConfigDialog::onProtocolChanged(int index)
{
    auto protocol = static_cast<ProtocolType>(m_cmbProtocol->itemData(index).toInt());
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
    m_lblStatus->setText(tr("測試中..."));
    m_lblStatus->setStyleSheet("color: orange;");
    m_btnTest->setEnabled(false);

    // 使用 QTimer 確保 UI 更新
    QTimer::singleShot(100, this, [this]() {
        CommunicationConfig config = getConfig();
        QString resource = config.toResourceString();

        if (resource.isEmpty()) {
            m_lblStatus->setText(tr("❌ 配置不完整"));
            m_lblStatus->setStyleSheet("color: red;");
            m_btnTest->setEnabled(true);
            return;
        }

        // 使用 CommunicationFactory 測試連線
        ICommunication* comm = CommunicationFactory::create(resource);
        if (!comm) {
            m_lblStatus->setText(tr("❌ 無法建立連線物件\n資源字串: %1").arg(resource));
            m_lblStatus->setStyleSheet("color: red;");
            m_btnTest->setEnabled(true);
            return;
        }

        if (comm->open()) {
            // ===== 修正：使用 write + read 代替 query =====
            // 發送 *IDN? 查詢
            QByteArray cmdData = "*IDN?\n";
            int writeResult = comm->write(cmdData);

            QString idn;
            if (writeResult > 0) {
                // 等待儀器回應
                QThread::msleep(500);

                // 讀取回應
                QByteArray response;
                int readResult = comm->read(response, 1024);

                if (readResult > 0) {
                    idn = QString::fromUtf8(response).trimmed();
                }
            }

            comm->close();

            if (!idn.isEmpty()) {
                m_lblStatus->setText(tr("✓ 連線成功\n%1").arg(idn.left(80)));
                m_lblStatus->setStyleSheet("color: green;");
            } else {
                m_lblStatus->setText(tr("⚠ 連線成功但 *IDN? 無回應\n(部分儀器不支援此指令)"));
                m_lblStatus->setStyleSheet("color: #CC7700;");
            }
        } else {
            QString errorMsg = comm->lastError();
            m_lblStatus->setText(tr("❌ 連線失敗\n%1").arg(
                errorMsg.isEmpty() ? resource : errorMsg));
            m_lblStatus->setStyleSheet("color: red;");
        }

        delete comm;
        m_btnTest->setEnabled(true);
    });
}

void CommunicationConfigDialog::setConfig(const CommunicationConfig& config)
{
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
        for (int i = 0; i < m_cmbTcpProtocol->count(); ++i) {
            if (m_cmbTcpProtocol->itemData(i).toInt() == static_cast<int>(config.protocol)) {
                m_cmbTcpProtocol->setCurrentIndex(i);
                break;
            }
        }
        break;

    case ProtocolType::Serial:
        m_cmbPortName->setEditText(config.portName);
        m_cmbBaudRate->setCurrentText(QString::number(config.baudRate));
        m_cmbDataBits->setCurrentText(QString::number(config.dataBits));
        for (int i = 0; i < m_cmbParity->count(); ++i) {
            if (m_cmbParity->itemData(i).toString() == config.parity) {
                m_cmbParity->setCurrentIndex(i);
                break;
            }
        }
        for (int i = 0; i < m_cmbStopBits->count(); ++i) {
            if (m_cmbStopBits->itemData(i).toString() == config.stopBits) {
                m_cmbStopBits->setCurrentIndex(i);
                break;
            }
        }
        for (int i = 0; i < m_cmbFlowControl->count(); ++i) {
            if (m_cmbFlowControl->itemData(i).toString() == config.flowControl) {
                m_cmbFlowControl->setCurrentIndex(i);
                break;
            }
        }
        break;

    case ProtocolType::Modbus_RTU:
        m_spnModbusRtuSlaveId->setValue(config.slaveId);
        m_cmbModbusRtuPort->setEditText(config.portName);
        m_cmbModbusRtuBaud->setCurrentText(QString::number(config.baudRate));
        m_cmbModbusRtuDataBits->setCurrentText(QString::number(config.dataBits));
        for (int i = 0; i < m_cmbModbusRtuParity->count(); ++i) {
            if (m_cmbModbusRtuParity->itemData(i).toString() == config.parity) {
                m_cmbModbusRtuParity->setCurrentIndex(i);
                break;
            }
        }
        for (int i = 0; i < m_cmbModbusRtuStopBits->count(); ++i) {
            if (m_cmbModbusRtuStopBits->itemData(i).toString() == config.stopBits) {
                m_cmbModbusRtuStopBits->setCurrentIndex(i);
                break;
            }
        }
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
}

CommunicationConfig CommunicationConfigDialog::getConfig() const
{
    CommunicationConfig config;

    // 獲取當前協議類型
    int protocolData = m_cmbProtocol->currentData().toInt();

    // 對於 TCP 類型，需要從子協議選單獲取
    if (protocolData == static_cast<int>(ProtocolType::TCP_VXI11) ||
        protocolData == static_cast<int>(ProtocolType::TCP_Socket) ||
        protocolData == static_cast<int>(ProtocolType::TCP_HiSLIP)) {
        config.protocol = static_cast<ProtocolType>(m_cmbTcpProtocol->currentData().toInt());
    } else {
        config.protocol = static_cast<ProtocolType>(protocolData);
    }

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
        config.portName = m_cmbPortName->currentData().isValid()
                              ? m_cmbPortName->currentData().toString()
                              : m_cmbPortName->currentText().split(" - ").first().trimmed();
        config.baudRate = m_cmbBaudRate->currentText().toInt();
        config.dataBits = m_cmbDataBits->currentText().toInt();
        config.parity = m_cmbParity->currentData().toString();
        config.stopBits = m_cmbStopBits->currentData().toString();
        config.flowControl = m_cmbFlowControl->currentData().toString();
        break;

    case ProtocolType::Modbus_RTU:
        config.slaveId = m_spnModbusRtuSlaveId->value();
        config.portName = m_cmbModbusRtuPort->currentData().isValid()
                              ? m_cmbModbusRtuPort->currentData().toString()
                              : m_cmbModbusRtuPort->currentText().split(" - ").first().trimmed();
        config.baudRate = m_cmbModbusRtuBaud->currentText().toInt();
        config.dataBits = m_cmbModbusRtuDataBits->currentText().toInt();
        config.parity = m_cmbModbusRtuParity->currentData().toString();
        config.stopBits = m_cmbModbusRtuStopBits->currentData().toString();
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
        case ProtocolType::GPIB:        name = "GPIB (IEEE-488)"; break;
        case ProtocolType::TCP_VXI11:   name = "TCP/IP (VXI-11)"; break;
        case ProtocolType::TCP_Socket:
        case ProtocolType::TCP_HiSLIP:
            continue;
        case ProtocolType::Serial:      name = "Serial (RS-232/RS-485)"; break;
        case ProtocolType::Modbus_RTU:  name = "Modbus RTU"; break;
        case ProtocolType::Modbus_TCP:  name = "Modbus TCP"; break;
        case ProtocolType::USB:         name = "USB (USBTMC)"; break;
        default: continue;
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
