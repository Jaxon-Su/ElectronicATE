#include "page4viewmodel.h"
#include "page4model.h"
#include "icommunication.h"
#include <QPointer>
#include <QScopeGuard>
#include <QDebug>
#include "consoleexchange.h"


#include <QCoreApplication>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>


Page4ViewModel::Page4ViewModel(Page4Model *model, CommunicationCreator create, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_create(std::move(create))

{

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
    if (m_comm) {
        closeConnection();
    }

    // 更新狀態：連線中
    m_model->setConnectionStatus(ConnectionStatus::Connecting);
    m_model->setCurrentAddress(trimmedAddr);
    emit connectionStatusChanged(ConnectionStatus::Connecting, tr("連線中..."));

    // 使用工廠創建通訊物件
    ++m_connectionRevision;
    QString failure;
    bool opened = false;
    try {
        m_comm = m_create ? m_create(trimmedAddr) : nullptr;
        if (m_comm) {
            opened = m_comm->open();
            if (!opened) failure = m_comm->lastError();
        } else {
            failure = tr("無法解析地址格式: %1").arg(trimmedAddr);
        }
    } catch (const std::exception& error) {
        failure = QString::fromUtf8(error.what());
    } catch (...) {
        failure = tr("通訊初始化發生未知錯誤");
    }

    if (!opened) {
        // Release ownership before notifying slots that may retry.
        closeConnection();
        QPointer<Page4ViewModel> alive(this);
        const auto revision = m_connectionRevision;
        m_model->setConnectionStatus(ConnectionStatus::Error);
        emit connectionStatusChanged(ConnectionStatus::Error, tr("連線失敗"));
        if (alive && m_connectionRevision == revision)
            emit errorOccurred(tr("連線失敗: %1").arg(failure));
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
    QPointer<Page4ViewModel> alive(this);
    if (m_commandInProgress) {
        emit errorOccurred(tr("指令仍在執行中，請稍後再試"));
        return;
    }
    m_commandInProgress = true;
    const auto releaseCommand = qScopeGuard([alive] {
        if (alive) alive->m_commandInProgress = false;
    });
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
    if (!alive) return;

    emit commandSent(cmd);
    if (!alive) return;

    // 判斷是否為查詢指令
    bool isQuery = cmd.endsWith('?');
    const auto result = executeCommand(cmd, isQuery);
    if (!alive) return;
    if (!result.success) {
        m_model->recordCommand(cmd, result.error.isEmpty() ? "No response" : result.error, false);
        if (!alive) return;
        if (!result.error.isEmpty()) emit errorOccurred(result.error);
        else emit responseReceived(cmd, tr("(無回應)"));
        return;
    }
    const QString& response = result.response;

    if (isQuery) {
        m_model->recordCommand(cmd, response, true);
        if (alive) emit responseReceived(cmd, response);
    } else {
        // 非查詢指令，標記為成功
        m_model->recordCommand(cmd, "OK", true);
        if (alive) emit responseReceived(cmd, tr("OK (無需回應)"));
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

Page4ViewModel::CommandResult Page4ViewModel::executeCommand(const QString &command, bool expectResponse)
{
    if (!m_comm) return {false, {}, tr("請先連線到儀器")};
    const quint64 revision = m_connectionRevision;
    QPointer<Page4ViewModel> alive(this);
    const auto result = exchangeConsoleCommand(*m_comm, command, expectResponse, m_model->timeout(),
        [alive, revision] {
            QCoreApplication::processEvents();
            return alive && alive->m_connectionRevision == revision;
        });
    using Error = ConsoleExchangeResult::Error;
    switch (result.error) {
    case Error::None: return {true, result.response, {}};
    case Error::Disconnected: return {false, {}, tr("請先連線到儀器")};
    case Error::Write: return {false, {}, tr("發送失敗: %1").arg(result.detail)};
    case Error::Read: return {false, {}, tr("讀取失敗: %1").arg(result.detail)};
    case Error::Timeout: return {false, {}, tr("回應逾時")};
    case Error::Interrupted: return {false, {}, tr("連線已變更")};
    case Error::NoResponse: return {};
    case Error::Exception: return {false, {}, tr("通訊失敗: %1").arg(result.detail)};
    }
    return {};
}
void Page4ViewModel::closeConnection()
{
    ++m_connectionRevision;
    // Detach first so failed cleanup cannot leave an apparently active owner.
    // Local ownership also guarantees destruction when the transport throws.
    auto connection = std::move(m_comm);
    if (!connection) return;
    try { connection->close(); }
    catch (const std::exception& error) {
        qWarning() << "[Page4ViewModel] close failed:" << error.what();
    } catch (...) {
        qWarning() << "[Page4ViewModel] unknown close failure";
    }
}

void Page4ViewModel::validateXml(QXmlStreamReader& reader) const
{
    Page4Model candidate;
    candidate.loadXml(reader);
}
