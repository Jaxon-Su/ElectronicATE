#pragma once
#include <QObject>
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
class ICommunication;

// All transport construction, I/O and destruction stay on one worker thread.
class ConsoleSession : public QObject {
    Q_OBJECT
public:
    using Creator = std::function<std::unique_ptr<ICommunication>(const QString&)>;
    ConsoleSession(Creator create, std::shared_ptr<std::atomic<quint64>> revision);
    ~ConsoleSession() override;
    void open(quint64 revision, const QString& address);
    void command(quint64 revision, const QString& text, int timeout);
    void close(quint64 revision);
    void shutdown();
signals:
    void opened(quint64 revision, bool success, const QString& error);
    void completed(quint64 revision, const QString& command, int error, const QString& response, const QString& detail);
    void closed(quint64 revision);
private:
    void release() noexcept;
    Creator m_create;
    std::shared_ptr<std::atomic<quint64>> m_revision;
    std::unique_ptr<ICommunication> m_connection;
};
