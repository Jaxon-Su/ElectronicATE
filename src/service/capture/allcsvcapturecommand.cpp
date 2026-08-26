#include "allcsvcapturecommand.h"
#include "oscilloscope.h"
#include "messageservice.h"
#include <QObject>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QList>
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

    // 在 Main Thread 查詢所有亮起的通道（避免與背景輪詢衝突）
    auto collectEnabled = [this](bool isMath) -> QList<int> {
        QList<int> result;
        const int total = m_ctx.oscilloscope->getTotalChannel();
        for (int i = 1; i <= total; ++i) {
            const bool on = isMath
                ? m_ctx.oscilloscope->isMathChannelEnabled(i)
                : m_ctx.oscilloscope->isChannelEnabled(i);
            if (on) result.append(i);
        }
        return result;
    };

    const QList<int> activeChannels     = collectEnabled(false);
    const QList<int> activeMathChannels = collectEnabled(true);

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
                       activeChannels, activeMathChannels]() {
        struct Guard {
            std::shared_ptr<std::atomic<bool>> flag;
            ~Guard() { if (flag) *flag = false; }
        } guard{captureInProgress};

        try {
            const QFileInfo fi(filePath);
            const QString dirPath  = fi.absolutePath();
            const QString baseName = fi.completeBaseName();
            const QString ext      = fi.suffix().isEmpty() ? "csv" : fi.suffix();

            bool anySuccess = false;

            auto captureChannels = [&](const QList<int>& channels, const QString& prefix) {
                for (int ch : channels) {
                    const QString targetPath =
                        dirPath + "/" + baseName +
                        QString("_%1%2.").arg(prefix).arg(ch) + ext;
                    if (oscilloscopePtr->captureWaveformFileToHost(ch, targetPath, "CSV", ""))
                        anySuccess = true;
                }
            };

            captureChannels(activeChannels,     "CH");
            captureChannels(activeMathChannels, "MCH");

            if (!anySuccess) {
                QMetaObject::invokeMethod(QCoreApplication::instance(), []() {
                    MessageService::instance().showWarning(
                        QObject::tr("Capture Failed"),
                        QObject::tr("Failed to capture CSV data."));
                }, Qt::QueuedConnection);
                return;
            }

            QMetaObject::invokeMethod(QCoreApplication::instance(),
                [dirPath, baseName, ext]() {
                    QMessageBox::information(
                        nullptr,
                        QObject::tr("Capture Complete"),
                        QObject::tr("Active Channels CSV saved successfully!\n\nDirectory:\n%1\n\nSaved as:\n%2_CHx.%3")
                            .arg(dirPath, baseName, ext));
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
