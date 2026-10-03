#pragma once
#include "../../service/testrun/testrunservice.h"

// Owns the service as a QObject child so both move to the same worker thread.
class Page5TestWorker : public QObject
{
    Q_OBJECT
  public:
    explicit Page5TestWorker(QObject *parent = nullptr);
    Page5TestWorker(InstrumentOperations operations, QObject *parent);
    Page5TestWorker(TestRunDependencies dependencies, QObject *parent);
    void prepareRun() { m_service->prepareRun(); }
  public slots:
    void startTasks(const QVector<TaskPayload> &payloads, const Page5ExecutionContext &context);
    void stop();
  signals:
    void taskStatusChanged(int runPanelIndex, TaskStatus status);
    void retryCountChanged(int runPanelIndex, int attempt);
    void logMessage(const QString &msg);
    void taskResultChanged(int runPanelIndex, const QString &result);
    void measurementReady(int runPanelIndex, const Page5ResultRecord &record, bool complete);
    void resultRecordsReady(const QVector<Page5ResultRecord> &records);
    void finished();

  private:
    TestRunService *m_service;
};
