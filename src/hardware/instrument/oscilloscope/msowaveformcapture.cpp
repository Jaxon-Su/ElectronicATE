#include "msoseries456.h"
#include "msofilelisting.h"
#include "binaryfilestore.h"
#include <QDebug>
#include <QScopeGuard>
#include <QUuid>

namespace {
QString quoted(QString value)
{
    value.replace('"', "\"\"");
    return '"' + value + '"';
}
void appendError(QString &error, const QString &detail)
{
    if (!detail.isEmpty()) error += (error.isEmpty() ? "" : "; ") + detail;
}
}

bool MSOSeries456::checkCaptureStatus(const QString &stage, QString &error)
{
    QString response;
    if (!queryString("*ESR?", response)) {
        appendError(error, stage + ": status query failed: " + lastError());
        return false;
    }
    bool ok = false;
    const int status = response.simplified().section(' ', -1).toInt(&ok);
    if (!ok || status < 0 || status > 255) {
        appendError(error, stage + ": invalid ESR response: " + response);
        return false;
    }
    if (status & 0x3c) {
        appendError(error, QString("%1: instrument error (ESR=%2)").arg(stage).arg(status));
        return false;
    }
    return true;
}

// Full replies are required for directories too: queryString's 256-byte read
// would leave a long LDIR response queued ahead of the next query/file transfer.
bool MSOSeries456::readCaptureReply(const QString &command, QByteArray &reply, QString &error,
                                   bool &transportReady)
{
    bool received = false;
    try {
        received = queryRaw(command, reply);
        if (!received) appendError(error, command + ": " + lastError());
    } catch (const std::exception &e) {
        appendError(error, command + ": " + QString::fromUtf8(e.what()));
    } catch (...) {
        appendError(error, command + ": unexpected transfer error");
    }
    if (!received) {
        // Do not send an ESR query into an unfinished response. Clear the transport
        // first, then consume/report this operation's status. Never retry the capture.
        reply.clear();
        transportReady = false;
        if (!m_comm || !m_comm->deviceClear()) {
            appendError(error, "Capture transport recovery failed");
            return false;
        }
        transportReady = true;
    }
    const bool statusOk = checkCaptureStatus(command, error);
    return received && statusOk;
}

bool MSOSeries456::waveformFileSize(const QString &path, qint64 &size, QString &error,
                                   bool &transportReady)
{
    QByteArray reply;
    if (!readCaptureReply("FILESystem:CWD?", reply, error, transportReady)) return false;
    QString previous;
    bool restoreNeeded = false;
    try {
        previous = MsoFileListing::currentDirectory(reply);
        const auto slash = path.lastIndexOf('/');
        const QString directory = path.left(slash);
        // Mark before writing: even a failed status check may follow a successful change.
        restoreNeeded = true;
        QString settingError;
        if (!writeConfigurationCommand("FILESystem:CWD " + quoted(directory), settingError))
            appendError(error, "Capture directory: " + settingError);
        else if (readCaptureReply("FILESystem:LDIR?", reply, error, transportReady))
            size = MsoFileListing::fileSize(reply, path.mid(slash + 1));
    } catch (const std::exception &e) {
        appendError(error, "Capture file listing: " + QString::fromUtf8(e.what()));
    } catch (...) {
        appendError(error, "Capture file listing: unexpected error");
    }
    if (restoreNeeded && !transportReady)
        appendError(error, "Capture directory not restored: transport recovery failed");
    if (restoreNeeded && transportReady) {
        try {
            QString restoreError;
            if (!writeConfigurationCommand("FILESystem:CWD " + quoted(previous), restoreError))
                appendError(error, "Restore capture directory: " + restoreError);
        } catch (const std::exception &e) {
            appendError(error, "Restore capture directory: " + QString::fromUtf8(e.what()));
        } catch (...) {
            appendError(error, "Restore capture directory: unexpected error");
        }
    }
    return error.isEmpty() && size > 0;
}

