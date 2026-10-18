#include "instrumentoperationqueue.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <QSemaphore>
#include <iostream>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        InstrumentOperationQueue queue;
        QVector<int> order;
        int completed = 0;
        for (int i = 0; i < 3; ++i) {
            queue.submit([&, i]() -> InstrumentOperationResult {
                order.append(i);
                if (i == 1) throw std::runtime_error("expected failure");
                return {true, {}};
            }, [&, i](const InstrumentOperationResult& result) {
                require(result.success == (i != 1), "failure conversion changed");
                ++completed;
                if (i == 0) queue.submit([&] { order.append(3); return InstrumentOperationResult{true, {}}; },
                                        [&](const auto&) { ++completed; });
            });
        }
        for (int i = 0; i < 4; ++i) {
            require(QThreadPool::globalInstance()->waitForDone(3000), "worker stalled");
            QCoreApplication::processEvents();
        }
        require(completed == 4 && order == QVector<int>{0, 1, 2, 3}, "FIFO/reentrant submission failed");
        QSemaphore started, resume;
        auto* deleted = new InstrumentOperationQueue;
        int stale = 0;
        deleted->submit([&] { started.release(); resume.acquire(); return InstrumentOperationResult{}; },
                        [&](const auto&) { ++stale; });
        deleted->submit([&] { ++stale; return InstrumentOperationResult{}; }, {});
        const bool running = started.tryAcquire(1, 3000);
        delete deleted;
        resume.release();
        require(QThreadPool::globalInstance()->waitForDone(3000), "shutdown worker stalled");
        QCoreApplication::processEvents();
        require(running && stale == 0, "destroyed queue executed pending work or callback");
        InstrumentOperationQueue closed;
        closed.submit([&] { started.release(); resume.acquire(); return InstrumentOperationResult{}; },
                      [&](const auto&) { ++stale; });
        const bool closingActive = started.tryAcquire(1, 3000);
        closed.submit([&] { ++stale; return InstrumentOperationResult{}; }, {});
        closed.close();
        closed.close();
        closed.submit([&] { ++stale; return InstrumentOperationResult{}; }, {});
        resume.release();
        require(QThreadPool::globalInstance()->waitForDone(3000), "closed worker stalled");
        QCoreApplication::processEvents();
        require(closingActive && stale == 0, "closed queue accepted or delivered work");
        auto* deletedInCompletion = new InstrumentOperationQueue;
        QPointer<InstrumentOperationQueue> callbackAlive(deletedInCompletion);
        deletedInCompletion->submit([] { return InstrumentOperationResult{true, {}}; },
                                   [deletedInCompletion](const auto&) { delete deletedInCompletion; });
        deletedInCompletion->submit([&] { ++stale; return InstrumentOperationResult{}; }, {});
        require(QThreadPool::globalInstance()->waitForDone(3000), "completion-delete worker stalled");
        QCoreApplication::processEvents();
        require(callbackAlive.isNull() && stale == 0, "completion deletion continued pending work");
        std::cout << "PASS: FIFO operations, exception recovery and owner shutdown\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
