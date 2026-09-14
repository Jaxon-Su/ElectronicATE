#include "page4viewmodel.h"
#include "page4model.h"
#include "icommunication.h"
#include <QPointer>
#include <QScopeGuard>
#include <QDebug>
#include "consoleexchange.h"
#include "consolesession.h"
#include <QThread>


#include <QCoreApplication>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>


Page4ViewModel::Page4ViewModel(Page4Model* model, CommunicationCreator create, QObject* parent)
    : QObject(parent), m_model(model), m_revision(std::make_shared<std::atomic<quint64>>(0))
{
    m_thread = new QThread;
    m_session = new ConsoleSession(std::move(create), m_revision);
    m_session->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_session, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_session, &ConsoleSession::opened, this, [this](quint64 revision, bool success, const QString& error) {
        if (revision != m_connectionRevision) return;
        m_connectionPending = false;
        m_connected = success;
        m_connectionActive = success;
        const auto status = success ? ConnectionStatus::Connected : ConnectionStatus::Error;
        QPointer<Page4ViewModel> alive(this);
        m_model->setConnectionStatus(status);
        if (!alive) return;
        if (success) m_model->addToAddressHistory(m_model->currentAddress());
        if (!alive) return;
        emit connectionStatusChanged(status, success ? tr("已連線") : tr("連線失敗"));
        if (!alive || revision != m_connectionRevision) return;
        emit controlActiveChanged(isControlActive());
        if (alive && !success && revision == m_connectionRevision) emit errorOccurred(error);
    });
    connect(m_session, &ConsoleSession::closed, this, [this](quint64 revision) {
        if (revision != m_connectionRevision) return;
        m_connectionPending = false;
        m_connected = false;
        m_connectionActive = false;
        QPointer<Page4ViewModel> alive(this);
        m_model->setConnectionStatus(ConnectionStatus::Disconnected);
        if (!alive) return;
        emit connectionStatusChanged(ConnectionStatus::Disconnected, tr("已斷線"));
        if (alive) emit controlActiveChanged(isControlActive());
    });
    connect(m_session, &ConsoleSession::completed, this,
        [this](quint64 revision, const QString& command, int code, const QString& response, const QString& detail) {
        if (revision != m_commandRevision) return;
        QPointer<Page4ViewModel> alive(this);
        const auto finish = qScopeGuard([alive] {
            if (!alive) return;
            alive->m_commandInProgress = false;
            emit alive->controlActiveChanged(alive->isControlActive());
        });
        using Error = ConsoleExchangeResult::Error;
        const auto error = revision == m_connectionRevision ? static_cast<Error>(code) : Error::Interrupted;
        QString message;
        switch (error) {
        case Error::None: break;
        case Error::Disconnected: message = tr("請先連線到儀器"); break;
        case Error::Write: message = tr("發送失敗: %1").arg(detail); break;
        case Error::Read: message = tr("讀取失敗: %1").arg(detail); break;
        case Error::Timeout: message = tr("回應逾時"); break;
        case Error::Interrupted: message = tr("連線已變更或操作已取消"); break;
        case Error::NoResponse: message = tr("無回應"); break;
        case Error::Exception: message = tr("通訊失敗: %1").arg(detail); break;
        }
        const bool success = error == Error::None;
        m_model->recordCommand(command, success ? (command.endsWith('?') ? response : QStringLiteral("OK")) : message, success);
        if (!alive) return;
        if (success) emit responseReceived(command, command.endsWith('?') ? response : tr("OK (無需回應)"));
        else emit errorOccurred(message);
    });
    connect(m_model, &Page4Model::addressHistoryChanged, this, &Page4ViewModel::historyUpdated);
    connect(m_model, &Page4Model::commandHistoryChanged, this, &Page4ViewModel::historyUpdated);
    m_thread->start();
}
Page4ViewModel::~Page4ViewModel()
{
    ++*m_revision;
    QMetaObject::invokeMethod(m_session, [session = m_session] { session->shutdown(); }, Qt::QueuedConnection);
}
void Page4ViewModel::connectToAddress(const QString& address)
{
    if (!m_controlAllowed) return;
    const QString target = address.trimmed();
    if (target.isEmpty()) { emit errorOccurred(tr("請輸入有效的地址")); return; }
    const auto revision = ++m_connectionRevision;
    m_revision->store(revision);
    m_connected = false;
    m_connectionPending = true;
    m_connectionActive = true;
    QPointer<Page4ViewModel> alive(this);
    // Queue before notifications, so reentrant requests retain FIFO order.
    QMetaObject::invokeMethod(m_session, [session = m_session, revision, target] { session->open(revision, target); }, Qt::QueuedConnection);
    m_model->setCurrentAddress(target);
    if (!alive || revision != m_connectionRevision) return;
    m_model->setConnectionStatus(ConnectionStatus::Connecting);
    if (!alive || revision != m_connectionRevision) return;
    emit controlActiveChanged(true);
    if (alive && revision == m_connectionRevision) emit connectionStatusChanged(ConnectionStatus::Connecting, tr("連線中..."));
}
void Page4ViewModel::disconnect()
{
    const auto revision = ++m_connectionRevision;
    m_revision->store(revision);
    m_connected = false;
    m_connectionPending = true;
    m_connectionActive = true;
    QMetaObject::invokeMethod(m_session, [session = m_session, revision] { session->close(revision); }, Qt::QueuedConnection);
    emit controlActiveChanged(true);
}
bool Page4ViewModel::isConnected() const { return m_connected; }
void Page4ViewModel::sendCommand(const QString& command)
{
    if (!m_controlAllowed) return;
    if (isOperationPending()) { emit errorOccurred(tr("操作仍在執行中，請稍後再試")); return; }
    const QString text = command.trimmed();
    if (text.isEmpty()) { emit errorOccurred(tr("請輸入指令")); return; }
    if (!m_connected) { emit errorOccurred(tr("請先連線到儀器")); return; }
    m_commandInProgress = true;
    m_commandRevision = m_connectionRevision;
    const auto revision = m_commandRevision;
    const int timeout = m_model->timeout();
    QPointer<Page4ViewModel> alive(this);
    QMetaObject::invokeMethod(m_session, [session = m_session, revision, text, timeout] { session->command(revision, text, timeout); }, Qt::QueuedConnection);
    m_model->addToCommandHistory(text);
    if (!alive) return;
    emit controlActiveChanged(true);
    if (alive) emit commandSent(text);
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

void Page4ViewModel::validateXml(QXmlStreamReader& reader) const
{
    Page4Model candidate;
    candidate.loadXml(reader);
}

bool Page4ViewModel::isControlActive() const
{
    return m_connectionActive || m_commandInProgress;
}

void Page4ViewModel::publishXmlLoaded()
{
    emit historyUpdated();
}
