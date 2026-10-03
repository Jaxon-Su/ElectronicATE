#include "page3viewmodel.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <iostream>
#include <stdexcept>

void check(bool value) { if (!value) throw std::runtime_error("Page3 DC assertion failed"); }
void finish() {
    check(QThreadPool::globalInstance()->waitForDone(3000));
    QCoreApplication::processEvents();
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        {
            Page3Model groupedModel;
            Page3Operations groupedOps;
            DcGroup received;
            int executions = 0;
            groupedOps.dcGroup = [&](const Page1Config&, const DcGroup& group, InputAction action) {
                received = group;
                ++executions;
                return InstrumentOperationResult{true, {}, false,
                    action == InputAction::Change ? std::nullopt : std::optional<bool>(action == InputAction::PowerOn)};
            };
            Page3ViewModel groupedVm(&groupedModel, groupedOps);
            TestConditionSnapshot snapshot;
            snapshot.dcNames = {"Group1", "Group2"};
            snapshot.dcRows = {{"24", "5", ""}, {"30", "6", ""}};
            snapshot.dcRows2 = {{"12", "3", ""}, {"15", "4", ""}};
            snapshot.dcRows3 = {{"5", "1", ""}, {"6", "2", ""}};
            groupedVm.onConditionsChanged(snapshot);
            groupedVm.onDcSelected(0, 1, "group2");
            groupedVm.onDcInputToggled(0, true);
            snapshot.dcRows2[1].vin = "16";
            groupedVm.onConditionsChanged(snapshot);
            finish();
            check(executions == 1 && received[0].vin == "30" && received[1].vin == "15"
                  && received[2].vin == "6" && groupedVm.hasActiveControl());
            groupedVm.onDcInputChanged(0);
            finish();
            check(received[1].vin == "16" && groupedVm.hasActiveControl());
            groupedVm.onConditionsChanged({});
            groupedVm.onDcInputToggled(0, false);
            finish();
            check(executions == 3 && !groupedVm.hasActiveControl());
        }
        Page3Model model;
        Page3Operations ops;
        QStringList calls;
        bool fail = false;
        bool unchanged = false;
        ops.dcInput = [&](const Page1Config&, int source, const DcRow& row, InputAction action) {
            calls << QString("%1:%2:%3:%4").arg(source).arg(int(action)).arg(row.vin, row.currentLimit);
            return InstrumentOperationResult{!fail, fail ? "offline failure" : QString(), unchanged};
        };
        Page3ViewModel vm(&model, ops);
        bool on[3] = {}, busy[3] = {};
        QStringList titles[3];
        QObject::connect(&vm, &Page3ViewModel::dcOutputStateChanged, [&](int s, bool v) { on[s] = v; });
        QObject::connect(&vm, &Page3ViewModel::dcOperationBusyChanged, [&](int s, bool v) { busy[s] = v; });
        QObject::connect(&vm, &Page3ViewModel::dcInputUpdated, [&](int s, const QStringList& t, int) { titles[s] = t; });
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
        unchanged = false;
        vm.onDcInputToggled(0, true);
        finish();
        check(on[0] && vm.hasActiveControl());
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
