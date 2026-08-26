#include "wfmcapturecommand.h"
#include "oscilloscope.h"
#include "messageservice.h"
#include <QObject>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrentRun>

WfmCaptureCommand::WfmCaptureCommand(const CaptureContext& ctx)
    : m_ctx(ctx)
{}

void WfmCaptureCommand::execute()
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

    // 優先用 UI 選擇的通道；fallback 查詢儀器 trigger source
    int captureChannel = m_ctx.captureChannel;
    if (captureChannel <= 0) {
        const QString triggerSource = m_ctx.oscilloscope->getTriggerSource();
        if (triggerSource.startsWith("CH", Qt::CaseInsensitive)) {
            bool ok;
            const int ch = triggerSource.mid(2).toInt(&ok);
            if (ok && ch >= 1 && ch <= 4)
                captureChannel = ch;
        }
        if (captureChannel <= 0) captureChannel = 1;
    }

    const QString timestamp       = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString modelName       = m_ctx.oscilloscope->model();
    const QString defaultFileName = QString("%1_waveform_%2.wfm").arg(modelName, timestamp);
    const QString defaultPath     = QDir(m_ctx.lastSaveDir).filePath(defaultFileName);

    const QString filePath = QFileDialog::getSaveFileName(
        nullptr,
        QObject::tr("Save Waveform Data (WFM)"),
        defaultPath,
        QObject::tr("WFM File (*.wfm);;All Files (*)"));

    if (filePath.isEmpty()) return;

    if (m_ctx.onSaveDirChanged)
        m_ctx.onSaveDirChanged(filePath);

    bool expected = false;
    if (!m_ctx.captureInProgress->compare_exchange_strong(expected, true)) {
        MessageService::instance().showWarning(
            QObject::tr("Capture Busy"),
            QObject::tr("A capture operation is already in progress. Please wait."));
        return;
    }

    std::shared_ptr<Oscilloscope>      oscilloscopePtr   = m_ctx.oscilloscope;
    std::shared_ptr<std::atomic<bool>> captureInProgress = m_ctx.captureInProgress;

    QtConcurrent::run([oscilloscopePtr, captureInProgress, filePath, captureChannel]() {
        struct Guard {
            std::shared_ptr<std::atomic<bool>> flag;
            ~Guard() { if (flag) *flag = false; }
        } guard{captureInProgress};

        try {
            // 示波器端暫存路徑（INTERNal 格式，副檔名 .wfm）
            const QString scopeTempPath =
                QString("C:\\TekScope\\Waveforms\\wave_ch%1.wfm").arg(captureChannel);

            const bool success = oscilloscopePtr->captureWaveformFileToHost(
                captureChannel, filePath, "WFM", scopeTempPath);

            if (!success) {
                QMetaObject::invokeMethod(QCoreApplication::instance(),
                    [oscilloscopePtr]() {
                        MessageService::instance().showWarning(
                            QObject::tr("Capture Failed"),
                            QObject::tr("Failed to capture WFM data.\nModel %1 may not support this feature.")
                                .arg(oscilloscopePtr->model()));
                    }, Qt::QueuedConnection);
                return;
            }

            const qint64 fileSize = QFileInfo(filePath).size();
            QMetaObject::invokeMethod(QCoreApplication::instance(),
                [filePath, fileSize, captureChannel]() {
                    QMessageBox::information(
                        nullptr,
                        QObject::tr("Capture Complete"),
                        QObject::tr("WFM saved to:\n%1\n\nChannel: CH%2\nFile size: %3 KB")
                            .arg(filePath)
                            .arg(captureChannel)
                            .arg(fileSize / 1024.0, 0, 'f', 1));
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
