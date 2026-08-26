#include "page4viewmodel.h"
#include "page4model.h"
#include "icommunication.h"
#include "communicationfactory.h"
#include <QRegularExpression>
#include <QThread>
#include <QCoreApplication>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDebug>

Page4ViewModel::Page4ViewModel(Page4Model *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_readTimer(new QTimer(this))
{
    m_readTimer->setSingleShot(true);
    connect(m_readTimer, &QTimer::timeout, this, &Page4ViewModel::onReadTimeout);

    // 監聽 Model 變化，轉發給 View
    connect(m_model, &Page4Model::addressHistoryChanged,
            this, &Page4ViewModel::historyUpdated);
    connect(m_model, &Page4Model::commandHistoryChanged,
            this, &Page4ViewModel::historyUpdated);
}

Page4ViewModel::~Page4ViewModel()
{
    closeConnection();
}

// ==================== 連線操作 ====================

void Page4ViewModel::connectToAddress(const QString &address)
{
    QString trimmedAddr = address.trimmed();

    if (trimmedAddr.isEmpty()) {
        emit errorOccurred(tr("請輸入有效的地址"));
        return;
    }

    // 如果已連線，先斷開
    if (m_comm && m_comm->isOpen()) {
        closeConnection();
    }

    // 更新狀態：連線中
    m_model->setConnectionStatus(ConnectionStatus::Connecting);
    m_model->setCurrentAddress(trimmedAddr);
    emit connectionStatusChanged(ConnectionStatus::Connecting, tr("連線中..."));

    // 使用工廠創建通訊物件
    m_comm.reset(CommunicationFactory::create(trimmedAddr));

    if (!m_comm) {
        m_model->setConnectionStatus(ConnectionStatus::Error);
        emit connectionStatusChanged(ConnectionStatus::Error, tr("無法解析地址格式"));
        emit errorOccurred(tr("無法解析地址格式: %1").arg(trimmedAddr));
        return;
    }

    // 嘗試開啟連線
    if (!m_comm->open()) {
        QString error = m_comm->lastError();
        m_model->setConnectionStatus(ConnectionStatus::Error);
        emit connectionStatusChanged(ConnectionStatus::Error, tr("連線失敗"));
        emit errorOccurred(tr("連線失敗: %1").arg(error));

        m_comm.reset();
        return;
    }

    // 連線成功
    m_model->setConnectionStatus(ConnectionStatus::Connected);
    m_model->addToAddressHistory(trimmedAddr);
    emit connectionStatusChanged(ConnectionStatus::Connected,
                                 tr("已連線到 %1").arg(trimmedAddr));
}

void Page4ViewModel::disconnect()
{
    closeConnection();
    m_model->setConnectionStatus(ConnectionStatus::Disconnected);
    emit connectionStatusChanged(ConnectionStatus::Disconnected, tr("已斷線"));
}

bool Page4ViewModel::isConnected() const
{
    return m_comm && m_comm->isOpen();
}

// ==================== 指令操作 ====================

void Page4ViewModel::sendCommand(const QString &command)
{
    QString cmd = command.trimmed();

    if (cmd.isEmpty()) {
        emit errorOccurred(tr("請輸入指令"));
        return;
    }

    if (!isConnected()) {
        emit errorOccurred(tr("請先連線到儀器"));
        return;
    }

    // 加入命令歷史
    m_model->addToCommandHistory(cmd);
    m_lastCommand = cmd;
    emit commandSent(cmd);

    // 判斷是否為查詢指令
    bool isQuery = cmd.endsWith('?');
    QString response = executeCommand(cmd, isQuery);

    if (isQuery) {
        if (!response.isEmpty()) {
            m_model->recordCommand(cmd, response, true);
            emit responseReceived(cmd, response);
        } else {
            m_model->recordCommand(cmd, "No response", false);
            emit responseReceived(cmd, tr("(無回應)"));
        }
    } else {
        // 非查詢指令，標記為成功
        m_model->recordCommand(cmd, "OK", true);
        emit responseReceived(cmd, tr("OK (無需回應)"));
    }
}

void Page4ViewModel::sendQuery(const QString &command)
{
    QString cmd = command.trimmed();
    if (!cmd.endsWith('?')) {
        cmd += '?';
    }
    sendCommand(cmd);
}

// ==================== 常用指令 ====================

void Page4ViewModel::sendIDN()
{
    sendCommand("*IDN?");
}

void Page4ViewModel::sendRST()
{
    sendCommand("*RST");
}

void Page4ViewModel::sendCLS()
{
    sendCommand("*CLS");
}

void Page4ViewModel::sendOPC()
{
    sendCommand("*OPC?");
}

// ==================== 歷史管理 ====================

void Page4ViewModel::clearAllHistory()
{
    m_model->clearCommandHistory();
    m_model->clearFullHistory();
}

// ==================== 配置 ====================

void Page4ViewModel::setTimeout(int ms)
{
    m_model->setTimeout(ms);
}

int Page4ViewModel::timeout() const
{
    return m_model->timeout();
}

// ==================== XML 序列化 ====================

void Page4ViewModel::writeXml(QXmlStreamWriter &writer) const
{
    m_model->writeXml(writer);
}

void Page4ViewModel::loadXml(QXmlStreamReader &reader)
{
    m_model->loadXml(reader);

    // 載入後發送信號通知 UI 更新
    emit historyUpdated();
}

// ==================== 內部方法 ====================

QString Page4ViewModel::executeCommand(const QString &command, bool expectResponse)
{
    if (!m_comm || !m_comm->isOpen()) {
        return QString();
    }

    // 準備命令（加換行符）
    QString cmdToSend = command;
    if (!cmdToSend.endsWith('\n')) {
        cmdToSend += '\n';
    }

    // 發送命令
    int bytesWritten = m_comm->write(cmdToSend.toUtf8());
    if (bytesWritten < 0) {
        emit errorOccurred(tr("發送失敗: %1").arg(m_comm->lastError()));
        return QString();
    }

    if (!expectResponse) {
        return QString();
    }

    // 等待回應
    QByteArray responseData;
    QString response;
    int timeoutMs = m_model->timeout();
    int elapsed = 0;
    const int pollInterval = 10;  // 每 10ms 檢查一次

    while (elapsed < timeoutMs) {
        QByteArray chunk;
        int bytesRead = m_comm->read(chunk, 4096);

        if (bytesRead > 0) {
            responseData.append(chunk);

            // 檢查是否收到完整回應（以換行結尾）
            if (responseData.contains('\n')) {
                break;
            }
        }

        QThread::msleep(pollInterval);
        QCoreApplication::processEvents();
        elapsed += pollInterval;
    }

    if (responseData.isEmpty() && elapsed >= timeoutMs) {
        emit errorOccurred(tr("回應逾時"));
    }

    response = QString::fromUtf8(responseData).trimmed();
    return response;
}

void Page4ViewModel::closeConnection()
{
    if (m_comm) {
        m_comm->close();
        m_comm.reset();
    }
    m_readTimer->stop();
}

void Page4ViewModel::onReadTimeout()
{
    // 讀取超時處理（如果需要非阻塞讀取可以擴展）
    qDebug() << "[Page4ViewModel] Read timeout";
}

bool Page4ViewModel::parseAddress(const QString &address, CommType &type)
{
    QString addr = address.trimmed().toUpper();

    if (addr.startsWith("TCPIP")) {
        type = CommType::TCPIP;
        return true;
    }
    if (addr.startsWith("GPIB")) {
        type = CommType::GPIB;
        return true;
    }
    if (addr.startsWith("COM") || addr.startsWith("/DEV/TTY")) {
        type = CommType::Serial;
        return true;
    }
    if (addr.startsWith("USB")) {
        type = CommType::USB;
        return true;
    }
    if (addr.startsWith("MODBUS")) {
        type = CommType::Modbus;
        return true;
    }

    // 簡化格式：IP:Port
    static const QRegularExpression ipPortRx(
        R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}:\d+$)");
    if (ipPortRx.match(address).hasMatch()) {
        type = CommType::TCPIP;
        return true;
    }

    type = CommType::Unknown;
    return false;
}
