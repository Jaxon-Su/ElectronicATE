#include "page3viewmodel.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <QSemaphore>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void finishWork()
{
    require(QThreadPool::globalInstance()->waitForDone(3000), "worker timeout");
    QCoreApplication::processEvents();
}

class PendingScope : public Oscilloscope {
public:
    QString model() const override { return "pending"; }
    QString vendor() const override { return "offline"; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return capture ? capture() : QByteArray{}; }
    std::function<QByteArray()> capture;
    bool isConnected() const override { return connected; }
    void disconnect() override { connected = false; ++disconnects; }
    bool connected = true;
    int disconnects = 0;
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        Page3Model model;
        Page3Operations operations;
        QStringList calls;
        operations.connectScopes = [](const Page1Config&) { return OscilloscopeManager::OscMap{}; };
        operations.input = [&](const Page1Config&, const InputRow& row, InputAction action) {
            calls << (action == InputAction::PowerOn ? "on:" : "off:") + row.vin;
            return InstrumentOperationResult{true, {}};
        };
        operations.relay = [&](const Page1Config&, const QVector<RelayDataRow>&, int, RelayAction) {
            calls << "relay";
            return InstrumentOperationResult{true, {}};
        };
        bool failLoad = true;
        operations.load = [&](const Page1Config&, const QVector<LoadDataRow>&, int, const LoadMetaRow&, LoadAction, bool) {
            calls << "load";
            if (failLoad) throw std::runtime_error("offline load error");
            return InstrumentOperationResult{true, {}};
        };
        Page3ViewModel vm(&model, operations);
        Page1Config config;
        for (const QString& type : QStringList{"InputSource", "Relay", "Load"}) {
            InstrumentConfig instrument;
            instrument.type = type;
            config.instruments.append(instrument);
        }
        vm.onPage1ConfigChanged(config);
        require(QMetaObject::invokeMethod(&vm, "applyPendingConfig", Qt::DirectConnection), "config slot missing");
        finishWork();
        TestConditionSnapshot conditions;
        conditions.inputRows = {{"1phase", "110", "60", "0"}};
        conditions.loadMeta.vo = {"12"};
        conditions.loadRows = {{"load", {"2"}}};
        conditions.relayRows = {{"relay", {"on"}}};
        vm.onConditionsChanged(conditions);
        require(model.getLoadMetaData().vo == QVector<QString>{"12"} && model.getRelayRowsData().size() == 1,
                "condition snapshot did not reach model");
        vm.onSelected(TableKind::Input, 0, "input");
        vm.onSelected(TableKind::Relay, 0, "relay");
        vm.onSelected(TableKind::Load, 0, "load");
        vm.handleInput(InputAction::PowerOn);
        vm.handleRelay(RelayAction::RelayOn);
        vm.handleInput(InputAction::PowerOff);
        conditions.inputRows[0].vin = "230";
        vm.onConditionsChanged(conditions);
        for (int i = 0; i < 3; ++i) finishWork();
        require(calls == QStringList{"on:110", "relay", "off:110"}, "FIFO or captured input changed");
        bool busy = false;
        int forcedOff = 0;
        QObject::connect(&vm, &Page3ViewModel::loadOperationBusyChanged, [&](bool value) { busy = value; });
        QObject::connect(&vm, &Page3ViewModel::forceOff, [&](TableKind kind) { if (kind == TableKind::Load) ++forcedOff; });
        vm.handleLoad(LoadAction::LoadOn);
        vm.handleLoad(LoadAction::Change);
        require(busy, "load did not become busy");
        finishWork();
        require(!busy && forcedOff == 1 && calls.count("load") == 1, "load failure stranded busy state or overlapped");
        failLoad = false;
        vm.handleLoad(LoadAction::LoadOn);
        finishWork();
        require(!busy && calls.count("load") == 2, "load could not retry");
        auto* deletedOnConfig = new Page3ViewModel(&model, operations);
        QPointer<Page3ViewModel> configAlive(deletedOnConfig);
        QObject::connect(deletedOnConfig, &Page3ViewModel::page1ConfigChanged,
                         [deletedOnConfig](const auto&) { delete deletedOnConfig; });
        deletedOnConfig->onPage1ConfigChanged(config);
        QMetaObject::invokeMethod(deletedOnConfig, "applyPendingConfig", Qt::DirectConnection);
        require(configAlive.isNull(), "config callback did not delete owner");
        auto* deletedOnBusy = new Page3ViewModel(&model, operations);
        deletedOnBusy->onPage1ConfigChanged(config);
        QMetaObject::invokeMethod(deletedOnBusy, "applyPendingConfig", Qt::DirectConnection);
        finishWork();
        deletedOnBusy->onConditionsChanged(conditions);
        deletedOnBusy->onSelected(TableKind::Load, 0, "load");
        QPointer<Page3ViewModel> busyAlive(deletedOnBusy);
        QObject::connect(deletedOnBusy, &Page3ViewModel::loadOperationBusyChanged,
                         [deletedOnBusy](bool) { delete deletedOnBusy; });
        deletedOnBusy->handleLoad(LoadAction::LoadOn);
        require(busyAlive.isNull() && calls.count("load") == 2, "deleted owner submitted a load");
        QSemaphore connecting, resume;
        auto pendingScope = std::make_shared<PendingScope>();
        operations.connectScopes = [&](const Page1Config&) {
            connecting.release();
            resume.acquire();
            return OscilloscopeManager::OscMap{{"pending", pendingScope}};
        };
        auto* deletedWhileConnecting = new Page3ViewModel(&model, operations);
        deletedWhileConnecting->onPage1ConfigChanged(config);
        QMetaObject::invokeMethod(deletedWhileConnecting, "applyPendingConfig", Qt::DirectConnection);
        const bool connectingStarted = connecting.tryAcquire(1, 3000);
        delete deletedWhileConnecting;
        resume.release();
        finishWork();
        require(connectingStarted && pendingScope->disconnects == 1,
                "unadopted connection survived deleted ViewModel");
        auto adoptedScope = std::make_shared<PendingScope>();
        operations.connectScopes = [adoptedScope](const Page1Config&) {
            return OscilloscopeManager::OscMap{{"pending", adoptedScope}};
        };
        auto* adopted = new Page3ViewModel(&model, operations);
        adopted->onPage1ConfigChanged(config);
        QMetaObject::invokeMethod(adopted, "applyPendingConfig", Qt::DirectConnection);
        finishWork();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        require(adoptedScope->disconnects == 0, "adopted scope closed with future result");
        delete adopted;
        require(adoptedScope->disconnects == 1, "adopted scope not closed by owner");
        int connectionAttempts = 0;
        operations.connectScopes = [&](const Page1Config&) -> OscilloscopeManager::OscMap {
            if (++connectionAttempts == 1) throw std::runtime_error("expected connect failure");
            return {};
        };
        Page3ViewModel retryConfig(&model, operations);
        for (int attempt = 0; attempt < 2; ++attempt) {
            retryConfig.onPage1ConfigChanged(config);
            QMetaObject::invokeMethod(&retryConfig, "applyPendingConfig", Qt::DirectConnection);
            finishWork();
        }
        require(connectionAttempts == 2, "failed connection stranded pending-config runner");
        // Control ownership spans the whole FIFO and successful output ON state.
        {
            Page3Model lockModel;
            Page3Operations lockOps;
            int commands = 0, configurations = 0;
            bool failOff = false;
            bool restoredOn = false;
            lockOps.connectScopes = [&](const Page1Config&) {
                ++configurations;
                return OscilloscopeManager::OscMap{};
            };
            lockOps.input = [&](const Page1Config&, const InputRow&, InputAction action) {
                ++commands;
                return InstrumentOperationResult{!(failOff && action == InputAction::PowerOff), {}};
            };
            Page3ViewModel lockVm(&lockModel, lockOps);
            lockVm.setControlAllowed(false);
            lockVm.onPage1ConfigChanged(config);
            QMetaObject::invokeMethod(&lockVm, "applyPendingConfig", Qt::DirectConnection);
            require(!lockVm.isControlActive() && configurations == 0, "disabled page reconnected instruments");
            lockVm.setControlAllowed(true);
            QMetaObject::invokeMethod(&lockVm, "applyPendingConfig", Qt::DirectConnection);
            require(lockVm.isControlActive(), "scope connection did not lock other pages");
            finishWork();
            require(!lockVm.isControlActive() && configurations == 1, "connection completion kept lock");
            lockVm.onConditionsChanged(conditions);
            lockVm.onSelected(TableKind::Input, 0, "input");
            QObject::connect(&lockVm, &Page3ViewModel::restoreOutputState, [&](TableKind, bool on) { restoredOn = on; });
            QVector<bool> activity;
            QObject::connect(&lockVm, &Page3ViewModel::controlActiveChanged, [&](bool on) { activity.append(on); });
            lockVm.handleInput(InputAction::PowerOn);
            require(lockVm.isControlActive(), "accepted command did not lock immediately");
            finishWork();
            require(lockVm.isControlActive(), "output ON released ownership");
            failOff = true;
            lockVm.handleInput(InputAction::PowerOff);
            finishWork();
            require(lockVm.isControlActive() && restoredOn, "failed output OFF released ownership or lost retry button");
            failOff = false;
            lockVm.handleInput(InputAction::PowerOff);
            finishWork();
            require(!lockVm.isControlActive() && activity == QVector<bool>{true, false}, "output ownership flickered or stayed locked");
            lockVm.setControlAllowed(false);
            lockVm.handleInput(InputAction::PowerOn);
            finishWork();
            require(commands == 3 && !lockVm.isControlActive(), "disabled page accepted command");
        }
        {
            Page3Model captureModel;
            Page3Operations captureOps;
            auto scope = std::make_shared<PendingScope>();
            captureOps.connectScopes = [scope](const Page1Config&) {
                return OscilloscopeManager::OscMap{{"pending", scope}};
            };
            Page3ViewModel captureVm(&captureModel, captureOps);
            captureVm.onPage1ConfigChanged(config);
            QMetaObject::invokeMethod(&captureVm, "applyPendingConfig", Qt::DirectConnection);
            finishWork();
            bool selected = false;
            captureVm.setCaptureFileSelector([&](const QString&, const QString&, const QString&) {
                selected = captureVm.isControlActive();
                return QString{};
            });
            captureVm.OnWaveformCaptured();
            require(selected && !captureVm.isControlActive(), "capture cancellation stranded lock");
            QTemporaryDir directory;
            QSemaphore capturing, releaseCapture;
            scope->capture = [&] {
                capturing.release();
                releaseCapture.acquire();
                return QByteArray{};
            };
            captureVm.setCaptureFileSelector([&](const QString&, const QString&, const QString&) {
                return directory.filePath("capture.png");
            });
            captureVm.OnWaveformCaptured();
            const bool started = capturing.tryAcquire(1, 3000);
            const bool held = captureVm.isControlActive();
            releaseCapture.release();
            finishWork();
            QEventLoop settle;
            QTimer::singleShot(100, &settle, &QEventLoop::quit);
            settle.exec();
            require(started && held && !captureVm.isControlActive(), "background capture lock lifetime incorrect");
        }
        std::cout << "PASS: Page3 injected operations, snapshots, FIFO and load recovery\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
