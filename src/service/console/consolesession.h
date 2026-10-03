#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <atomic>
#include <functional>
#include <memory>
#include "modbusrequest.h"
class ICommunication;

// All transport construction, I/O and destruction stay on one worker thread.
class ConsoleSession : public QObject {
    Q_OBJECT
  public:
    using Creator = std::function<std::unique_ptr<ICommunication>(const QString&)>;
    ConsoleSession(Creator create, std::shared_ptr<std::atomic<quint64>> revision);
    ~ConsoleSession() override;
    void open(quint64 revision, const QString& address, int rtuQuietMs = 0);
    void command(quint64 revision, const QString& text, int timeout);
    void modbusCommand(quint64 revision, const ModbusRequest &request, int timeout);
    void close(quint64 revision);
    void shutdown();
  signals:
    void opened(quint64 revision, bool success, const QString& error);
    void completed(quint64 revision, const QString& command, int error, const QString& response,
                   const QString& detail);
    void closed(quint64 revision);
    void modbusCompleted(quint64 revision, const QByteArray &tx, const QByteArray &rx,
                         const QString &summary, const QString &error, bool connected);
    void modbusTransmitting(quint64 revision, const QByteArray &tx);

  private:
    void release() noexcept;
    Creator m_create;
    std::shared_ptr<std::atomic<quint64>> m_revision;
    std::unique_ptr<ICommunication> m_connection;
    int m_rtuQuietMs = 0;
};
