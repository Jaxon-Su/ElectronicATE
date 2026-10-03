#include "testrunsession.h"
#include "testrunservicefactory.h"
#include "../execution/reportwriter.h"
#include <QThread>
#include <stdexcept>

std::unique_ptr<TestRunSession> makeTestRunSession()
{
    return std::make_unique<TestRunSession>(makeTestRunService(),
                                            std::make_unique<ReportWriter>());
}
TestRunSession::TestRunSession(std::unique_ptr<TestRunService> service, std::unique_ptr<ReportWriter> writer)
    : m_service(service.get()), m_writer(writer.get()), m_thread(new QThread(this))
{
    if (!service || !writer)
        throw std::invalid_argument("Missing execution session dependency");
    m_writer->setParent(this);
    writer.release();
    m_service->setParent(nullptr);
    m_service->moveToThread(m_thread);
    service.release();
    connect(m_thread, &QThread::finished, m_service, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, this, [this] {
        m_service = nullptr;
        m_shutdownComplete = true;
        emit shutdownFinished();
    });
    connect(m_service, &TestRunService::taskStatusChanged, this, &TestRunSession::taskStatusChanged);
    connect(m_service, &TestRunService::retryCountChanged, this, &TestRunSession::retryCountChanged);
    connect(m_service, &TestRunService::taskResultChanged, this, &TestRunSession::taskResultChanged);
    connect(m_service, &TestRunService::measurementReady, this, &TestRunSession::measurementReady);
    connect(m_service, &TestRunService::logMessage, this, &TestRunSession::logMessage);
    connect(m_service, &TestRunService::outputsUncertain, this, &TestRunSession::outputsUncertain);
    connect(m_service, &TestRunService::resultRecordsReady, this,
            [this](const QVector<Page5ResultRecord> &records) {
                emit resultRecordsReady(records);
                if (m_report) {
                    emit logMessage("Saving Excel report...");
                    if (!m_writer->submit(m_directory, m_name, records))
                        emit reportCompleted(false, {}, "Report writer rejected the request");
                }
            });
    connect(m_service, &TestRunService::finished, this, [this] {
        m_runFinished = true;
        finishWhenIdle();
    });
    connect(m_writer, &ReportWriter::completed, this,
            [this](bool ok, const QString &path, const QString &error) {
                emit reportCompleted(ok, path, error);
                finishWhenIdle();
            });
    m_thread->start();
}
TestRunSession::~TestRunSession()
{
    // Fallback for non-UI owners; normal close waits asynchronously for shutdownFinished.
    if (m_service)
        m_service->stop();
    m_thread->quit();
    m_thread->wait();
}
bool TestRunSession::start(const QVector<TaskPayload> &tasks, Page5ExecutionContext context,
                           std::shared_ptr<Oscilloscope> scope, bool report, QString name, QString directory)
{
    if (m_running || m_shutdownRequested || tasks.isEmpty() || !m_service)
        return false;
    m_scope = std::move(scope);
    context.scope = m_scope.get();
    m_report = report;
    m_name = std::move(name);
    m_directory = std::move(directory);
    m_runFinished = false;
    m_running = true;
    m_service->prepareRun();
    emit runningChanged(true);
    const bool queued = QMetaObject::invokeMethod(
        m_service, [service = m_service, tasks, context] { service->startTasks(tasks, context); },
        Qt::QueuedConnection);
    if (!queued) {
        m_runFinished = true;
        finishWhenIdle();
    }
    return queued;
}
void TestRunSession::stop()
{
    // Direct atomic cancellation: the service thread may be inside a blocking task.
    if (m_service && m_running)
        m_service->stop();
}
void TestRunSession::requestShutdown()
{
    if (m_shutdownRequested)
        return;
    m_shutdownRequested = true;
    stop();
    finishWhenIdle();
}
void TestRunSession::finishWhenIdle()
{
    if (m_running && m_runFinished && !m_writer->isBusy()) {
        m_running = false;
        m_scope.reset();
        emit runningChanged(false);
    }
    if (m_shutdownRequested && !m_running && !m_writer->isBusy()) {
        m_service = nullptr;
        m_thread->quit();
    }
}
