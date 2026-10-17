#pragma once
#include "testsequencetaskpayload.h"
#include "testsequenceresultrecord.h"
#include "taskstatus.h"
#include <QObject>
#include <memory>
class TestRunService;
class ReportWriter;
class QThread;
class Oscilloscope;

// Owns execution, report completion and the scope lease as one lifetime.
class TestRunSession : public QObject
{
    Q_OBJECT
  public:
    TestRunSession(std::unique_ptr<TestRunService> service, std::unique_ptr<ReportWriter> writer);
    ~TestRunSession() override;
    bool start(const QVector<TaskPayload> &tasks, TestSequenceExecutionContext context,
               std::shared_ptr<Oscilloscope> scope, bool report, QString name, QString directory);
    void stop();
    void requestShutdown();
    bool isShutdownComplete() const { return m_shutdownComplete; }
  signals:
    void outputsUncertain(const InstrumentSetupConfig &config, const QVector<TableKind> &outputs,
                          const ShortCircuitProtection &protection = {});
    void taskStatusChanged(int index, TaskStatus status);
    void retryCountChanged(int index, int attempt);
    void taskResultChanged(int index, const QString &text);
    void measurementReady(int index, const TestSequenceResultRecord &record, bool complete);
    void resultRecordsReady(const QVector<TestSequenceResultRecord> &records);
    void logMessage(const QString &text);
    void reportCompleted(bool success, const QString &path, const QString &error);
    void runningChanged(bool running);
    void shutdownFinished();

  private:
    void finishWhenIdle();
    TestRunService *m_service;
    ReportWriter *m_writer;
    QThread *m_thread;
    std::shared_ptr<Oscilloscope> m_scope;
    bool m_running = false, m_runFinished = false, m_shutdownRequested = false, m_shutdownComplete = false;
    bool m_report = false;
    QString m_name, m_directory;
};
std::unique_ptr<TestRunSession> makeTestRunSession();
