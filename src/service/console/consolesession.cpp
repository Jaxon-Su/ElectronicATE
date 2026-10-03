#include "consolesession.h"
#include "consoleexchange.h"
#include "modbusrtuexchange.h"
#include "icommunication.h"
#include <QThread>
#include <QDebug>
ConsoleSession::ConsoleSession(Creator create, std::shared_ptr<std::atomic<quint64>> revision)
    : m_create(std::move(create)), m_revision(std::move(revision))
{
}
ConsoleSession::~ConsoleSession()
{
    release();
}
void ConsoleSession::release() noexcept
{
    auto connection = std::move(m_connection);
    if (!connection)
        return;
    try {
        connection->close();
    } catch (...) {
        qWarning() << "Console transport close failed";
    }
}
void ConsoleSession::open(quint64 revision, const QString& address, int rtuQuietMs)
{
    release();
    m_rtuQuietMs = rtuQuietMs;
    QString error;
    bool success = false;
    try {
        if (m_revision->load() == revision) {
            m_connection = m_create ? m_create(address) : nullptr;
            success = m_connection && m_connection->open();
            if (!success)
                error = m_connection ? m_connection->lastError() : QStringLiteral("No transport for address");
        }
    } catch (const std::exception& e) {
        error = QString::fromUtf8(e.what());
    } catch (...) {
        error = QStringLiteral("Unknown connection error");
    }
    if (m_revision->load() != revision)
        success = false;
    if (!success)
        release();
    emit opened(revision, success, error);
}
void ConsoleSession::command(quint64 revision, const QString& text, int timeout)
{
    ConsoleExchangeResult result{ConsoleExchangeResult::Error::Interrupted, {}, {}};
    if (m_revision->load() == revision && m_connection && m_rtuQuietMs == 0)
        result = exchangeConsoleCommand(*m_connection, text, text.endsWith('?'), timeout,
                                        [this, revision] { return m_revision->load() == revision; });
    if (m_revision->load() != revision)
        result = {ConsoleExchangeResult::Error::Interrupted, {}, {}};
    emit completed(revision, text, static_cast<int>(result.error), result.response, result.detail);
}
void ConsoleSession::close(quint64 revision)
{
    release();
    emit closed(revision);
}
void ConsoleSession::modbusCommand(quint64 revision, const ModbusRequest &request, int timeout)
{
    ModbusExchangeResult result;
    result.error = "Modbus operation cancelled or RTU connection unavailable";
    if (m_revision->load() == revision && m_connection && m_rtuQuietMs > 0)
        result = exchangeModbusRtu(*m_connection, request, timeout, m_rtuQuietMs,
                                  [this, revision] { return m_revision->load() == revision; },
                                  [this, revision](const QByteArray &tx) { emit modbusTransmitting(revision, tx); });
    if (m_revision->load() != revision) result.error = "Modbus operation cancelled";
    // A timed-out/invalid exchange must not leave a late reply for the next command.
    if (!result.succeeded()) release();
    emit modbusCompleted(revision, result.tx, result.rx, result.summary, result.error, bool(m_connection));
}
void ConsoleSession::shutdown()
{
    release();
    QThread::currentThread()->quit();
}
