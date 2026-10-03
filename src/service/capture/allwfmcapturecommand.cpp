#include "capturefile.h"
#include "allwfmcapturecommand.h"
#include "iscopecapture.h"
#include "messageservice.h"
#include "capturelease.h"
#include <QObject>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QStringList>
#include <QCoreApplication>
#include "capturetaskrunner.h"

AllWfmCaptureCommand::AllWfmCaptureCommand(const CaptureContext& ctx) : m_ctx(ctx)
{
}

void AllWfmCaptureCommand::executeImpl()
{
    if (!m_ctx.oscilloscope) {
        MessageService::instance().showWarning(QObject::tr("Capture Failed"),
                                               QObject::tr("No oscilloscope available."));
        return;
    }
    auto captureLease = CaptureLease::tryAcquire(m_ctx.captureSession);
    if (!captureLease) {
        MessageService::instance().showWarning(QObject::tr("Capture Busy"),
                                               QObject::tr("A capture operation is in progress."));
        return;
    }
    if (!m_ctx.oscilloscope->isConnected()) {
        MessageService::instance().showWarning(QObject::tr("Capture Failed"),
                                               QObject::tr("Oscilloscope is not connected."));
        return;
    }

    // 在 Main Thread 查詢所有亮起的實體通道（避免與背景擷取互搶通訊）
    auto collectEnabledChannels = [this]() -> QList<int> {
        QList<int> result;
        const int total = m_ctx.oscilloscope->getTotalChannel();
        for (int i = 1; i <= total; ++i) {
            if (m_ctx.oscilloscope->isChannelEnabled(i))
                result.append(i);
        }
        return result;
    };

    const QList<int> activeChannels = collectEnabledChannels();

    if (activeChannels.isEmpty()) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Failed"),
            QObject::tr("No channels are currently enabled (lit up) on the oscilloscope."));
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString modelName = m_ctx.oscilloscope->model();
    const QString defaultFileName = QString("%1_waveform_ALL_%2.wfm").arg(modelName, timestamp);
    const QString defaultPath = QDir(m_ctx.lastSaveDir).filePath(defaultFileName);

    const QString filePath = m_ctx.requestSaveFile(QObject::tr("Save All Waveforms Data"), defaultPath,
                                                   QObject::tr("WFM File (*.wfm);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    if (m_ctx.onSaveDirChanged)
        m_ctx.onSaveDirChanged(filePath);

    std::shared_ptr<IScopeCapture> oscilloscopePtr = m_ctx.oscilloscope;

    runCaptureTask([oscilloscopePtr, captureLease, filePath, activeChannels]() {
        const QFileInfo fi(filePath);
        const QString dirPath = fi.absolutePath();
        const QString baseName = fi.completeBaseName();
        const QString ext = fi.suffix().isEmpty() ? "wfm" : fi.suffix();

        int successCount = 0;
        QStringList failedChannels;

        for (int ch : activeChannels) {
            const QString targetPath = dirPath + "/" + baseName + QString("_CH%1.").arg(ch) + ext;
            if (CaptureFile::waveform(oscilloscopePtr.get(), ch, targetPath, "WFM")) {
                ++successCount;
            } else {
                failedChannels << QString("CH%1").arg(ch);
            }
        }

        if (successCount == 0) {
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [oscilloscopePtr]() {
                    MessageService::instance().showWarning(
                        QObject::tr("Capture Failed"),
                        QObject::tr("Failed to capture WFM data.\nModel %1 may not support this feature.")
                            .arg(oscilloscopePtr->model()));
                },
                Qt::QueuedConnection);
            return;
        }

        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [dirPath, baseName, ext, successCount, failedChannels]() {
                QString message =
                    QObject::tr("Active Channels WFM saved successfully!\n\nDirectory:\n%1\n\nSaved "
                                "as:\n%2_CHx.%3\n\nSaved channels: %4")
                        .arg(dirPath, baseName, ext)
                        .arg(successCount);
                if (!failedChannels.isEmpty()) {
                    message += QObject::tr("\nFailed channels: %1").arg(failedChannels.join(", "));
                }

                MessageService::instance().showInfo(QObject::tr("Capture Complete"), message);
            },
            Qt::QueuedConnection);
    });
}
