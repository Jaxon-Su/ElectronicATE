#pragma once
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

class CommScanProcess : public QObject {
    Q_OBJECT
  public:
    explicit CommScanProcess(QString program, QObject *parent = nullptr);
    ~CommScanProcess() override;
    bool isRunning() const { return m_process.state() != QProcess::NotRunning; }
    void start(int timeoutMs = 15000);
    void stop();
  signals:
    void messageReceived(const QJsonObject &message);
    void finished(const QString &error);
  private:
    void consumeOutput();
    QString m_program;
    QProcess m_process;
    QTimer m_timeout;
    QByteArray m_output;
    QString m_error;
    bool m_done = false;
};
