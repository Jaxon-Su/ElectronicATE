#include "commscanprocess.h"
#include <QJsonDocument>

CommScanProcess::CommScanProcess(QString program, QObject *parent)
    : QObject(parent), m_program(std::move(program))
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        m_error = "Resource enumeration timed out; results may be incomplete.";
        m_process.kill();
    });
    connect(&m_process, &QProcess::started, this, [this] {
        m_process.closeWriteChannel();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &CommScanProcess::consumeOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] { m_process.readAllStandardError(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_timeout.stop();
            emit finished("Cannot start scanner: " + m_process.errorString());
        }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        m_timeout.stop();
        consumeOutput();
        if (m_error.isEmpty() && (status != QProcess::NormalExit || code != 0 || !m_done))
            m_error = "Scanner exited without a complete result.";
        emit finished(m_error);
    });
}

CommScanProcess::~CommScanProcess()
{
    disconnect(&m_process, nullptr, this, nullptr);
    if (isRunning()) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

void CommScanProcess::start(int timeoutMs)
{
    if (isRunning())
        return;
    m_output.clear();
    m_error.clear();
    m_done = false;
    m_timeout.start(qMax(1, timeoutMs));
    m_process.start(m_program, {"--scan-comm-helper"});
}

void CommScanProcess::stop()
{
    if (!isRunning())
        return;
    m_error = "Stopped.";
    m_process.kill();
}

void CommScanProcess::consumeOutput()
{
    m_output += m_process.readAllStandardOutput();
    if (m_output.size() > 1024 * 1024) {
        m_error = "Scanner returned too much data.";
        m_process.kill();
        return;
    }
    int newline;
    while ((newline = m_output.indexOf('\n')) >= 0) {
        const auto line = m_output.left(newline);
        m_output.remove(0, newline + 1);
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            m_error = "Scanner returned an invalid result.";
            m_process.kill();
            return;
        }
        const auto message = document.object();
        m_done |= message.value("done").toBool();
        emit messageReceived(message);
    }
}
