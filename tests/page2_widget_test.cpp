#include "page2.h"
#include <QApplication>
#include <QTabWidget>
#include <QTableWidget>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    try {
        Page2Model model;
        Page2ViewModel vm(&model);
        Page2 page(&vm);
        TestConditionSnapshot initial;
        initial.inputRows = {{"1phase", "110", "60", "0"}};
        initial.dcRows = {{"48"}};
        initial.dcRows[0].currentLimit = "5";
        initial.dcRows2 = {{"12", "3", "aux"}};
        initial.dcRows3 = {{"5", "1", "logic"}};
        initial.dcSourceTab = 2;
        initial.loadMeta = {{"CC"}, {"Auto Range"}, {"out"}, {"12"}, {"1"}};
        initial.loadRows = {{"load", {"2"}}};
        initial.dynamicMeta = {{"Auto Range"}, {"12"}, {"1"}, {"10~20"}};
        initial.dynamicRows = {{"dynamic", {"1~2"}}};
        initial.relayRows = {{"relay", {"on"}}};
        model.setSnapshot(initial);
        require(QMetaObject::invokeMethod(&page, "resetUIFromViewModel", Qt::DirectConnection), "reset slot unavailable");
        int snapshots = 0;
        QObject::connect(&vm, &Page2ViewModel::conditionsChanged, [&](const auto&) { ++snapshots; });
        page.syncUIToViewModel();
        require(snapshots == 1, "UI sync emitted multiple snapshots");
        auto result = vm.conditions();
        require(result.dcRows[0].currentLimit == "5" && result.dcRows2[0].label == "aux"
                    && result.dcRows3[0].vin == "5" && result.dcSourceTab == 2,
                "DC widgets lost data during restore/sync");
        auto *dc2 = page.findChild<QTableWidget*>("dcSource2Table");
        qobject_cast<QLineEdit*>(dc2->cellWidget(0, 3))->setText("4");
        require(vm.conditions().dcRows2[0].currentLimit == "4", "DC2 current edit did not reach model");
        require(result.inputRows.size() == 1 && result.inputRows[0].vin == "110"
                    && result.dcRows.size() == 1 && result.dcRows[0].vin == "48", "input data changed during UI round trip");
        require(result.loadRows.size() == 1 && result.loadRows[0].values[0] == "2"
                    && result.loadMeta.vo[0] == "12", "load snapshot changed during UI round trip");
        require(result.dynamicRows.size() == 1 && result.dynamicMeta.t1t2[0] == "10~20", "dynamic timing lost");
        vm.setMaxOutput(2);
        result = vm.conditions();
        require(result.inputRows[0].vin == "110" && result.dcRows[0].vin == "48"
                    && result.loadRows.size() == 1 && result.dynamicRows.size() == 1, "dynamic header update erased other tables");
        std::cout << "PASS: offscreen Page2 snapshot round trip and output resize\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
