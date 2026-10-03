#include "connectiontestservice.h"
#include <QFutureWatcher>
#include <QScopeGuard>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

ConnectionTestService::ConnectionTestService(Creator create, QObject *parent)
    : QObject(parent), m_create(std::move(create))
{
}

bool ConnectionTestService::start(const QString &resource)
{
    if (m_busy || resource.trimmed().isEmpty())
        return false;
    m_busy = true;
    auto *watcher = new QFutureWatcher<ConnectionTestResult>(this);
    connect(watcher, &QFutureWatcher<ConnectionTestResult>::finished, this,
            [this, watcher]
            {
                const auto result = watcher->result();
                watcher->deleteLater();
                m_busy = false;
                emit completed(result);
            });
    watcher->setFuture(QtConcurrent::run(
        [create = m_create, resource]
        {
            ConnectionTestResult result;
            result.resource = resource;
            try
            {
                auto connection = create ? create(resource) : nullptr;
                if (!connection)
                {
                    result.error = "Cannot create a connection for this resource";
                    return result;
                }
                const auto close = qScopeGuard(
                    [&]
                    {
                        try
                        {
                            connection->close();
                        }
                        catch (...)
                        {
                            result.error += " Connection cleanup failed";
                        }
                    });
                result.opened = connection->open();
                if (!result.opened)
                {
                    result.error = connection->lastError();
                }
                else if (connection->write("*IDN?\n") > 0)
                {
                    QThread::msleep(500);
                    QByteArray response;
                    if (connection->read(response, 1024) > 0)
                        result.identity = QString::fromUtf8(response).trimmed();
                }
            }
            catch (const std::exception &error)
            {
                result.error = QString::fromUtf8(error.what());
            }
            catch (...)
            {
                result.error = "Unexpected connection test error";
            }
            return result;
        }));
    return true;
}
