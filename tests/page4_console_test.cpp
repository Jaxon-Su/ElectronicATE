#include "page4viewmodel.h"
#include "page4model.h"
#include "icommunication.h"
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <QPointer>
#include <atomic>
#include <QSemaphore>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void waitUntil(const std::function<bool()>& done)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 4000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(done(), "asynchronous console timeout");
}
void settle(Page4ViewModel& vm) { waitUntil([&] { return !vm.isOperationPending(); }); }

struct CommState {
    QSemaphore* readEntered = nullptr;
    QSemaphore* releaseRead = nullptr;
    QThread* ownerThread = nullptr;
    std::atomic<bool> wrongThread{false};
    bool open = false;
    bool openSuccess = true;
    int throwOpen = 0;
    int written = -2; // -2 means complete write
    int closes = 0;
    std::atomic<int> destroyed{0};
    bool throwClose = false;
    int writes = 0;
    int reads = 0;
    int readDelayMs = 0;
    bool readFails = false;
    bool throwRead = false;
    bool throwWrite = false;
    QByteArray sent;
    QByteArray response = "offline reply\n";
};

class ConsoleCommunication : public ICommunication {
public:
    explicit ConsoleCommunication(std::shared_ptr<CommState> state) : state(std::move(state)) { this->state->ownerThread = QThread::currentThread(); }
    ~ConsoleCommunication() override { if (state->ownerThread != QThread::currentThread()) state->wrongThread = true; ++state->destroyed; }
    bool open() override {
        if (state->throwOpen == 1) throw std::runtime_error("open exception");
        if (state->throwOpen == 2) throw 42;
        state->open = state->openSuccess;
        return state->open;
    }
    void close() override {
        ++state->closes;
        if (state->throwClose) throw std::runtime_error("close exception");
        state->open = false;
    }
    bool isOpen() const override { return state->open; }
    QString lastError() const override { return "offline write error"; }
    int write(const QByteArray& data) override {
        if (state->throwWrite) throw std::runtime_error("write exception");
        ++state->writes;
        state->sent = data;
        return state->written == -2 ? data.size() : state->written;
    }
    int read(QByteArray& data, int) override {
        if (state->throwRead) throw 42;
        ++state->reads;
        if (state->ownerThread != QThread::currentThread()) state->wrongThread = true;
        if (state->readEntered) state->readEntered->release();
        if (state->releaseRead) state->releaseRead->acquire();
        if (state->readDelayMs) QThread::msleep(state->readDelayMs);
        if (state->readFails) return -1;
        data = state->response;
        return data.size();
    }
    std::shared_ptr<CommState> state;
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        auto state = std::make_shared<CommState>();
        Page4Model model;
        QString factoryAddress;
        Page4ViewModel vm(&model, [&](const QString& address) {
            factoryAddress = address;
            return std::make_unique<ConsoleCommunication>(state);
        });
        int errors = 0;
        QString response;
        QObject::connect(&vm, &Page4ViewModel::errorOccurred, &app, [&](const QString&) { ++errors; });
        QObject::connect(&vm, &Page4ViewModel::responseReceived, &app,
                         [&](const QString&, const QString& value) { response = value; });
        vm.connectToAddress("  offline-address  ");
        settle(vm);
        require(vm.isConnected() && factoryAddress == "offline-address", "injected creator/trim failed");
        vm.sendCommand(" *CLS ");
        settle(vm);
        require(state->sent == "*CLS\n" && model.fullHistory().first().success, "successful command changed");
        vm.sendIDN();
        settle(vm);
        require(state->sent == "*IDN?\n" && response == "offline reply" && model.fullHistory().first().success,
                "query framing/response changed");
        state->written = -1;
        vm.sendCommand("*RST");
        settle(vm);
        require(errors == 1 && !model.fullHistory().first().success, "failed non-query recorded as success");
        state->written = 1;
        vm.sendCommand("*CLS");
        settle(vm);
        require(errors == 2 && !model.fullHistory().first().success, "partial command recorded as success");
        state->written = -1;
        vm.sendIDN();
        settle(vm);
        require(errors == 3 && !model.fullHistory().first().success, "failed query recorded as success");
        vm.disconnect();
        settle(vm);
        require(!vm.isConnected() && state->closes == 1, "disconnect ownership changed");
        state->openSuccess = false;
        vm.connectToAddress("offline-address");
        settle(vm);
        require(model.connectionStatus() == ConnectionStatus::Error && !vm.isConnected(), "failed open accepted");
        state->openSuccess = true;
        state->written = -2;
        state->response.clear();
        vm.connectToAddress("offline-address");
        settle(vm);
        vm.setTimeout(100);
        QTimer::singleShot(0, &vm, [&] { vm.disconnect(); });
        vm.sendIDN();
        settle(vm);
        require(!model.fullHistory().first().success && !vm.isConnected(), "query survived disconnected transport");
        vm.connectToAddress("offline-address");
        settle(vm);
        QTimer::singleShot(0, &vm, [&] { vm.connectToAddress("replacement-address"); });
        vm.sendIDN();
        settle(vm);
        require(!model.fullHistory().first().success && vm.isConnected(), "query continued on replacement transport");
        const int writesBefore = state->writes;
        QTimer::singleShot(0, &vm, [&] { vm.sendCommand("*CLS"); });
        vm.sendIDN();
        settle(vm);
        require(state->writes == writesBefore + 1, "nested command interleaved with query");
        state->response = "retry works\n";
        vm.sendIDN();
        settle(vm);
        require(model.fullHistory().first().success, "query guard was not released after failure");

