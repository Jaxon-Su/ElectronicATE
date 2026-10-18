#include "capturelease.h"
#include "oscilloscopemanager.h"
#include "oscilloscope.h"
#include "pngcapturecommand.h"
#include <QCoreApplication>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <atomic>
#include <thread>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class SessionScope : public Oscilloscope {
public:
    QString model() const override { return "Offline"; }
    QString vendor() const override { return "Test"; }
    bool isConnected() const override { return connected; }
    void disconnect() override { ++disconnects; connected = false; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return {}; }
    bool connected = true;
    int disconnects = 0;
};

struct TransferState {
    QSemaphore started, resume;
    std::atomic<bool> transferring{false};
    std::atomic<bool> overlapped{false};
    std::atomic<int> disconnects{0};
};

class BlockingScope : public SessionScope {
public:
    explicit BlockingScope(std::shared_ptr<TransferState> state) : state(std::move(state)) {}
    QByteArray captureScreenshot(const QString&, const QString&) override {
        state->transferring = true;
        state->started.release();
        state->resume.acquire();
        state->transferring = false;
        return "offline image";
    }
    void disconnect() override {
        state->overlapped = state->transferring.load();
        ++state->disconnects;
        SessionScope::disconnect();
    }
    std::shared_ptr<TransferState> state;
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        auto session = std::make_shared<CaptureSession>();
        auto lease = CaptureLease::tryAcquire(session);
        auto workerLease = lease;
        auto scope = std::make_shared<SessionScope>();
        OscilloscopeManager manager;
        manager.assign({{"scope", scope}});
        auto scopes = manager.takeAll();
        require(manager.isEmpty() && !manager.current() && scope->disconnects == 0,
                "ownership transfer disconnected active scope");
        int cleanups = 0;
        session->closeWhenIdle([scopes = std::move(scopes), &cleanups]() mutable {
            ++cleanups;
            OscilloscopeManager::disconnectAll(scopes);
        });
        require(!CaptureLease::tryAcquire(session) && scope->disconnects == 0,
                "closing admitted capture or disconnected before drain");
        lease.reset();
        require(cleanups == 0, "cleanup ran before final lease owner exited");
        session.reset(); // mirrors deletion of the ViewModel owning the session
        std::thread worker([lease = std::move(workerLease)]() mutable { lease.reset(); });
        worker.join();
        require(cleanups == 1 && scope->disconnects == 1, "deferred cleanup did not run once");

        auto idle = std::make_shared<CaptureSession>();
        idle->closeWhenIdle([&] { ++cleanups; });
        idle->closeWhenIdle([&] { ++cleanups; });
        require(cleanups == 2 && !CaptureLease::tryAcquire(idle), "idle close was not permanent/idempotent");

        // Exercise both acquisition/closure orderings without sleeps or hardware.
        for (int i = 0; i < 100; ++i) {
            auto raced = std::make_shared<CaptureSession>();
            std::atomic<int> cleanupCount{0};
            std::thread acquire([raced] { auto owned = CaptureLease::tryAcquire(raced); });
            raced->closeWhenIdle([&] { ++cleanupCount; });
            acquire.join();
            require(cleanupCount == 1 && !CaptureLease::tryAcquire(raced), "close/acquire race lost cleanup");
        }
        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory failed");
        auto state = std::make_shared<TransferState>();
        auto activeScope = std::make_shared<BlockingScope>(state);
        manager.assign({{"active", activeScope}});
        CaptureContext context;
        context.oscilloscope = activeScope;
        context.captureSession = std::make_shared<CaptureSession>();
        context.selectSaveFile = [&](const QString&, const QString&, const QString&) {
            return dir.filePath("image.png");
        };
        PngCaptureCommand(context).execute();
        const bool started = state->started.tryAcquire(1, 3000);
        auto activeMap = manager.takeAll();
        context.captureSession->closeWhenIdle([owned = std::move(activeMap)]() mutable {
            OscilloscopeManager::disconnectAll(owned);
        });
        const bool stillConnected = state->disconnects == 0;
        context.oscilloscope.reset();
        context.captureSession.reset();
        activeScope.reset();
        state->resume.release();
        const bool finished = QThreadPool::globalInstance()->waitForDone(3000);
        QCoreApplication::processEvents();
        require(started && finished && stillConnected && state->disconnects == 1 && !state->overlapped,
                "command shutdown disconnected during transfer or lost deferred cleanup");
        std::cout << "PASS: capture drain, scope ownership, idle close and acquisition race\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
