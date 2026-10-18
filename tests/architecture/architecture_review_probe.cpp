#include "../support/configuration_wait.h"
#include "manualcontrolviewmodel.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <QSemaphore>
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
static void finish() {
    require(QThreadPool::globalInstance()->waitForDone(3000), "worker timeout");
    QCoreApplication::processEvents();
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        ManualControlModel model; ManualControlOperations operations;
        QVector<bool> dirtyFlags;
        enum class Outcome { Success, Failure, StandardException, UnknownException };
        Outcome outcome = Outcome::Success;
        bool block = false;
        QSemaphore entered, released;
        operations.connectScopes = [](const InstrumentSetupConfig &) { return OscilloscopeManager::OscMap{}; };
        operations.dynamic = [&](const InstrumentSetupConfig &, const QVector<DynamicDataRow> &, int,
                                 const DynamicMetaRow &, DynamicLoadAction, bool, bool dirty) {
            dirtyFlags << dirty;
            if (block) {
                entered.release();
                if (!released.tryAcquire(1, 3000)) throw std::runtime_error("blocked operation timeout");
            }
            if (outcome == Outcome::StandardException) throw std::runtime_error("injected failure");
            if (outcome == Outcome::UnknownException) throw 42;
            return outcome == Outcome::Failure
                ? InstrumentOperationResult{false, "injected connection failure", true}
                : InstrumentOperationResult{};
        };
        ManualControlViewModel vm(&model, operations);
        InstrumentSetupConfig config; InstrumentConfig load; load.type = "Load"; load.enabled = true;
        load.name = "Load1"; load.modelName = "63600";
        load.commConfig.protocol = ProtocolType::GPIB; load.commConfig.gpibAddress = 4;
        load.channels = {{"63640-80-80", 1, 0}}; load.channelNumbers = {1}; config.instruments << load;
        vm.onInstrumentSetupConfigChanged(config); startQueuedConfiguration(&vm); finish();
        TestConditionSnapshot conditions; conditions.dynamicRows = {{"dynamic", {"1~2"}}};
        conditions.dynamicMeta.t1t2 = {"10~10"}; vm.onConditionsChanged(conditions);
        vm.onSelected(TableKind::DyLoad, 0, "dynamic");
        auto operate = [&](DynamicLoadAction action) {
            const auto before = dirtyFlags.size();
            vm.handleDyLoad(action); finish();
            require(dirtyFlags.size() == before + 1, "operation rejected or not completed");
        };
        // Default enabled sync has no pending update to consume.
        require(vm.loadSyncSettings().enabled && !vm.loadSyncSettings().dirty, "wrong default sync");
        operate(DynamicLoadAction::LoadOn);
        require(!vm.loadSyncSettings().dirty, "default sync created a pending update");

        for (const auto fault : {Outcome::Failure, Outcome::StandardException, Outcome::UnknownException}) {
            for (const auto action : {DynamicLoadAction::LoadOn, DynamicLoadAction::Change}) {
                vm.onSyncChanged(false); outcome = fault;
                operate(action);
                const auto testSequenceSnapshot = vm.loadSyncSettings();
                require(!testSequenceSnapshot.enabled && testSequenceSnapshot.dirty && dirtyFlags.last(),
                        "failure lost pending sync or TestSequence snapshot");
                outcome = Outcome::Success; operate(action);
                require(dirtyFlags.last() && !vm.loadSyncSettings().dirty, "successful retry did not apply/consume sync");
                require(testSequenceSnapshot.dirty, "captured TestSequence snapshot was mutated");
                operate(action);
                require(!dirtyFlags.last(), "acknowledged sync still pending");
            }
        }
        // OFF does not apply synchronization, regardless of its output result.
        for (const auto fault : {Outcome::Success, Outcome::Failure, Outcome::StandardException, Outcome::UnknownException}) {
            vm.onSyncChanged(false); outcome = fault; operate(DynamicLoadAction::LoadOff);
            require(vm.loadSyncSettings().dirty, "OFF consumed a sync update");
            outcome = Outcome::Success; operate(DynamicLoadAction::LoadOn);
            require(dirtyFlags.last() && !vm.loadSyncSettings().dirty, "ON after OFF lost sync update");
        }
        // The older successful operation must not acknowledge changes made while it runs.
        for (bool initiallyPending : {false, true}) {
            vm.onSyncChanged(initiallyPending ? false : true);
            require(vm.loadSyncSettings().dirty == initiallyPending, "bad concurrent fixture");
            block = true; vm.handleDyLoad(DynamicLoadAction::LoadOn);
            const bool started = entered.tryAcquire(1, 3000);
            const bool pendingWhileRunning = vm.loadSyncSettings().dirty;
            vm.onSyncChanged(true); vm.onSyncChanged(false);
            released.release(); finish(); block = false;
            require(started && pendingWhileRunning == initiallyPending, "pending update consumed before completion");
            require(vm.loadSyncSettings().dirty, "stale success cleared newer update");
            operate(DynamicLoadAction::Change);
            require(dirtyFlags.last() && !vm.loadSyncSettings().dirty, "newer update was not applied");
        }
        // A toggle during completion signals also remains pending (GUI reentrancy).
        vm.onSyncChanged(false);
        bool notified = false;
        auto connection = QObject::connect(&vm, &ManualControlViewModel::loadOperationBusyChanged, &vm, [&](bool busy) {
            if (!busy && !notified) { notified = true; vm.onSyncChanged(false); }
        });
        operate(DynamicLoadAction::Change);
        QObject::disconnect(connection);
        require(notified && vm.loadSyncSettings().dirty, "completion callback lost new update");
        operate(DynamicLoadAction::Change);
        require(dirtyFlags.last() && !vm.loadSyncSettings().dirty, "reentrant update was not applied");
        std::cout << "C01 regression: retry/exception, OFF, successful ON/Change, newer update, TestSequence snapshot verified\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