        state->readFails = true;
        const int beforeReadFailure = state->reads;
        vm.sendIDN();
        settle(vm);
        require(!model.fullHistory().first().success && state->reads == beforeReadFailure + 1,
                "read failure was retried until timeout");
        state->readFails = false;
        state->response.clear();
        state->readDelayMs = 60;
        const int beforeSlowRead = state->reads;
        vm.sendIDN();
        settle(vm);
        require(!model.fullHistory().first().success && state->reads - beforeSlowRead <= 2,
                "driver read time was excluded from timeout");
        state->readDelayMs = 0;
        state->throwWrite = true;
        vm.sendCommand("*CLS");
        settle(vm);
        require(!model.fullHistory().first().success, "write exception recorded as success");
        state->throwWrite = false;
        state->throwRead = true;
        vm.sendIDN();
        settle(vm);
        require(!model.fullHistory().first().success, "read exception escaped or recorded success");
        state->throwRead = false;
        state->response = "after exception\n";
        vm.sendIDN();
        settle(vm);
        require(model.fullHistory().first().success, "exception stranded command guard");

        state->response.clear();
        auto* deleted = new Page4ViewModel(&model, [&](const QString&) {
            return std::make_unique<ConsoleCommunication>(state);
        });
        deleted->connectToAddress("temporary-address");
        settle(*deleted);
        const int historyBefore = model.fullHistory().size();
        QPointer<Page4ViewModel> deletedGuard(deleted);
        const int destroyedBeforeDelete = state->destroyed.load();
        QTimer::singleShot(0, deleted, [deleted] { delete deleted; });
        deleted->sendIDN();
        waitUntil([&] { return !deletedGuard; });
        waitUntil([&] { return state->destroyed.load() > destroyedBeforeDelete; });
        require(model.fullHistory().size() == historyBefore, "deleted query owner wrote a completion record");
        auto opening = std::make_shared<CommState>();
        Page4Model openingModel;
        int factoryFailure = 0;
        Page4ViewModel openingVm(&openingModel, [&](const QString&) {
            if (factoryFailure == 1) throw std::runtime_error("factory exception");
            if (factoryFailure == 2) throw 42;
            return std::make_unique<ConsoleCommunication>(opening);
        });
        for (int mode : {1, 2}) {
            factoryFailure = mode;
            openingVm.connectToAddress("factory-failure");
            settle(openingVm);
            require(openingModel.connectionStatus() == ConnectionStatus::Error && !openingVm.isConnected(),
                    "factory exception stranded Connecting status");
            factoryFailure = 0;
            opening->throwOpen = mode;
            const int destroyedBefore = opening->destroyed;
            openingVm.connectToAddress("open-failure");
            settle(openingVm);
            require(openingModel.connectionStatus() == ConnectionStatus::Error && !openingVm.isConnected()
                        && opening->destroyed == destroyedBefore + 1,
                    "open exception retained failed transport");
            opening->throwOpen = 0;
            openingVm.connectToAddress("recovered");
            settle(openingVm);
            require(openingVm.isConnected(), "connection could not recover after exception");
            openingVm.disconnect();
            settle(openingVm);
        }
        auto failingClose = std::make_shared<CommState>();
        {
            Page4ViewModel cleanup(&model, [&](const QString&) {
                return std::make_unique<ConsoleCommunication>(failingClose);
            });
            cleanup.connectToAddress("close-test");
            settle(cleanup);
            failingClose->throwClose = true;
            cleanup.disconnect();
            settle(cleanup);
            require(!cleanup.isConnected() && failingClose->destroyed == 1,
                    "failed close retained transport ownership");
            cleanup.connectToAddress("destructor-test");
            settle(cleanup);
        }
        waitUntil([&] { return failingClose->destroyed.load() == 2; });
        require(failingClose->destroyed == 2, "destructor did not release throwing transport");
        {
            Page4Model lockModel;
            auto lockState = std::make_shared<CommState>();
            Page4ViewModel lockVm(&lockModel, [lockState](const QString&) {
                return std::make_unique<ConsoleCommunication>(lockState);
            });
            lockVm.setControlAllowed(false);
            lockVm.connectToAddress("offline");
            settle(lockVm);
            require(!lockVm.isControlActive() && !lockVm.isConnected(), "disabled console connected");
            lockVm.setControlAllowed(true);
            lockVm.connectToAddress("offline");
            settle(lockVm);
            require(lockVm.isControlActive(), "connected console did not own control");
            QObject::connect(&lockVm, &Page4ViewModel::commandSent, [&] {
                lockVm.disconnect();
                require(lockVm.isControlActive(), "disconnect unlocked an unfinished command");
            });
            lockVm.sendCommand("*CLS");
            settle(lockVm);
            require(!lockVm.isControlActive(), "finished disconnected console stayed locked");
            lockState->openSuccess = false;
            lockVm.connectToAddress("offline");
            settle(lockVm);
            require(!lockVm.isControlActive(), "failed connection stayed locked");
        }
        {
            Page4Model slowModel;
            auto slow = std::make_shared<CommState>();
            QSemaphore entered, resume;
            slow->readEntered = &entered;
            slow->releaseRead = &resume;
            Page4ViewModel slowVm(&slowModel, [slow](const QString&) { return std::make_unique<ConsoleCommunication>(slow); });
            slowVm.connectToAddress("blocked-driver");
            settle(slowVm);
            slowVm.sendIDN();
            const bool workerStarted = entered.tryAcquire(1, 3000);
            bool uiResponsive = false;
            QTimer::singleShot(0, &app, [&] { uiResponsive = true; });
            QCoreApplication::processEvents();
            slowVm.disconnect();
            const bool lockHeld = slowVm.isControlActive();
            resume.release();
            settle(slowVm);
            require(workerStarted && uiResponsive && lockHeld && !slowVm.isControlActive(), "blocking read froze UI or released lock early");
            require(!slowModel.fullHistory().first().success && !slow->wrongThread, "cancel accepted stale result or transport changed threads");
        }
        std::cout << "PASS: injected console communication, framing and failed write history\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