QByteArray MSOSeries456::captureWaveformFile(int channel, const QString &format,
                                            const QString &, int, int)
{
    const int previousTimeout = m_comm ? m_comm->timeoutMs() : 0;
    const auto restoreTimeout = qScopeGuard([&] { if (m_comm) m_comm->setTimeoutMs(previousTimeout); });
    QString extension = format.toLower();
    if (extension != "wfm" && extension != "csv" && extension != "mat") extension = "csv";
    QString directory = m_scopeSaveDir;
    directory.replace('\\', '/');
    while (directory.endsWith('/')) directory.chop(1);
    const QString path = directory + "/mso_wv_" + QUuid::createUuid().toString(QUuid::Id128) + "." + extension;
    const QString source = channel == 0 ? "ALL" : QString("CH%1").arg(channel);
    QString error;
    QByteArray data;
    bool saved = false;
    bool transportReady = true;
    try {
        auto execute = [&] {
            if (!m_comm) { error = "No communication object"; return; }
            if (extension == "csv" && channel != 0 && !m_comm->deviceClear()) {
                error = "CSV device clear failed: " + m_comm->lastError();
                return;
            }
            if (!writeConfigurationCommand("*CLS", error) ||
                !writeConfigurationCommand("SAVe:WAVEform:GATing NONe", error)) return;
            if (!writeConfigurationCommand("SAVe:WAVEform " + source + ", " + quoted(path), error)) return;
            // Only a confirmed saved file is eligible for cleanup; never read stale files.
            m_comm->setTimeoutMs(120000);
            QByteArray completion;
            const bool completed = readCaptureReply("*OPC?", completion, error, transportReady);
            m_comm->setTimeoutMs(previousTimeout);
            if (!completed) return;
            if (completion.trimmed() != "1") {
                appendError(error, "Waveform SAVE: invalid completion response");
                return;
            }
            saved = true;
            qint64 expectedSize = -1;
            if ((extension == "wfm" || extension == "csv") && channel != 0 &&
                !waveformFileSize(path, expectedSize, error, transportReady)) return;
            m_comm->setTimeoutMs(60000);
            if (!readCaptureReply("FILESystem:READFile " + quoted(path), data, error, transportReady)) return;
            // LDIR counts file bytes; READFile may append a response LF outside the file.
            // Remove only that exact suffix at the known file boundary. Never trim file
            // content: CSV can end in newlines and WFM can contain any byte, including LF.
            if (expectedSize > 0 && data.size() - expectedSize == 1 && data.endsWith('\n')) {
                data.chop(1);
                qDebug() << "[MSO456] READFile: removed response LF after" << expectedSize << "file bytes";
            }
            if (data.isEmpty() || (expectedSize > 0 && data.size() != expectedSize)) {
                appendError(error, QString("Waveform size mismatch: received %1 / expected %2 bytes")
                                      .arg(data.size()).arg(expectedSize));
                if (expectedSize > 0 && data.size() > expectedSize)
                    appendError(error, "READFile extra bytes (hex)=" +
                                       QString::fromLatin1(data.mid(expectedSize, 16).toHex(' ')));
            }
        };
        execute();
    } catch (const std::exception &e) {
        appendError(error, QString::fromUtf8(e.what()));
    } catch (...) {
        appendError(error, "Unexpected waveform capture error");
    }
    if (m_comm) m_comm->setTimeoutMs(previousTimeout);
    if (saved && transportReady) {
        try {
            QString cleanupError;
            if (!writeConfigurationCommand("FILESystem:DELEte " + quoted(path), cleanupError))
                appendError(error, "Delete waveform temporary file: " + cleanupError);
        } catch (const std::exception &e) {
            appendError(error, "Delete waveform temporary file: " + QString::fromUtf8(e.what()));
        } catch (...) {
            appendError(error, "Delete waveform temporary file: unexpected error");
        }
    }
    // Cleanup/status queries must not replace the original operation's error.
    m_lastError = error;
    if (!error.isEmpty()) {
        qWarning() << "[MSO456] captureWaveformFile:" << source << extension << error;
        return {};
    }
    qDebug() << "[MSO456] captureWaveformFile complete:" << source << extension << data.size() << "bytes";
    return data;
}

bool MSOSeries456::captureWaveformFileToHost(int channel, const QString &hostPath,
                                           const QString &format, const QString &scopePath,
                                           int start, int stop)
{
    const auto bytes = captureWaveformFile(channel, format, scopePath, start, stop);
    if (bytes.isEmpty()) return false;
    const auto saved = BinaryFileStore::save(hostPath, bytes);
    if (!saved.succeeded()) {
        m_lastError = "Waveform host save failed: " + saved.detail;
        return false;
    }
    return true;
}
