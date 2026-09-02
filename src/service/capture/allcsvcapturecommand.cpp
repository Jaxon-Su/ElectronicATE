#include "allcsvcapturecommand.h"
#include "oscilloscope.h"
#include "messageservice.h"
#include <QObject>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QStringList>
#include <QMessageBox>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrentRun>

AllCsvCaptureCommand::AllCsvCaptureCommand(const CaptureContext& ctx)
    : m_ctx(ctx)
{}

void AllCsvCaptureCommand::execute()
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

    const QString timestamp       = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString modelName       = m_ctx.oscilloscope->model();
    const QString defaultFileName = QString("%1_waveform_ALL_%2.csv").arg(modelName, timestamp);
    const QString defaultPath     = QDir(m_ctx.lastSaveDir).filePath(defaultFileName);

    const QString filePath = QFileDialog::getSaveFileName(
        nullptr,
        QObject::tr("Save All Waveforms Data"),
        defaultPath,
        QObject::tr("CSV File (*.csv);;All Files (*)"));

    if (filePath.isEmpty()) return;

    if (m_ctx.onSaveDirChanged)
        m_ctx.onSaveDirChanged(filePath);

    bool expected = false;
    if (!m_ctx.captureInProgress->compare_exchange_strong(expected, true)) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Busy"),
            QObject::tr("A capture operation is in progress."));
        return;
    }

    std::shared_ptr<Oscilloscope>      oscilloscopePtr   = m_ctx.oscilloscope;
    std::shared_ptr<std::atomic<bool>> captureInProgress = m_ctx.captureInProgress;

    QtConcurrent::run([oscilloscopePtr, captureInProgress, filePath,
                       activeChannels]() {
        struct Guard {
            std::shared_ptr<std::atomic<bool>> flag;
            ~Guard() { if (flag) *flag = false; }
        } guard{captureInProgress};

        try {
            const QFileInfo fi(filePath);
            const QString dirPath  = fi.absolutePath();
            const QString baseName = fi.completeBaseName();
            const QString ext      = fi.suffix().isEmpty() ? "csv" : fi.suffix();

            int successCount = 0;
            QStringList failedChannels;

            for (int ch : activeChannels) {
                const QString targetPath =
                    dirPath + "/" + baseName + QString("_CH%1.").arg(ch) + ext;
                const QString scopeTempPath =
                    QString("C:\\TekScope\\Waveforms\\wave_ch%1.csv").arg(ch);

                if (oscilloscopePtr->captureWaveformFileToHost(
                        ch, targetPath, "CSV", scopeTempPath)) {
                    ++successCount;
                } else {
                    failedChannels << QString("CH%1").arg(ch);
                }
            }

            if (successCount == 0) {
                QMetaObject::invokeMethod(QCoreApplication::instance(),
                    [oscilloscopePtr]() {
                    MessageService::instance().showWarning(
                        QObject::tr("Capture Failed"),
                        QObject::tr("Failed to capture CSV data.\nModel %1 may not support this feature.")
                            .arg(oscilloscopePtr->model()));
                }, Qt::QueuedConnection);
                return;
            }

            QMetaObject::invokeMethod(QCoreApplication::instance(),
                [dirPath, baseName, ext, successCount, failedChannels]() {
                    QString message = QObject::tr(
                        "Active Channels CSV saved successfully!\n\nDirectory:\n%1\n\nSaved as:\n%2_CHx.%3\n\nSaved channels: %4")
                            .arg(dirPath, baseName, ext)
                            .arg(successCount);
                    if (!failedChannels.isEmpty()) {
                        message += QObject::tr("\nFailed channels: %1")
                            .arg(failedChannels.join(", "));
                    }

                    QMessageBox::information(
                        nullptr,
                        QObject::tr("Capture Complete"),
                        message);
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
