#pragma once
#include "page5taskpayload.h"
#include "page5resultrecord.h"
#include "taskstatus.h"
#include <QObject>
#include <memory>
class Page5TestWorker;
class ReportWriter;
class QThread;
class Oscilloscope;

// Owns execution, report completion and the scope lease as one lifetime.
class TestRunSession : public QObject
{
    Q_OBJECT
  public:
    TestRunSession(std::unique_ptr<Page5TestWorker> worker, std::unique_ptr<ReportWriter> writer);
    ~TestRunSession() override;
    bool start(const QVector<TaskPayload> &tasks, Page5ExecutionContext context,
               std::shared_ptr<Oscilloscope> scope, bool report, QString name, QString directory);
    void stop();
    void requestShutdown();
    bool isShutdownComplete() const { return m_shutdownComplete; }
  signals:
    void taskStatusChanged(int index, TaskStatus status);
    void retryCountChanged(int index, int attempt);
    void taskResultChanged(int index, const QString &text);
    void measurementReady(int index, const Page5ResultRecord &record, bool complete);
    void resultRecordsReady(const QVector<Page5ResultRecord> &records);
    void logMessage(const QString &text);
    void reportCompleted(bool success, const QString &path, const QString &error);
    void runningChanged(bool running);
    void shutdownFinished();

  private:
    void finishWhenIdle();
    Page5TestWorker *m_worker;
    ReportWriter *m_writer;
    QThread *m_thread;
    std::shared_ptr<Oscilloscope> m_scope;
    bool m_running = false, m_runFinished = false, m_shutdownRequested = false, m_shutdownComplete = false;
    bool m_report = false;
    QString m_name, m_directory;
};
std::unique_ptr<TestRunSession> makeTestRunSession();
