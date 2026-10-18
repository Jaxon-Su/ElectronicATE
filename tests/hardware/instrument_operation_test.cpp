#include "instrumentoperationrunner.h"
#include <QCoreApplication>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QObject owner;
        for (int mode = 0; mode < 4; ++mode) {
            int completions = 0;
            bool workerThread = false;
            InstrumentOperationResult delivered;
            runInstrumentOperation(&owner, [&, mode]() -> InstrumentOperationResult {
                workerThread = QThread::currentThread() != app.thread();
                if (mode == 2) throw std::runtime_error("device error");
                if (mode == 3) throw 42;
                return {mode == 0, mode == 0 ? QString{} : QString("rejected")};
            }, [&](const InstrumentOperationResult& result) {
                require(QThread::currentThread() == owner.thread(), "completion ran outside owner thread");
                ++completions;
                delivered = result;
            });
            require(QThreadPool::globalInstance()->waitForDone(3000), "operation did not finish");
            QCoreApplication::processEvents();
            require(workerThread && completions == 1 && delivered.success == (mode == 0), "completion state mismatch");
            if (mode == 1) require(delivered.errorMessage == "rejected", "result error changed");
            if (mode == 2) require(delivered.errorMessage == "device error", "exception detail lost");
            if (mode == 3) require(delivered.errorMessage.contains("Unknown"), "unknown exception not handled");
        }
        QSemaphore started, resume;
        auto* destroyedOwner = new QObject;
        int staleCompletions = 0;
        runInstrumentOperation(destroyedOwner, [&] {
            started.release();
            resume.acquire();
            return InstrumentOperationResult{};
        }, [&](const InstrumentOperationResult&) { ++staleCompletions; });
        const bool running = started.tryAcquire(1, 3000);
        delete destroyedOwner;
        resume.release();
        const bool done = QThreadPool::globalInstance()->waitForDone(3000);
        QCoreApplication::processEvents();
        require(running && done && staleCompletions == 0, "deleted owner received completion");
        std::cout << "PASS: operation completion affinity, failures and deleted owner lifetime\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
