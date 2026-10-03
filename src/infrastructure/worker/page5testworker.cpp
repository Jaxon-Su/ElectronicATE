#include "page5testworker.h"

Page5TestWorker::Page5TestWorker(TestRunDependencies dependencies, QObject *parent)
    : QObject(parent), m_service(new TestRunService(std::move(dependencies), this))
{
    connect(m_service, &TestRunService::taskStatusChanged, this, &Page5TestWorker::taskStatusChanged);
    connect(m_service, &TestRunService::retryCountChanged, this, &Page5TestWorker::retryCountChanged);
    connect(m_service, &TestRunService::logMessage, this, &Page5TestWorker::logMessage);
    connect(m_service, &TestRunService::taskResultChanged, this, &Page5TestWorker::taskResultChanged);
    connect(m_service, &TestRunService::resultRecordsReady, this, &Page5TestWorker::resultRecordsReady);
    connect(m_service, &TestRunService::measurementReady, this, &Page5TestWorker::measurementReady);
    connect(m_service, &TestRunService::finished, this, &Page5TestWorker::finished);
}
void Page5TestWorker::startTasks(const QVector<TaskPayload> &payloads, const Page5ExecutionContext &context)
{
    m_service->startTasks(payloads, context);
}
void Page5TestWorker::stop() { m_service->stop(); }
