#include "messageservice.h"

MessageService& MessageService::instance()
{
    static MessageService instance;
    return instance;
}

void MessageService::showWarning(const QString& title, const QString& message)
{
    emit warningRequested(title, message);
}

void MessageService::showError(const QString& title, const QString& message)
{
    emit errorRequested(title, message);
}

void MessageService::showInfo(const QString& title, const QString& message)
{
    emit infoRequested(title, message);
}

// Jaxon check
//發射信號後暫時由mainwindow統一處理!
//mainwindow:setupMessageService()有各種connection
