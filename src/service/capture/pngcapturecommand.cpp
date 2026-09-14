#include "pngcapturecommand.h"
#include "oscilloscope.h"
#include "messageservice.h"
#include "capturelease.h"
#include <QObject>
#include <QDateTime>
#include <QDir>
#include "binaryfilestore.h"
#include <QFileInfo>
#include <QCoreApplication>
#include "capturetaskrunner.h"

PngCaptureCommand::PngCaptureCommand(const CaptureContext& ctx)
    : m_ctx(ctx)
{}

void PngCaptureCommand::executeImpl()
{
    if (!m_ctx.oscilloscope) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Failed"),
            QObject::tr("No oscilloscope available."));
        return;
    }
    auto captureLease = CaptureLease::tryAcquire(m_ctx.captureSession);
    if (!captureLease) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Busy"),
            QObject::tr("A capture operation is already in progress. Please wait."));
        return;
    }
    if (!m_ctx.oscilloscope->isConnected()) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Failed"),
            QObject::tr("Oscilloscope is not connected."));
        return;
    }



    const QString timestamp       = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString modelName       = m_ctx.oscilloscope->model();
    const QString defaultFileName = QString("%1_screenshot_%2.png").arg(modelName, timestamp);
    const QString defaultPath     = QDir(m_ctx.lastSaveDir).filePath(defaultFileName);

    const QString filePath = m_ctx.requestSaveFile(
        QObject::tr("Save Oscilloscope Screenshot"),
        defaultPath,
        QObject::tr("PNG Image (*.png);;BMP Image (*.bmp);;All Files (*)"));

    if (filePath.isEmpty()) return;

    if (m_ctx.onSaveDirChanged)
        m_ctx.onSaveDirChanged(filePath);

    const QString format = filePath.endsWith(".bmp", Qt::CaseInsensitive) ? "BMP" : "PNG";

    std::shared_ptr<Oscilloscope>      oscilloscopePtr   = m_ctx.oscilloscope;

    runCaptureTask([oscilloscopePtr, captureLease, filePath, format]() {

        QByteArray imageData = oscilloscopePtr->captureScreenshot(format, filePath);

        if (imageData.isEmpty()) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), []() {
                MessageService::instance().showWarning(
                    QObject::tr("Capture Failed"),
                    QObject::tr("Failed to capture screenshot. Image data is empty."));
            }, Qt::QueuedConnection);
            return;
        }

        const auto saved = BinaryFileStore::save(filePath, imageData);
        if (!saved.succeeded()) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [filePath]() {
                MessageService::instance().showWarning(
                    QObject::tr("Save Failed"),
                    QObject::tr("Failed to save file:\n%1").arg(filePath));
            }, Qt::QueuedConnection);
            return;
        }

        const qint64 bytesWritten = saved.bytesWritten;

        QMetaObject::invokeMethod(QCoreApplication::instance(), [filePath, bytesWritten]() {
            MessageService::instance().showInfo(
                QObject::tr("Capture Complete"),
                QObject::tr("Screenshot saved to:\n%1\n\nFile size: %2 KB")
                    .arg(filePath)
                    .arg(bytesWritten / 1024.0, 0, 'f', 1));
        }, Qt::QueuedConnection);

    });
}
