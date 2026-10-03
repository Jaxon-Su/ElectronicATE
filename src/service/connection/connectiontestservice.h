#pragma once
#include "icommunication.h"
#include <QObject>
#include <functional>
#include <memory>

struct ConnectionTestResult
{
    QString resource;
    bool opened = false;
    QString identity;
    QString error;
};

// Owns a short-lived connection on a worker; destroying the owner only discards
// delivery.
class ConnectionTestService : public QObject
{
    Q_OBJECT
  public:
    using Creator = std::function<std::unique_ptr<ICommunication>(const QString &)>;
    explicit ConnectionTestService(Creator create, QObject *parent = nullptr);
    bool start(const QString &resource);
    bool isBusy() const { return m_busy; }
  signals:
    void completed(const ConnectionTestResult &result);

  private:
    Creator m_create;
    bool m_busy = false;
};

std::unique_ptr<ConnectionTestService> makeConnectionTestService();
