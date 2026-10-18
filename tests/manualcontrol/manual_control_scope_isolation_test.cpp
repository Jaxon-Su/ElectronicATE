#include "manualcontrolpage.h"
#include "controlpagelock.h"
#include "manualcontrolclose.h"
#include "messageservice.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QPushButton>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <QScopeGuard>
#include <QTimer>
#include <array>
#include <atomic>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
void until(const std::function<bool()>& done)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 4000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(done(), "async state did not settle");
}
class Controller : public ITriggerController {
public:
    using ITriggerController::ITriggerController;
    QString getSupportedModel() const override { return "MSOSeries456"; }
    void setInstrument(Oscilloscope* value) override { scope = value; }
    Oscilloscope* getInstrument() const override { return scope; }
    Oscilloscope* scope = nullptr;
};
QWidget* TriggerWidgetFactory::createTriggerWidget(const QString&, QWidget* parent,
                                                   QObject** controller, int)
{
    auto* group = new QGroupBox("Trigger", parent);
    auto* layout = new QVBoxLayout(group);
    auto* measure = new QPushButton("Get Value", group);
    measure->setObjectName("testMeasure");
    layout->addWidget(measure);
    *controller = new Controller(group);
    return group;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/arial.ttf");
    app.setFont(QFont("Arial", 9));
    QThreadPool::globalInstance()->setMaxThreadCount(4);
    QSemaphore entered, release;
    try {
        std::array<std::atomic<int>, 6> calls{};
        std::atomic<int> opens{0};
        std::atomic<bool> redirected{false};
        InstrumentOperationResult nextResult;
        ManualControlModel model;
        ManualControlOperations ops;
        ops.connectScopes = [&](const InstrumentSetupConfig&) {
            if (++opens == 1) {
                entered.release();
                require(release.tryAcquire(1, 15000), "slow connection was not released");
            }
            return OscilloscopeManager::OscMap{}; // offline scope
        };
        auto record = [&](int device, const InstrumentSetupConfig& config) {
            if (config.instruments[0].name != "Original") redirected = true;
            ++calls[device];
            return nextResult;
        };
        ops.input = [&](const InstrumentSetupConfig& c, const InputRow&, InputAction) { return record(0, c); };
        ops.load = [&](const InstrumentSetupConfig& c, const QVector<LoadDataRow>&, int, const LoadMetaRow&, LoadAction, bool) { return record(1, c); };
        ops.dynamic = [&](const InstrumentSetupConfig& c, const QVector<DynamicDataRow>&, int, const DynamicMetaRow&, DynamicLoadAction, bool, bool) { return record(2, c); };
        ops.relay = [&](const InstrumentSetupConfig& c, const QVector<RelayDataRow>&, int, RelayAction) { return record(3, c); };
        ops.dcInput = [&](const InstrumentSetupConfig& c, int, const DcRow&, InputAction) { return record(4, c); };
        ops.dcGroup = [&](const InstrumentSetupConfig& c, const DcGroup&, InputAction) { return record(5, c); };
        ManualControlViewModel vm(&model, ops);
        // Release the blocked fake before destroying the VM on a failed assertion.
        const auto unblock = qScopeGuard([&] { release.release(); });
        QTabWidget tabs;
        tabs.addTab(new QWidget, "InstrumentSetup");
        tabs.addTab(new QWidget, "TestConditions");
        auto* page = new ManualControlPage(&vm);
        tabs.addTab(page, "ManualControl");
        tabs.addTab(new QWidget, "CommandConsole");
        tabs.addTab(new QWidget, "TestSequence");
        tabs.setCurrentIndex(2);
        int notifications = 0;
        bool loadBusy = false;
        QObject::connect(&vm, &ManualControlViewModel::loadOperationBusyChanged, &tabs,
                         [&](bool busy) { loadBusy = busy; });
        QObject::connect(&vm, &ManualControlViewModel::controlActiveChanged, &tabs, [&](bool) {
            ++notifications;
            applyControlPageLock(&tabs, {vm.hasActiveControl(), false, false});
        });
        InstrumentSetupConfig config;
        for (const auto& type : {"InputSource", "Load", "Relay", "InputDCSource", "Oscilloscope"}) {
            InstrumentConfig instrument;
            instrument.type = type;
            instrument.name = instrument.type == "InputDCSource" ? "DC Source1" : "Original";
            if (instrument.type == "Oscilloscope") instrument.modelName = "MSO44B";
            config.instruments.append(instrument);
        }
        TestConditionSnapshot conditions;
        conditions.inputRows = {{"1phase", "110", "60", "0"}};
        conditions.loadRows = {{"load", {"2"}}};
        conditions.dynamicRows = {{"dynamic", {"1~2"}}};
        conditions.relayRows = {{"relay", {"on"}}};
        conditions.dcNames = {"DC1"};
        conditions.dcRows = {{"24", "5", ""}};
        vm.onConditionsChanged(conditions);
        vm.onSelected(TableKind::Input, 0, "input");
        vm.onSelected(TableKind::Load, 0, "load");
        vm.onSelected(TableKind::DyLoad, 0, "dynamic");
        vm.onSelected(TableKind::Relay, 0, "relay");
        vm.onDcSelected(0, 0, "DC1");
        vm.onInstrumentSetupConfigChanged(config);
        until([&] { return entered.available() != 0; });
        require(vm.isConfigurationBusy() && page->isEnabled(), "slow scope disabled ManualControl");
        require(!page->findChild<QPushButton*>("btnPic")->isEnabled() &&
                !page->findChild<QPushButton*>("testMeasure")->isEnabled(), "scope controls enabled while connecting");
        require(page->findChild<QPushButton*>("btnChange")->isEnabled() &&
                page->findChild<QPushButton*>("btnDcInput1")->isEnabled(), "other controls disabled while connecting");
        bool fileDialog = false;
        vm.setCaptureFileSelector([&](const QString&, const QString&, const QString&) { fileDialog = true; return QString{}; });
        vm.OnWaveformCaptured();
        require(!fileDialog && !vm.borrowOscilloscope(), "scope operation admitted during connection");
        const std::array<std::function<void(int)>, 6> actions{
            [&](int action) { vm.handleInput(InputAction(action)); },
            [&](int action) { vm.handleLoad(LoadAction(action)); },
            [&](int action) { vm.handleDyLoad(DynamicLoadAction(action)); },
            [&](int action) { vm.handleRelay(RelayAction(action)); },
            [&](int action) { vm.handleDcInput(0, InputAction(action)); },
            [&](int action) { if (action == int(InputAction::Change)) vm.onDcGroupChanged(); else vm.onDcGroupToggled(action == int(InputAction::PowerOn)); }
        };
        const std::array<std::array<int, 3>, 6> modes{{
            {int(InputAction::PowerOn), int(InputAction::Change), int(InputAction::PowerOff)},
            {int(LoadAction::LoadOn), int(LoadAction::Change), int(LoadAction::LoadOff)},
            {int(DynamicLoadAction::LoadOn), int(DynamicLoadAction::Change), int(DynamicLoadAction::LoadOff)},
            {int(RelayAction::RelayOn), int(RelayAction::Change), int(RelayAction::RelayOff)},
            {int(InputAction::PowerOn), int(InputAction::Change), int(InputAction::PowerOff)},
            {int(InputAction::PowerOn), int(InputAction::Change), int(InputAction::PowerOff)}
        }};
        for (int device = 0; device < 6; ++device) {
            const int before = notifications;
            for (int action = 0; action < 3; ++action) {
                actions[device](modes[device][action]);
                until([&] { return calls[device] == action + 1; });
                // Completion is queued to the GUI; allow it to settle before the next action.
                until([&] { return !loadBusy && (action == 2 ? !vm.hasActiveControl() :
                    (device < 4 ? vm.outputState(std::array{TableKind::Input, TableKind::Load, TableKind::DyLoad, TableKind::Relay}[device]) == OutputState::ConfirmedOn :
                                  !vm.dcBusy(0) && vm.dcOutputOn(0))); });
                require(vm.isConfigurationBusy() && page->isEnabled(), "manual action cancelled or waited for scope connection");
                require(tabs.isTabEnabled(0) == (action == 2), "ownership notification lost while scope stayed busy");
            }
            require(notifications >= before + 2, "aggregate busy masked manual ownership changes");
        }
        vm.handleInput(InputAction::PowerOn);
        until([&] { return calls[0] == 4 && vm.outputState(TableKind::Input) == OutputState::ConfirmedOn; });
        config.instruments[0].name = "Replacement";
        vm.onInstrumentSetupConfigChanged(config);
        release.release();
        until([&] { return !vm.isConfigurationBusy(); });
        require(!tabs.isTabEnabled(0) && opens == 1, "pending settings replaced an active output configuration");
        vm.handleInput(InputAction::PowerOff);
        until([&] { return calls[0] == 5 && !vm.hasActiveControl(); });
        require(!redirected, "OFF targeted a pending configuration instead of the active one");
        until([&] { return opens == 2 && !vm.isConfigurationBusy(); });
        require(page->isEnabled() && !vm.scopeControlsEnabled(), "failed scope connection locked unrelated controls");
        const int completed = calls[3];
        vm.handleRelay(RelayAction::RelayOn);
        until([&] { return calls[3] == completed + 1 && vm.outputState(TableKind::Relay) == OutputState::ConfirmedOn; });
        vm.handleRelay(RelayAction::RelayOff);
        until([&] { return !vm.hasActiveControl(); });
        vm.setControlAllowed(false);
        const int blocked = calls[0];
        vm.handleInput(InputAction::PowerOn);
        require(calls[0] == blocked && !vm.hasActiveControl(), "console/test ownership interlock bypassed");
        vm.setControlAllowed(true);

        auto buttonFor = [&](const QString& title) -> QPushButton* {
            for (auto* group : page->findChildren<QGroupBox*>())
                if (group->title() == title)
                    for (auto* button : group->findChildren<QPushButton*>())
                        if (button->isCheckable()) return button;
            throw std::runtime_error("output button missing");
        };
        int warnings = 0;
        TableKind expectedKind = TableKind::Input;
        QPushButton* expectedButton = nullptr;
        const auto warningCheck = QObject::connect(&MessageService::instance(), &MessageService::warningRequested, &tabs,
                         [&](const QString&, const QString&) {
            ++warnings;
            require(!vm.hasPendingControlOperation(), "warning displayed before completion settled");
            const bool unknown = !nextResult.outputUnchanged;
            require(vm.hasActiveControl() == unknown, "warning displayed before ownership settled");
            require(expectedButton->text() == (unknown ? "UNKNOWN" : "OFF"), "warning displayed with stale ON button");
            require(expectedButton->isChecked() == unknown && expectedButton->isEnabled(), "OFF retry state wrong");
            require(vm.outputState(expectedKind) == (unknown ? OutputState::Unknown : OutputState::ConfirmedOff),
                    "failed control knowledge was corrupted");
        });
        const std::array<TableKind, 4> kinds{TableKind::Input, TableKind::Load, TableKind::DyLoad, TableKind::Relay};
        const std::array<QString, 4> titles{"AC Input", "Load", "Dynamic Load", "Relay"};
        for (int device = 0; device < 4; ++device) {
            expectedKind = kinds[device];
            expectedButton = buttonFor(titles[device]);
            nextResult = {false, "viOpen failed", true};
            const int before = warnings;
            expectedButton->click();
            require(vm.hasPendingControlOperation() && !expectedButton->isEnabled(), "pending command not locked");
            require(expectedButton->property("outputStatus") == "busy", "pending output style not neutral");
            until([&] { return warnings == before + 1; });
            require(!vm.hasActiveControl() && !vm.hasUnknownOutput() && tabs.isTabEnabled(0),
                    "open failure stranded close/page locks");
            nextResult = {false, "ON write/readback failed", false};
            expectedButton->click();
            until([&] { return warnings == before + 2; });
            require(vm.hasUnknownOutput() && !vm.hasPendingControlOperation(), "unknown output not distinguished from running work");
            require(expectedButton->property("outputStatus") == "unknown", "unknown output style missing");
            const auto fill = expectedButton->grab().toImage().pixelColor(8, 8);
            require(fill.red() > fill.blue() + 30 && fill.green() > fill.blue(), "UNKNOWN still rendered blue");
            if (device == 3) require(expectedButton->grab().save("manualControl-relay-unknown.png"), "unknown preview failed");
            nextResult = {};
            expectedButton->click(); // UNKNOWN must attempt OFF, never another ON.
            until([&] { return !vm.hasActiveControl(); });
            require(!expectedButton->isChecked() && expectedButton->text() == "OFF", "successful OFF did not recover");
            require(expectedButton->property("outputStatus") == "normal", "successful OFF kept unknown style");
        }
        // Two TestSequence aliases of the same hardware must never deadlock recovery.
        for (const auto &title : {QString("Load"), QString("Dynamic Load")}) {
            vm.adoptUncertainOutputs(config, {TableKind::Load, TableKind::DyLoad});
            auto* load = buttonFor("Load");
            auto* dynamic = buttonFor("Dynamic Load");
            require(load->isEnabled() && dynamic->isEnabled(), "dual UNKNOWN disabled both OFF buttons");
            require(load->text() == "UNKNOWN" && dynamic->text() == "UNKNOWN", "dual UNKNOWN not visible");
            nextResult = {};
            buttonFor(title)->click();
            until([&] { return !vm.hasActiveControl(); });
            require(load->text() == "OFF" && dynamic->text() == "OFF", "shared load OFF did not settle both aliases");
        }
        QObject::disconnect(warningCheck);
        // Failed reconnect before an OFF write preserves the last known ON.
        // Keep the interlock until a later OFF succeeds.
        vm.handleRelay(RelayAction::RelayOn);
        until([&] { return !vm.hasPendingControlOperation(); });
        nextResult = {false, "reconnect unavailable before OFF", true};
        vm.handleRelay(RelayAction::RelayOff);
        until([&] { return !vm.hasPendingControlOperation(); });
        require(!vm.hasUnknownOutput() && vm.outputState(TableKind::Relay) == OutputState::ConfirmedOn &&
                vm.hasActiveControl() && !tabs.isTabEnabled(0), "failed OFF reconnect cleared a known ON output");
        nextResult = {};
        vm.handleRelay(RelayAction::RelayOff);
        until([&] { return !vm.hasActiveControl(); });
        require(tabs.isTabEnabled(0) && buttonFor("Relay")->text() == "OFF", "successful OFF did not unlock InstrumentSetup");
        for (bool unknown : {false, true}) {
            for (bool accept : {false, true}) {
                QTimer::singleShot(0, [&] {
                    auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                    require(dialog && dialog->objectName() == "activeOutputExitDialog", "exit confirmation missing");
                    require(dialog->defaultButton()->text() == "Return to controls", "exit default was unsafe");
                    for (auto* button : dialog->buttons())
                        if (dialog->buttonRole(button) == (accept ? QMessageBox::AcceptRole : QMessageBox::RejectRole))
                            button->click();
                });
                require(confirmActiveOutputExit(page, unknown) == accept, "exit confirmation choice ignored");
            }
        }
        tabs.resize(1100, 850);
        tabs.show();
        app.processEvents();
        if (argc > 1) require(page->grab().save(QString::fromLocal8Bit(argv[1])), "UI preview failed");
        require(QThreadPool::globalInstance()->waitForDone(3000), "workers leaked");
        std::cout << "PASS: disconnected scope isolates controls; ON/Change/OFF and ownership remain independent\n";
    } catch (const std::exception& error) {
        release.release();
        QThreadPool::globalInstance()->waitForDone(3000);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
