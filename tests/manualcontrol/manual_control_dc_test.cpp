#include "../support/configuration_wait.h"
#include "manualcontrolviewmodel.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <iostream>
#include <stdexcept>

void check(bool value) { if (!value) throw std::runtime_error("ManualControl DC assertion failed"); }
void finish() {
    check(QThreadPool::globalInstance()->waitForDone(3000));
    QCoreApplication::processEvents();
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        {
            ManualControlModel groupedModel;
            ManualControlOperations groupedOps;
            groupedOps.connectScopes = [](const InstrumentSetupConfig&) { return OscilloscopeManager::OscMap{}; };
            DcGroup received;
            int executions = 0;
            groupedOps.dcGroup = [&](const InstrumentSetupConfig&, const DcGroup& group, InputAction action) {
                received = group;
                ++executions;
                return InstrumentOperationResult{true, {}, false,
                    action == InputAction::Change ? std::nullopt : std::optional<bool>(action == InputAction::PowerOn)};
            };
            ManualControlViewModel groupedVm(&groupedModel, groupedOps);
            InstrumentSetupConfig config;
            config.dcInputs = 3;
            for (int i = 0; i < 3; ++i) {
                InstrumentConfig instrument;
                instrument.type = "InputDCSource";
                instrument.name = QString("DC Source%1").arg(i + 1);
                config.instruments.append(instrument);
            }
            groupedVm.onInstrumentSetupConfigChanged(config);
            startQueuedConfiguration(&groupedVm);
            finish();
            TestConditionSnapshot snapshot;
            snapshot.dcNames = {"Group1", "Group2"};
            snapshot.dcRows = {{"24", "5", ""}, {"30", "6", ""}};
            snapshot.dcRows2 = {{"12", "3", ""}, {"15", "4", ""}};
            snapshot.dcRows3 = {{"5", "1", ""}, {"6", "2", ""}};
            groupedVm.onConditionsChanged(snapshot);
            groupedVm.onDcSelected(0, 1, "group2");
            groupedVm.onDcGroupToggled(true);
            snapshot.dcRows2[1].vin = "16";
            groupedVm.onConditionsChanged(snapshot);
            finish();
            check(executions == 1 && received[0].vin == "30" && received[1].vin == "15"
                  && received[2].vin == "6" && groupedVm.hasActiveControl());
            check(groupedVm.dcOutputOn(0) && groupedVm.dcOutputOn(1) && groupedVm.dcOutputOn(2));
            groupedVm.onDcGroupChanged();
            finish();
            check(received[1].vin == "16" && groupedVm.hasActiveControl());
            groupedVm.onConditionsChanged({});
            groupedVm.onDcGroupToggled(false);
            finish();
            check(executions == 3 && !groupedVm.hasActiveControl());
        }
        {
            ManualControlModel model;
            ManualControlOperations ops;
            int groupCalls = 0, singleCalls = 0, selectedSource = -1;
            DcRow received;
            InstrumentOperationResult groupResult;
            ops.connectScopes = [](const InstrumentSetupConfig&) { return OscilloscopeManager::OscMap{}; };
            ops.dcGroup = [&](const InstrumentSetupConfig&, const DcGroup&, InputAction) {
                ++groupCalls;
                return groupResult;
            };
            ops.dcInput = [&](const InstrumentSetupConfig&, int source, const DcRow& row, InputAction) {
                ++singleCalls;
                selectedSource = source;
                received = row;
                return InstrumentOperationResult{};
            };
            ManualControlViewModel vm(&model, ops);
            InstrumentSetupConfig config;
            config.dcInputs = 3;
            for (int i = 0; i < 3; ++i) {
                InstrumentConfig instrument;
                instrument.type = "InputDCSource";
                instrument.name = QString("DC Source%1").arg(i + 1);
                instrument.enabled = i != 2;
                config.instruments.append(instrument);
            }
            vm.onInstrumentSetupConfigChanged(config);
            startQueuedConfiguration(&vm);
            finish();
            TestConditionSnapshot conditions;
            conditions.dcNames = {"A", "B"};
            conditions.dcRows = {{"12", "2", ""}, {"15", "2", ""}};
            conditions.dcRows2 = {{"5", "1", ""}, {"6", "1", ""}};
            vm.onConditionsChanged(conditions);
            vm.onDcSelected(1, 1, "B");
            vm.onDcInputToggled(1, true);
            // Group requests cannot overlap an active member's operation.
            vm.onDcGroupToggled(true);
            finish();
            check(singleCalls == 1 && groupCalls == 0 && selectedSource == 1 && received.vin == "6");
            check(!vm.dcOutputOn(0) && vm.dcOutputOn(1) && !vm.dcOutputOn(2));
            groupResult = {false, "preflight failure", true};
            vm.onDcGroupToggled(true);
            check(vm.dcBusy(0) && vm.dcBusy(1) && !vm.dcBusy(2));
            vm.onDcInputToggled(0, false);
            finish();
            check(singleCalls == 1 && groupCalls == 1);
            check(!vm.dcOutputOn(0) && vm.dcOutputOn(1) && !vm.dcOutputOn(2));
            groupResult = {};
            vm.onDcGroupToggled(true);
            finish();
            check(vm.dcOutputOn(0) && vm.dcOutputOn(1) && !vm.dcOutputOn(2));
            vm.onDcInputToggled(1, false);
            finish();
            check(vm.dcOutputOn(0) && !vm.dcOutputOn(1) && vm.hasActiveControl());
            groupResult = {false, "ON failed, rollback confirmed OFF", false, false};
            vm.onDcGroupToggled(true);
            finish();
            check(!vm.dcOutputOn(0) && !vm.dcOutputOn(1) && !vm.hasActiveControl());
        }
        ManualControlModel model;
        ManualControlOperations ops;
        QStringList calls;
        bool fail = false;
        bool unchanged = false;
        ops.dcInput = [&](const InstrumentSetupConfig&, int source, const DcRow& row, InputAction action) {
            calls << QString("%1:%2:%3:%4").arg(source).arg(int(action)).arg(row.vin, row.currentLimit);
            return InstrumentOperationResult{!fail, fail ? "offline failure" : QString(), unchanged};
        };
        ManualControlViewModel vm(&model, ops);
        bool on[3] = {}, busy[3] = {};
        QStringList titles[3];
        QObject::connect(&vm, &ManualControlViewModel::dcOutputStateChanged, [&](int s, bool v) { on[s] = v; });
        QObject::connect(&vm, &ManualControlViewModel::dcOperationBusyChanged, [&](int s, bool v) { busy[s] = v; });
        QObject::connect(&vm, &ManualControlViewModel::dcInputUpdated, [&](int s, const QStringList& t, int) { titles[s] = t; });
        TestConditionSnapshot conditions;
        conditions.dcRows = {{"", "", ""}};
        vm.onConditionsChanged(conditions);
        check(titles[0] == QStringList{QString()});
        vm.onDcInputToggled(0, true);
        check(calls.isEmpty() && !vm.hasActiveControl());
        conditions.dcRows = {{"24", "5", ""}};
        conditions.dcRows2 = {{"12", "3", "aux"}};
        conditions.dcRows3 = {{"5", "1", "logic"}};
        vm.onConditionsChanged(conditions);
        for (int s = 0; s < 3; ++s) vm.onDcSelected(s, 0, titles[s][0]);
        for (int s = 0; s < 3; ++s) vm.onDcInputToggled(s, true);
        check(busy[0] && busy[1] && busy[2] && vm.hasActiveControl());
        vm.onDcInputChanged(0);
        conditions.dcRows[0].vin = "30";
        vm.onConditionsChanged(conditions);
        for (int s = 0; s < 3; ++s) finish();
        check(calls == QStringList{"0:0:24:5", "1:0:12:3", "2:0:5:1"});
        check(on[0] && on[1] && on[2] && !busy[0] && !busy[1] && !busy[2]);
        vm.onDcInputChanged(0);
        finish();
        check(calls.last() == "0:2:30:5" && on[0]);
        fail = true;
        vm.onDcInputToggled(1, false);
        finish();
        check(on[1] && !busy[1] && vm.hasActiveControl());
        check(vm.dcOutputState(1) == OutputState::Unknown);
        fail = false;
        vm.onConditionsChanged({});
        for (int s = 0; s < 3; ++s) vm.onDcInputToggled(s, false);
        for (int s = 0; s < 3; ++s) finish();
        check(!on[0] && !on[1] && !on[2] && !vm.hasActiveControl());
        vm.onConditionsChanged(conditions);
        vm.onDcSelected(0, 0, "30 V / 5 A");
        fail = true;
        unchanged = true;
        vm.onDcInputToggled(0, true);
        finish();
        check(!on[0] && !vm.hasActiveControl());
        check(vm.dcOutputState(0) == OutputState::ConfirmedOff);
        unchanged = false;
        vm.onDcInputToggled(0, true);
        finish();
        check(on[0] && vm.hasActiveControl());
        check(vm.dcOutputState(0) == OutputState::Unknown);
        fail = false;
        vm.onDcInputToggled(0, false);
        finish();
        const auto count = calls.size();
        vm.setControlAllowed(false);
        vm.onDcInputToggled(0, true);
        vm.onDcInputToggled(3, true);
        check(calls.size() == count);
        std::cout << "PASS: independent DC states, snapshots, busy/failure and OFF without rows\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
