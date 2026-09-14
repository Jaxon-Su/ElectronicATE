#include "icapturecommand.h"
#include "messageservice.h"
#include <QObject>
#include <exception>

void ICaptureCommand::execute()
{
    try {
        executeImpl();
    } catch (const std::exception& error) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Error"),
            QObject::tr("Error: %1").arg(QString::fromUtf8(error.what())));
    } catch (...) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Error"),
            QObject::tr("Error: %1").arg(QObject::tr("Unknown capture error.")));
    }
}
