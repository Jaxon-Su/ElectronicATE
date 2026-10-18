#include "../support/configuration_wait.h"
#include "oscmeasurementresult.h"
#include "../../src/service/measurement/measurementformat.h"
#include "manualcontrolviewmodel.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <QSemaphore>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <atomic>
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

class BorrowController : public ITriggerController {
public:
    bool suspended = false, busy = false;
    Oscilloscope* scope = nullptr;
    void setInstrument(Oscilloscope* value) override { scope = value; }
    Oscilloscope* getInstrument() const override { return scope; }
    QString getSupportedModel() const override { return "MSOSeries456"; }
    bool isOperationActive() const override { return busy; }
    void setSuspended(bool value) override { suspended = value; }
};

class PendingScope : public Oscilloscope {
public:
    QString model() const override { return "pending"; }
    QString vendor() const override { return "offline"; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return capture ? capture() : QByteArray{}; }
    std::function<QByteArray()> capture;
    bool isConnected() const override { return connected; }
    void disconnect() override { connected = false; ++disconnects; }
    std::atomic<bool> connected{true};
    std::atomic<int> disconnects{0};
};

void awaitDisconnect(const std::shared_ptr<PendingScope>& scope)
{
    QElapsedTimer deadline;
    deadline.start();
    while (scope->disconnects == 0 && deadline.elapsed() < 3000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(scope->disconnects == 1, "scope background cleanup did not finish exactly once");
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        {
            ManualControlModel syncModel;
            ManualControlOperations syncOps;
            ManualControlViewModel syncVm(&syncModel, syncOps);
            const auto initial = syncVm.loadSyncSettings();
            require(initial.enabled && !initial.dirty, "default sync snapshot differs from ManualControl");
            syncVm.onSyncChanged(false);
            const auto disabled = syncVm.loadSyncSettings();
            require(!disabled.enabled && disabled.dirty, "disabled sync snapshot lost reconfiguration flag");
            syncVm.onSyncChanged(true);
            require(syncVm.loadSyncSettings().enabled, "sync snapshot did not follow ManualControl setting");
            require(!disabled.enabled && disabled.dirty, "captured settings changed after capture");
        }
        OscChannelMeasure unavailable;
        require(formatOscMeasurement(unavailable.maxVal) == "NA", "missing max must be NA");
        require(formatOscMeasurement(unavailable.rms) == "NA", "unmeasured RMS must be NA");
        require(formatOscMeasurement(unavailable.mean) == "NA", "unmeasured mean must be NA");
        require(formatOscMeasurement(0.0) == "0", "valid zero must remain numeric");
        require(formatOscMeasurement(std::numeric_limits<double>::infinity()) == "NA", "invalid value must be NA");
        {
            ManualControlModel removalModel;
            ManualControlOperations removalOps;
            QSemaphore entered, release;
            auto staleScope = std::make_shared<PendingScope>();
            int attempts = 0;
            removalOps.connectScopes = [&](const InstrumentSetupConfig& cfg) {
                ++attempts;
                if (!cfg.instruments[0].modelName.isEmpty()) {
                    entered.release(); release.acquire();
                    return OscilloscopeManager::OscMap{{"MSO44B", staleScope}};
                }
                return OscilloscopeManager::OscMap{};
            };
            ManualControlViewModel removalVm(&removalModel, removalOps);
            InstrumentSetupConfig cfg;
            InstrumentConfig scopeConfig;
            scopeConfig.type = "Oscilloscope"; scopeConfig.enabled = true; scopeConfig.modelName = "MSO44B";
            cfg.instruments.append(scopeConfig);
            removalVm.onInstrumentSetupConfigChanged(cfg);
            startQueuedConfiguration(&removalVm);
            const bool connecting = entered.tryAcquire(1, 2000);
            if (!connecting) release.release();
            require(connecting, "scope did not start connecting");
            bool removed = false;
            QObject::connect(&removalVm, &ManualControlViewModel::instrumentSetupConfigChanged,
                             [&](const InstrumentSetupConfig& updated) { removed = updated.instruments[0].modelName.isEmpty(); });
            cfg.instruments[0].modelName.clear();
            removalVm.onInstrumentSetupConfigChanged(cfg);
            QMetaObject::invokeMethod(&removalVm, "reconnectOscilloscopes", Qt::DirectConnection);
            release.release();
            require(removed, "cleared scope panel waited for failed connection");
            finishWork();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            awaitDisconnect(staleScope);
            require(staleScope->disconnects == 1, "stale scope was adopted instead of disconnected");
            startQueuedConfiguration(&removalVm);
            finishWork();
            QMetaObject::invokeMethod(&removalVm, "reconnectOscilloscopes", Qt::DirectConnection);
            startQueuedConfiguration(&removalVm);
            finishWork();
            require(removed && attempts == 2 && !removalVm.isConfigurationBusy(), "reconnect resurrected removed scope");
        }
        {
            ManualControlModel model;
            ManualControlOperations ops;
            auto scope = std::make_shared<PendingScope>();
            ops.connectScopes = [scope](const InstrumentSetupConfig&) {
                return OscilloscopeManager::OscMap{{"MSO44B", scope}};
            };
            auto vm = std::make_unique<ManualControlViewModel>(&model, ops);
            InstrumentSetupConfig cfg;
            InstrumentConfig instrument;
            instrument.type = "Oscilloscope";
            instrument.enabled = true;
            instrument.modelName = "MSO44B";
            cfg.instruments.append(instrument);
            vm->onInstrumentSetupConfigChanged(cfg);
            startQueuedConfiguration(vm.get());
            finishWork();
            BorrowController controller;
            vm->onTriggerWidgetCreated("MSO44B", &controller);
            controller.busy = true;
            require(!vm->borrowOscilloscope(), "borrow raced manual worker");
            controller.busy = false;
            auto borrowed = vm->borrowOscilloscope();
            require(borrowed.get() == scope.get(), "borrow failed");
            require(controller.suspended, "borrow did not suspend controller");
            borrowed.reset();
            QEventLoop resume;
            QTimer::singleShot(100, &resume, &QEventLoop::quit); resume.exec();
            require(!controller.suspended, "returned lease did not resume controller");
            borrowed = vm->borrowOscilloscope();
            require(controller.suspended, "second borrow did not suspend controller");
            require(!vm->borrowOscilloscope(), "duplicate borrow accepted");
            cfg.instruments[0].modelName.clear();
            vm->onInstrumentSetupConfigChanged(cfg);
            startQueuedConfiguration(vm.get());
            require(scope->disconnects == 0, "config disconnected borrowed scope");
            vm.reset();
            require(scope->disconnects == 0, "destruction disconnected borrowed scope");
            borrowed.reset();
            awaitDisconnect(scope);
            require(scope->disconnects == 1, "return did not clean up scope");
        }
        ManualControlModel model;
        ManualControlOperations operations;
        QStringList calls;
        operations.connectScopes = [](const InstrumentSetupConfig&) { return OscilloscopeManager::OscMap{}; };
        operations.input = [&](const InstrumentSetupConfig&, const InputRow& row, InputAction action) {
            calls << (action == InputAction::PowerOn ? "on:" : "off:") + row.vin;
            return InstrumentOperationResult{true, {}};
        };
        operations.relay = [&](const InstrumentSetupConfig&, const QVector<RelayDataRow>&, int, RelayAction) {
            calls << "relay";
            return InstrumentOperationResult{true, {}};
        };
        bool failLoad = true;
        operations.load = [&](const InstrumentSetupConfig&, const QVector<LoadDataRow>&, int, const LoadMetaRow&, LoadAction, bool) {
            calls << "load";
            if (failLoad) throw std::runtime_error("offline load error");
            return InstrumentOperationResult{true, {}};
        };
        ManualControlViewModel vm(&model, operations);
        {
            TestConditionSnapshot dc;
            dc.dcRows = {{"24", "5", ""}};
            dc.dcRows2 = {{"12", "3", "aux"}};
            dc.dcRows3 = {{"5", "1", "logic"}};
            QStringList dcTitles[3];
            int selected[3] = {-1, -1, -1};
            auto connection = QObject::connect(&vm, &ManualControlViewModel::dcInputUpdated,
                [&](int source, const QStringList& titles, int index) { dcTitles[source] = titles; selected[source] = index; });
            vm.onConditionsChanged(dc);
            vm.onDcSelected(0, 0, "aux / 3 A");
            QString saved;
            QXmlStreamWriter writer(&saved);
            vm.writeXml(writer);
            vm.onDcSelected(0, -1, "");
            QXmlStreamReader reader(saved);
            reader.readNextStartElement();
            vm.loadXml(reader);
            vm.publishXmlLoaded();
            require(!reader.hasError() && dcTitles[0] == QStringList{"24 V / 5 A"}
                        && dcTitles[2] == QStringList{"logic / 1 A"} && selected[0] == 0
                       , "DC titles or selections failed through ViewModel XML load");
            QObject::disconnect(connection);
        }
        InstrumentSetupConfig config;
        for (const QString& type : QStringList{"InputSource", "Relay", "Load"}) {
            InstrumentConfig instrument;
            instrument.type = type;
            config.instruments.append(instrument);
        }
        vm.onInstrumentSetupConfigChanged(config);
        startQueuedConfiguration(&vm);
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
        QObject::connect(&vm, &ManualControlViewModel::loadOperationBusyChanged, [&](bool value) { busy = value; });
        QObject::connect(&vm, &ManualControlViewModel::forceOff, [&](TableKind kind) { if (kind == TableKind::Load) ++forcedOff; });
        vm.handleLoad(LoadAction::LoadOn);
        vm.handleLoad(LoadAction::Change);
        require(busy, "load did not become busy");
        finishWork();
        require(!busy && forcedOff == 1 && calls.count("load") == 1, "load failure stranded busy state or overlapped");
        failLoad = false;
        vm.handleLoad(LoadAction::LoadOn);
        finishWork();
        require(!busy && calls.count("load") == 2, "load could not retry");
        auto* deletedOnConfig = new ManualControlViewModel(&model, operations);
        QPointer<ManualControlViewModel> configAlive(deletedOnConfig);
        QObject::connect(deletedOnConfig, &ManualControlViewModel::instrumentSetupConfigChanged,
                         [deletedOnConfig](const auto&) { delete deletedOnConfig; });
        deletedOnConfig->onInstrumentSetupConfigChanged(config);
        startQueuedConfiguration(deletedOnConfig);
        require(configAlive.isNull(), "config callback did not delete owner");
        auto* deletedOnBusy = new ManualControlViewModel(&model, operations);
        deletedOnBusy->onInstrumentSetupConfigChanged(config);
        startQueuedConfiguration(deletedOnBusy);
        finishWork();
        deletedOnBusy->onConditionsChanged(conditions);
        deletedOnBusy->onSelected(TableKind::Load, 0, "load");
        QPointer<ManualControlViewModel> busyAlive(deletedOnBusy);
        QObject::connect(deletedOnBusy, &ManualControlViewModel::loadOperationBusyChanged,
                         [deletedOnBusy](bool) { delete deletedOnBusy; });
        deletedOnBusy->handleLoad(LoadAction::LoadOn);
        require(busyAlive.isNull() && calls.count("load") == 2, "deleted owner submitted a load");
        QSemaphore connecting, resume;
        auto pendingScope = std::make_shared<PendingScope>();
        operations.connectScopes = [&](const InstrumentSetupConfig&) {
            connecting.release();
            resume.acquire();
            return OscilloscopeManager::OscMap{{"pending", pendingScope}};
        };
        auto* deletedWhileConnecting = new ManualControlViewModel(&model, operations);
        deletedWhileConnecting->onInstrumentSetupConfigChanged(config);
        startQueuedConfiguration(deletedWhileConnecting);
        const bool connectingStarted = connecting.tryAcquire(1, 3000);
        delete deletedWhileConnecting;
        resume.release();
        finishWork();
        awaitDisconnect(pendingScope);
        require(connectingStarted && pendingScope->disconnects == 1,
                "unadopted connection survived deleted ViewModel");
        auto adoptedScope = std::make_shared<PendingScope>();
        operations.connectScopes = [adoptedScope](const InstrumentSetupConfig&) {
            return OscilloscopeManager::OscMap{{"pending", adoptedScope}};
        };
        auto* adopted = new ManualControlViewModel(&model, operations);
        adopted->onInstrumentSetupConfigChanged(config);
        startQueuedConfiguration(adopted);
        finishWork();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        require(adoptedScope->disconnects == 0, "adopted scope closed with future result");
        delete adopted;
        awaitDisconnect(adoptedScope);
        require(adoptedScope->disconnects == 1, "adopted scope not closed by owner");
        int connectionAttempts = 0;
        operations.connectScopes = [&](const InstrumentSetupConfig&) -> OscilloscopeManager::OscMap {
            if (++connectionAttempts == 1) throw std::runtime_error("expected connect failure");
            return {};
        };
        ManualControlViewModel retryConfig(&model, operations);
        for (int attempt = 0; attempt < 2; ++attempt) {
            retryConfig.onInstrumentSetupConfigChanged(config);
            startQueuedConfiguration(&retryConfig);
            finishWork();
        }
        require(connectionAttempts == 2, "failed connection stranded pending-config runner");
        // Control ownership spans the whole FIFO and successful output ON state.
        {
            ManualControlModel lockModel;
            ManualControlOperations lockOps;
            int commands = 0, configurations = 0;
            bool failOff = false;
            bool restoredOn = false;
            lockOps.connectScopes = [&](const InstrumentSetupConfig&) {
                ++configurations;
                return OscilloscopeManager::OscMap{};
            };
            lockOps.input = [&](const InstrumentSetupConfig&, const InputRow&, InputAction action) {
                ++commands;
                return InstrumentOperationResult{!(failOff && action == InputAction::PowerOff), {}};
            };
            ManualControlViewModel lockVm(&lockModel, lockOps);
            lockVm.setControlAllowed(false);
            lockVm.onInstrumentSetupConfigChanged(config);
            startQueuedConfiguration(&lockVm);
            require(!lockVm.isControlActive() && configurations == 0, "disabled page reconnected instruments");
            lockVm.setControlAllowed(true);
            startQueuedConfiguration(&lockVm);
            require(lockVm.isControlActive() && lockVm.isConfigurationBusy() && !lockVm.hasActiveControl(),
                    "background connection was classified as manual control");
            require(!lockVm.scopeControlsEnabled(), "scope controls enabled before connection completion");
            finishWork();
            require(!lockVm.isControlActive() && configurations == 1, "connection completion kept lock");
            lockVm.onConditionsChanged(conditions);
            lockVm.onSelected(TableKind::Input, 0, "input");
            QObject::connect(&lockVm, &ManualControlViewModel::restoreOutputState, [&](TableKind, bool on) { restoredOn = on; });
            QVector<bool> activity;
            QObject::connect(&lockVm, &ManualControlViewModel::controlActiveChanged, [&](bool on) { activity.append(on); });
            lockVm.handleInput(InputAction::PowerOn);
            require(lockVm.isControlActive(), "accepted command did not lock immediately");
            require(lockVm.outputState(TableKind::Input) == OutputState::Unknown, "pending output was reported confirmed");
            finishWork();
            require(lockVm.isControlActive(), "output ON released ownership");
            require(lockVm.outputState(TableKind::Input) == OutputState::ConfirmedOn, "successful ON not confirmed");
            failOff = true;
            lockVm.handleInput(InputAction::PowerOff);
            finishWork();
            require(lockVm.isControlActive() && restoredOn, "failed output OFF released ownership or lost retry button");
            require(lockVm.outputState(TableKind::Input) == OutputState::Unknown, "failed OFF retained confirmed state");
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
            ManualControlModel captureModel;
            ManualControlOperations captureOps;
            auto scope = std::make_shared<PendingScope>();
            captureOps.connectScopes = [scope](const InstrumentSetupConfig&) {
                return OscilloscopeManager::OscMap{{"pending", scope}};
            };
            ManualControlViewModel captureVm(&captureModel, captureOps);
            captureVm.onInstrumentSetupConfigChanged(config);
            startQueuedConfiguration(&captureVm);
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
        std::cout << "PASS: ManualControl injected operations, snapshots, FIFO and load recovery\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
