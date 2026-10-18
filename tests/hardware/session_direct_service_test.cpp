#include "testrunsession.h"
#include "testrunservicefactory.h"
#include "reportwriter.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QSemaphore>
#include <QThread>
#include <atomic>
#include <iostream>
#include <stdexcept>

// This test injects the real service; production wiring is not used.
std::unique_ptr<TestRunService> makeTestRunService()
{
    throw std::logic_error("Unexpected production wiring");
}
static void require(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}
template<class Predicate> void waitFor(Predicate predicate)
{
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < 3000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(predicate(), "Session completion timed out");
}
class Scope final : public Oscilloscope
{
public:
    QString model() const override { return "MSO54"; }
    QString vendor() const override { return "Test"; }
    QByteArray captureScreenshot(const QString &, const QString &) override { return {}; }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        const QString scenario = app.arguments().value(1);
        const bool report = scenario == "report";
        QSemaphore reportRelease;
        std::atomic<bool> reportStarted{false}, deletedOffGui{false};
        auto service = std::make_unique<TestRunService>(TestRunDependencies{});
        QPointer<TestRunService> serviceGuard(service.get());
        QObject::connect(service.get(), &QObject::destroyed, [&] {
            deletedOffGui = QThread::currentThread() != app.thread();
        });
        auto writer = std::make_unique<ReportWriter>(
            [&](const QString &, const QString &, const QVector<TestSequenceResultRecord> &) {
                reportStarted = true;
                if (!reportRelease.tryAcquire(1, 5000))
                    return ReportWriteResult{false, {}, "Test release timed out"};
                return ReportWriteResult{false, {}, "Injected report failure"};
            }, nullptr);
        TestRunSession session(std::move(service), std::move(writer));
        bool done = false, shutdown = false, reportFailed = false, sawDelay = false;
        QVector<TestSequenceResultRecord> records;
        QObject::connect(&session, &TestRunSession::resultRecordsReady, &app,
                         [&](const auto &value) { records = value; });
        QObject::connect(&session, &TestRunSession::runningChanged, &app, [&](bool running) {
            if (running && scenario == "before") session.stop();
            if (!running) {
                require(!records.isEmpty(), "Run unlocked before result handoff");
                done = true;
            }
        });
        QObject::connect(&session, &TestRunSession::logMessage, &app, [&](const QString &text) {
            if (text.contains("  Delay:")) {
                sawDelay = true;
                if (scenario == "during") session.stop();
            }
        });
        QObject::connect(&session, &TestRunSession::reportCompleted, &app,
                         [&](bool ok, const QString &, const QString &) { reportFailed = !ok; });
        QObject::connect(&session, &TestRunSession::shutdownFinished, &app, [&] { shutdown = true; });
        TaskPayload task; task.task.name = "Delay"; task.settings["delay_ms"] = report ? 0 : 30000;
        auto scope = std::make_shared<Scope>();
        std::weak_ptr<Scope> borrowed = scope;
        require(session.start({task}, {}, scope, report, "test", "unused"), "Start rejected");
        scope.reset();
        if (report) {
            waitFor([&] { return reportStarted.load(); });
            session.requestShutdown(); session.requestShutdown();
            QCoreApplication::processEvents();
            require(!done && !shutdown && !borrowed.expired(), "Lease released before report completion");
            require(!session.start({task}, {}, {}, false, {}, {}), "Shutdown admitted another Run");
            reportRelease.release();
            waitFor([&] { return shutdown; });
            require(reportFailed && done && borrowed.expired(), "Failed report did not release the session");
        } else {
            waitFor([&] { return done; });
            const bool before = scenario == "before";
            require(records.size() == 1 && records[0].status == (before ? "Skipped" : "Cancelled") &&
                    records[0].attempt == (before ? 0 : 1), "Stop was lost or delayed until task completion");
            require(sawDelay == (scenario == "during"), "Wrong cancellation boundary");
            require(borrowed.expired(), "Completed Run retained scope lease");
            session.requestShutdown();
            waitFor([&] { return shutdown; });
        }
        waitFor([&] { return deletedOffGui.load(); });
        require(serviceGuard.isNull(), "Service was not destroyed on its worker thread");
        std::cout << "PASS direct service session " << scenario.toStdString() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
