#include "capturefile.h"
#include "waveformcaptureworkflow.h"
#include "iscopecapture.h"
#include "messageservice.h"
#include "capturelease.h"
#include <QObject>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include "capturetaskrunner.h"

void executeWaveformCapture(const CaptureContext& context, WaveformCaptureFormat format)
{
    const bool isCsv = format == WaveformCaptureFormat::Csv;
    const QString extension = isCsv ? QStringLiteral("csv") : QStringLiteral("wfm");
    const QString formatName = isCsv ? QStringLiteral("CSV") : QStringLiteral("WFM");
    if (!context.oscilloscope) {
        MessageService::instance().showWarning(QObject::tr("Capture Failed"),
                                               QObject::tr("No oscilloscope available."));
        return;
    }
    auto captureLease = CaptureLease::tryAcquire(context.captureSession);
    if (!captureLease) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Busy"),
            QObject::tr("A capture operation is already in progress. Please wait."));
        return;
    }
    if (!context.oscilloscope->isConnected()) {
        MessageService::instance().showWarning(QObject::tr("Capture Failed"),
                                               QObject::tr("Oscilloscope is not connected."));
        return;
    }
    const int captureChannel = context.captureChannel;
    const int channelCount = context.oscilloscope->getTotalChannel();
    if (captureChannel < 1 || captureChannel > channelCount) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Failed"),
            QObject::tr("Cannot determine a valid analog capture channel. Please select a channel supported "
                        "by this oscilloscope (CH1–CH%1).")
                .arg(channelCount));
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString modelName = context.oscilloscope->model();
    const QString defaultFileName = QString("%1_waveform_%2.%3").arg(modelName, timestamp, extension);
    const QString defaultPath = QDir(context.lastSaveDir).filePath(defaultFileName);

    const QString filePath = context.requestSaveFile(
        isCsv ? QObject::tr("Save Waveform Data") : QObject::tr("Save Waveform Data (WFM)"), defaultPath,
        isCsv ? QObject::tr("CSV File (*.csv);;All Files (*)")
              : QObject::tr("WFM File (*.wfm);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    if (context.onSaveDirChanged)
        context.onSaveDirChanged(filePath);

    std::shared_ptr<IScopeCapture> oscilloscopePtr = context.oscilloscope;

    runCaptureTask([oscilloscopePtr, captureLease, filePath, captureChannel, formatName, isCsv]() {
        const bool success =
            CaptureFile::waveform(oscilloscopePtr.get(), captureChannel, filePath, formatName);

        if (!success) {
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [oscilloscopePtr, isCsv]() {
                    MessageService::instance().showWarning(
                        QObject::tr("Capture Failed"),
                        (isCsv ? QObject::tr(
                                     "Failed to capture CSV data.\nModel %1 may not support this feature.")
                               : QObject::tr(
                                     "Failed to capture WFM data.\nModel %1 may not support this feature."))
                            .arg(oscilloscopePtr->model()));
                },
                Qt::QueuedConnection);
            return;
        }

        const qint64 fileSize = QFileInfo(filePath).size();
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [filePath, fileSize, captureChannel, isCsv]() {
                MessageService::instance().showInfo(
                    QObject::tr("Capture Complete"),
                    (isCsv ? QObject::tr("CSV saved to:\n%1\n\nChannel: CH%2\nFile size: %3 KB")
                           : QObject::tr("WFM saved to:\n%1\n\nChannel: CH%2\nFile size: %3 KB"))
                        .arg(filePath)
                        .arg(captureChannel)
                        .arg(fileSize / 1024.0, 0, 'f', 1));
            },
            Qt::QueuedConnection);
    });
}
