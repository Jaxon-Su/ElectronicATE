#include "page2.h"
#include "page3viewmodel.h"
#include <QApplication>
#include <QTableWidget>
#include <QKeyEvent>
#include <iostream>
#include <stdexcept>
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    try {
        Page2Model model;
        Page2ViewModel vm(&model);
        Page2 page(&vm);
        Page3Model controlModel;
        Page3Operations operations;
        operations.dcGroup = [](const Page1Config&, const DcGroup&, InputAction) { return InstrumentOperationResult{}; };
        Page3ViewModel control(&controlModel, operations);
        QObject::connect(&vm, &Page2ViewModel::conditionsChanged, &control, &Page3ViewModel::onConditionsChanged);
        QStringList titles;
        int selected = -1;
        QObject::connect(&control, &Page3ViewModel::dcInputUpdated,
            [&](int source, const QStringList& values, int index) { if (!source) { titles = values; selected = index; } });
        TestConditionSnapshot initial;
        initial.inputRows = {{"1phase", "110", "60", "0"}};
        initial.dcNames = {"Startup", "Rated load"};
        initial.dcRows = {{"24", "5", ""}, {"30", "6", ""}};
        initial.dcRows2 = {{"12", "3", "aux"}}; // Legacy unequal lengths are padded by the view.
        initial.dcRows3 = {{"5", "1", "logic"}};
        model.setSnapshot(initial);
        QMetaObject::invokeMethod(&page, "resetUIFromViewModel", Qt::DirectConnection);
        auto* table = page.findChild<QTableWidget*>("dcSourceTable");
        require(table && table->columnCount() == 6 && table->rowCount() == 4, "DC group layout");
        require(table->horizontalHeaderItem(5)->text() == "Index3" && table->rowSpan(0, 0) == 2, "Index headers/sequence span");
        page.syncUIToViewModel();
        require(vm.conditions().dcRows2.size() == 2 && vm.conditions().dcRows2[0].label == "aux", "legacy rows lost");
        require(selected == 0 && titles[0] == "Startup", "group combo title");
        require(table->isColumnHidden(4) && table->isColumnHidden(5), "default DC count must be one");
        Page1Config threeSources;
        threeSources.dcInputs = 3;
        vm.onPage1ConfigChanged(threeSources);
        page.show();
        QApplication::processEvents();
        for (int source = 0; source < 3; ++source) {
            auto* voltage = qobject_cast<QLineEdit*>(table->cellWidget(0, source + 3));
            auto* current = qobject_cast<QLineEdit*>(table->cellWidget(1, source + 3));
            voltage->setText("20"); current->setText("2");
            require(vm.conditions().dcSourceRows(source)[0].vin == "20", "index routing");
            voltage->setFocus();
            QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
            QApplication::sendEvent(voltage, &down);
            require(current->hasFocus(), "voltage/current navigation");
        }
        auto* first = table->cellWidget(0, 3);
        first->setFocus();
        QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(first, &right);
        require(table->cellWidget(0, 4)->hasFocus(), "Index navigation");
        qobject_cast<QLineEdit*>(table->cellWidget(0, 1))->setText("Main & Aux");
        require(titles[0] == "Main & Aux" && vm.conditions().dcNames[0] == "Main & Aux", "custom DC name propagation");
        QString xml;
        QXmlStreamWriter writer(&xml); model.writeXml(writer);
        Page2Model loaded;
        QXmlStreamReader reader(xml); reader.readNextStartElement(); loaded.loadXml(reader);
        require(!reader.hasError() && loaded.snapshot().dcRows3[0].currentLimit == "2" && loaded.snapshot().dcNames[0] == "Main & Aux", "group XML roundtrip");
        vm.addRow(TableKind::Dc);
        require(table->rowCount() == 6 && vm.conditions().dcRows3.size() == 3, "add group");
        table->selectRow(2);
        vm.removeRow(TableKind::Dc);
        require(table->rowCount() == 4 && vm.conditions().dcRows2.size() == 2, "delete whole group");
        vm.setMaxOutput(2);
        require(vm.conditions().inputRows[0].vin == "110" && vm.conditions().dcRows[0].vin == "20", "load resize erased DC");
        Page1Config config;
        for (int count : {1, 2, 3}) {
            config.dcInputs = count;
            vm.onPage1ConfigChanged(config);
            for (int source = 0; source < 3; ++source)
                require(table->isColumnHidden(source + 3) == (source >= count), "DC count visibility");
            require(vm.conditions().dcRows3[0].vin == "20", "DC count erased hidden data");
        }
        std::cout << "PASS: indexed DC UI, navigation, grouping, Page3 and XML\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
