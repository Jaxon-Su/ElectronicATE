#include "pngcapturecommand.h"
#include "oscilloscope.h"
#include "messageservice.h"
#include <QObject>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrentRun>

PngCaptureCommand::PngCaptureCommand(const CaptureContext& ctx)
    : m_ctx(ctx)
{}

void PngCaptureCommand::execute()
{
    if (!m_ctx.oscilloscope) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Failed"),
            QObject::tr("No oscilloscope available."));
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

    const QString filePath = QFileDialog::getSaveFileName(
        nullptr,
        QObject::tr("Save Oscilloscope Screenshot"),
        defaultPath,
        QObject::tr("PNG Image (*.png);;BMP Image (*.bmp);;All Files (*)"));

    if (filePath.isEmpty()) return;

    if (m_ctx.onSaveDirChanged)
        m_ctx.onSaveDirChanged(filePath);

    const QString format = filePath.endsWith(".bmp", Qt::CaseInsensitive) ? "BMP" : "PNG";

    bool expected = false;
    if (!m_ctx.captureInProgress->compare_exchange_strong(expected, true)) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Busy"),
            QObject::tr("A capture operation is already in progress. Please wait."));
        return;
    }

    std::shared_ptr<Oscilloscope>      oscilloscopePtr   = m_ctx.oscilloscope;
    std::shared_ptr<std::atomic<bool>> captureInProgress = m_ctx.captureInProgress;

    QtConcurrent::run([oscilloscopePtr, captureInProgress, filePath, format]() {
        struct Guard {
            std::shared_ptr<std::atomic<bool>> flag;
            ~Guard() { if (flag) *flag = false; }
        } guard{captureInProgress};

        try {
            QByteArray imageData = oscilloscopePtr->captureScreenshot(format, filePath);

            if (imageData.isEmpty()) {
                QMetaObject::invokeMethod(QCoreApplication::instance(), []() {
                    MessageService::instance().showWarning(
                        QObject::tr("Capture Failed"),
                        QObject::tr("Failed to capture screenshot. Image data is empty."));
                }, Qt::QueuedConnection);
                return;
            }

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QMetaObject::invokeMethod(QCoreApplication::instance(), [filePath]() {
                    MessageService::instance().showWarning(
                        QObject::tr("Save Failed"),
                        QObject::tr("Failed to save file:\n%1").arg(filePath));
                }, Qt::QueuedConnection);
                return;
            }

            const qint64 bytesWritten = file.write(imageData);
            file.close();

            QMetaObject::invokeMethod(QCoreApplication::instance(), [filePath, bytesWritten]() {
                QMessageBox::information(
                    nullptr,
                    QObject::tr("Capture Complete"),
                    QObject::tr("Screenshot saved to:\n%1\n\nFile size: %2 KB")
                        .arg(filePath)
                        .arg(bytesWritten / 1024.0, 0, 'f', 1));
            }, Qt::QueuedConnection);

        } catch (const std::exception& e) {
            const QString errMsg = QString::fromStdString(e.what());
            QMetaObject::invokeMethod(QCoreApplication::instance(), [errMsg]() {
                MessageService::instance().showWarning(
                    QObject::tr("Capture Error"),
                    QObject::tr("Error: %1").arg(errMsg));
            }, Qt::QueuedConnection);
        }
    });
}
