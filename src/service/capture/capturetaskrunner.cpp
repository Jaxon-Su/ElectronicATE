#include "capturetaskrunner.h"
#include "messageservice.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <exception>
#include <utility>

namespace {
void reportFailure(const QString& message)
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), [message] {
        MessageService::instance().showWarning(
            QObject::tr("Capture Error"), QObject::tr("Error: %1").arg(message));
    }, Qt::QueuedConnection);
}
}

void runCaptureTask(std::function<void()> task)
{
    QThreadPool::globalInstance()->start([task = std::move(task)] {
        try {
            task();
        } catch (const std::exception& error) {
            reportFailure(QString::fromStdString(error.what()));
        } catch (...) {
            reportFailure(QObject::tr("Unknown capture error."));
        }
    });
}
