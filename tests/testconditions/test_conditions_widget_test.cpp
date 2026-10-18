#include "testconditionspage.h"
#include "conditionrowsmodel.h"
#include "conditionrowsview.h"
#include <QComboBox>
#include <QTableView>
#include <QShortcut>
#include <QSplitter>
#include <QScrollBar>
#include <QHeaderView>
#include "manualcontrolviewmodel.h"
#include <QApplication>
#include <QFontDatabase>
#include <QTableView>
#include <QLineEdit>
#include "groupedconditionsmodel.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>
#include <iostream>
#include <stdexcept>
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void mouse(QWidget *widget, QEvent::Type type, QPoint position, Qt::MouseButton button,
           Qt::MouseButtons buttons)
{
    QMouseEvent event(type, position, widget->mapToGlobal(position), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

void clickCell(QTableView *view, int row)
{
    const auto point = view->visualRect(view->model()->index(row, 0)).center();
    mouse(view->viewport(), QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    mouse(view->viewport(), QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
}

void dragRows(QTableView *view, int first, int last)
{
    auto start = view->visualRect(view->model()->index(first, 0)).center();
    auto end = view->visualRect(view->model()->index(last, 0)).center();
    start.setX(2);
    end.setX(view->columnWidth(0) - 3);
    mouse(view->viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(view->viewport(), QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
    mouse(view->viewport(), QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();
}

void verifyDragCopy()
{
    TestConditionsModel model;
    TestConditionsViewModel vm(&model);
    TestConditionSnapshot conditions;
    conditions.inputRows = {{"1phase", "110", "60", "0"}, {"3phase", "220", "50", "90"}};
    vm.setConditions(conditions);
    ConditionRowsView view(&vm, TableKind::Input);
    view.resize(500, 200);
    view.show();
    QApplication::processEvents();
    clickCell(&view, 0);
    require(!view.property("conditionRangeSelection").toBool(), "click highlights whole row");
    dragRows(&view, 0, 1);
    require(view.selectionModel()->selectedRows().size() == 2, "drag did not select both rows");
    require(view.property("conditionRangeSelection").toBool(), "drag selection invisible");
    QKeyEvent control(QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier);
    QApplication::sendEvent(&view, &control);
    QKeyEvent copy(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
    QApplication::sendEvent(&view, &copy);
    require(view.property("conditionRangeSelection").toBool(), "Ctrl+C hides the dragged range");
    auto *editor = view.indexWidget(view.model()->index(0, 3));
    bool copied = false;
    QTimer closeMenu;
    closeMenu.setSingleShot(true);
    QObject::connect(&closeMenu, &QTimer::timeout, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (!menu) return;
        for (auto *action : menu->actions()) {
            if (action->text().contains("Copy  (2 rows)") && action->isEnabled() &&
                view.property("conditionRangeSelection").toBool()) {
                action->trigger();
                copied = true;
                break;
            }
        }
        menu->close();
    });
    closeMenu.start(0);
    const auto point = editor->rect().center();
    mouse(editor, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton);
    mouse(editor, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);
    QContextMenuEvent context(QContextMenuEvent::Mouse, point, editor->mapToGlobal(point));
    QApplication::sendEvent(editor, &context);
    require(copied, "right click on editor lost the range Copy menu or highlight");
    for (auto *shortcut : view.findChildren<QShortcut*>())
        if (shortcut->key() == QKeySequence::Paste) QMetaObject::invokeMethod(shortcut, "activated");
    QApplication::processEvents();
    require(vm.inputRows().size() == 4 && vm.inputRows()[2].vin == "110" &&
                vm.inputRows()[3].vin == "220", "drag/right-click copy did not preserve both rows");
    require(!view.property("conditionRangeSelection").toBool(), "model reset retained drag highlight");
    dragRows(&view, 0, 1);
    editor = view.indexWidget(view.model()->index(0, 3));
    mouse(editor, QEvent::MouseButtonPress, editor->rect().center(), Qt::LeftButton, Qt::LeftButton);
    mouse(editor, QEvent::MouseButtonRelease, editor->rect().center(), Qt::LeftButton, Qt::NoButton);
    require(!view.property("conditionRangeSelection").toBool() &&
                !editor->property("conditionSelected").toBool(), "editing leaves blue range behind");
    dragRows(&view, 0, 1);
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(&view, &key);
    require(!view.property("conditionRangeSelection").toBool(), "keyboard navigation retains blue range");
}

void verifyAcKeyboardNavigation()
{
    TestConditionsModel model;
    TestConditionsViewModel vm(&model);
    TestConditionSnapshot conditions;
    conditions.inputRows = {{"1phase", "110", "60", "0"},
                            {"1phase", "220", "50", "90"},
                            {"3phase", "380", "60", "120"}};
    vm.setConditions(conditions);
    ConditionRowsView view(&vm, TableKind::Input);
    view.resize(460, 130);
    view.show();
    QApplication::processEvents();
    auto editor = [&](int row, int column) {
        auto* widget = view.indexWidget(view.model()->index(row, column));
        require(widget != nullptr, "AC navigation editor missing");
        return widget;
    };
    auto focus = [&](int row, int column) {
        view.setCurrentIndex(view.model()->index(row, column));
        editor(row, column)->setFocus();
        require(editor(row, column)->hasFocus(), "AC test could not focus editor");
    };
    auto key = [&](int keyCode, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QKeyEvent event(QEvent::KeyPress, keyCode, modifiers);
        QApplication::sendEvent(QApplication::focusWidget(), &event);
    };
    auto at = [&](int row, int column) {
        require(editor(row, column)->hasFocus(), "AC arrow did not move editor focus");
        require(view.currentIndex() == view.model()->index(row, column), "AC current cell did not follow arrow");
    };
    focus(0, 3);
    auto* voltage = qobject_cast<QLineEdit*>(editor(0, 3));
    voltage->setText("234.5");
    key(Qt::Key_Right);
    at(0, 4);
    require(vm.inputRows()[0].vin == "234.5", "AC navigation lost voltage edit");
    qobject_cast<QLineEdit*>(editor(0, 4))->setText("55");
    key(Qt::Key_Down);
    at(1, 4);
    require(vm.inputRows()[0].frequency == "55", "AC navigation lost frequency edit");
    key(Qt::Key_Left);
    at(1, 3);
    key(Qt::Key_Up);
    at(0, 3);
    key(Qt::Key_Up);
    at(0, 3);
    focus(2, 5);
    qobject_cast<QLineEdit*>(editor(2, 5))->setText("-90");
    key(Qt::Key_Left);
    at(2, 4);
    require(vm.inputRows()[2].phase == "-90", "AC navigation lost negative phase");
    key(Qt::Key_Down);
    at(2, 4);
    view.setColumnHidden(4, true);
    focus(1, 3);
    key(Qt::Key_Right);
    at(1, 5);
    key(Qt::Key_Right);
    at(1, 5);
    view.setColumnHidden(4, false);
    view.setRowHidden(1, true);
    focus(0, 3);
    key(Qt::Key_Down);
    at(2, 3);
    view.setRowHidden(1, false);
    focus(0, 3);
    voltage->setText("1234");
    voltage->setCursorPosition(2);
    key(Qt::Key_Left, Qt::ShiftModifier);
    require(voltage->hasFocus() && voltage->selectedText() == "2", "AC navigation consumed text-selection shortcut");
    voltage->setText("-");
    key(Qt::Key_Right);
    require(voltage->hasFocus() && vm.inputRows()[0].vin == "234.5", "incomplete AC number was committed or lost focus");
    voltage->setText("240");
    key(Qt::Key_Left);
    at(0, 2);
    require(vm.inputRows()[0].vin == "240", "moving to Mode lost voltage");
    auto* mode = qobject_cast<QComboBox*>(editor(0, 2));
    key(Qt::Key_Down);
    require(mode->hasFocus() && mode->currentText() == "3phase", "Mode arrow selection was hijacked");
    require(vm.inputRows()[0].phaseMode == "3phase", "Mode choice was not committed");
    auto* rows = static_cast<ConditionRowsModel*>(view.model());
    rows->removeRecords({0});
    QApplication::processEvents();
    focus(0, 3);
    key(Qt::Key_Down);
    at(1, 3);
    require(vm.inputRows().size() == 2, "navigation changed row count");
}

void verifyXmlEditorSelection()
{
    TestConditionsModel sourceModel;
    TestConditionsViewModel source(&sourceModel);
    TestConditionsPage sourcePage(&source);
    for (auto kind : {TableKind::Input, TableKind::Dc, TableKind::Relay, TableKind::Load, TableKind::DyLoad}) {
        source.addRow(kind);
        source.addRow(kind);
    }
    auto set = [&](const char *name, int row, int column, const QString &value) {
        auto *table = sourcePage.findChild<QTableView *>(name);
        require(table && table->model()->setData(table->model()->index(row, column), value),
                "XML selection fixture invalid");
    };
    set("acConditionTable", 0, 3, "90");
    set("dcSourceTable", 0, 3, "100");
    set("relayConditionTable", 0, 1, "RL1");
    set("loadConditionTable", 2, 2, "12V");
    set("loadConditionTable", 5, 2, "2");
    set("dynamicConditionTable", 3, 2, "0~1");
    QString xml;
    QXmlStreamWriter writer(&xml);
    source.writeXml(writer);

    TestConditionsModel model;
    TestConditionsViewModel vm(&model);
    TestConditionsPage page(&vm);
    page.resize(1400, 800);
    page.show();
    QXmlStreamReader reader(xml);
    reader.readNextStartElement();
    vm.loadXml(reader);
    require(!reader.hasError(), "selection test XML rejected");
    QApplication::processEvents();
    for (const auto *name : {"acConditionTable", "dcSourceTable", "relayConditionTable",
                             "loadConditionTable", "dynamicConditionTable"}) {
        auto *table = page.findChild<QTableView *>(name);
        int populated = 0;
        for (int row = 0; row < table->model()->rowCount(); ++row)
            for (int column = 0; column < table->model()->columnCount(); ++column)
                if (auto *line = qobject_cast<QLineEdit *>(table->indexWidget(table->model()->index(row, column)))) {
                    if (!line->text().isEmpty()) ++populated;
                    require(!line->hasSelectedText(), "XML load leaves editor text automatically selected");
                }
        require(populated > 0, "selection fixture missed a table");
    }

    auto *load = page.findChild<QTableView *>("loadConditionTable");
    auto *line = qobject_cast<QLineEdit *>(load->indexWidget(load->model()->index(2, 2)));
    require(line && line->text() == "12V", "loaded editor lost text");
    line->setFocus();
    QKeyEvent selectAll(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
    QApplication::sendEvent(line, &selectAll);
    require(line->selectedText() == "12V", "manual text selection broken");
    QMetaObject::invokeMethod(&page, "resetUIFromViewModel", Qt::DirectConnection);
    QApplication::processEvents();
    require(line->selectedText() == "12V", "refresh cleared an existing user's text selection");
    const auto saved = vm.conditions();
    require(saved.inputRows[0].vin == "90" && saved.dcRows[0].vin == "100" &&
                saved.relayRows[0].label == "RL1" && saved.loadRows[0].values[0] == "2" &&
                saved.dynamicRows[0].values[0] == "0~1", "selection cleanup altered loaded values");
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if (argc > 1) {
        const int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        const auto families = QFontDatabase::applicationFontFamilies(font);
        if (!families.isEmpty()) app.setFont(QFont(families.first(), 9));
    }
    try {
        verifyAcKeyboardNavigation();
        verifyDragCopy();
        verifyXmlEditorSelection();
        TestConditionsModel model;
        TestConditionsViewModel vm(&model);
        TestConditionsPage page(&vm);
        page.resize(1400, 800);
        page.show();
        QApplication::processEvents();
        require(page.findChildren<QSplitter*>().isEmpty(), "unexpected condition pane drag handles");
        ManualControlModel controlModel;
        ManualControlOperations operations;
        operations.dcGroup = [](const InstrumentSetupConfig&, const DcGroup&, InputAction) { return InstrumentOperationResult{}; };
        ManualControlViewModel control(&controlModel, operations);
        QObject::connect(&vm, &TestConditionsViewModel::conditionsChanged, &control, &ManualControlViewModel::onConditionsChanged);
        QStringList titles;
        int selected = -1;
        QObject::connect(&control, &ManualControlViewModel::dcInputUpdated,
            [&](int source, const QStringList& values, int index) { if (!source) { titles = values; selected = index; } });
        TestConditionSnapshot initial;
        initial.inputRows = {{"1phase", "110", "60", "0"}};
        initial.dcNames = {"Startup", "Rated load"};
        initial.dcRows = {{"24", "5", ""}, {"30", "6", ""}};
        initial.dcRows2 = {{"12", "3", "aux"}}; // Legacy unequal lengths are padded by the view.
        initial.dcRows3 = {{"5", "1", "logic"}};
        model.setSnapshot(initial);
        QMetaObject::invokeMethod(&page, "resetUIFromViewModel", Qt::DirectConnection);
        auto* table = page.findChild<QTableView*>("dcSourceTable");
        require(table && table->model()->columnCount() == 6 && table->model()->rowCount() == 4, "DC group layout");
        require(table->model()->headerData(5,Qt::Horizontal).toString() == "Index3" && table->rowSpan(0, 0) == 2, "Index headers/sequence span");
        page.syncUIToViewModel();
        require(vm.conditions().dcRows2.size() == 2 && vm.conditions().dcRows2[0].label == "aux", "legacy rows lost");
        require(selected == 0 && titles[0] == "Startup", "group combo title");
        require(table->isColumnHidden(4) && table->isColumnHidden(5), "default DC count must be one");
        InstrumentSetupConfig threeSources;
        threeSources.dcInputs = 3;
        vm.onInstrumentSetupConfigChanged(threeSources);
        page.show();
        QApplication::processEvents();
        for (const auto &name : {"acConditionTable", "dcSourceTable"}) {
            auto *inputs = page.findChild<QTableView*>(name);
            require(inputs && inputs->horizontalScrollBar()->maximum() == 0,
                    "normal page width should show every AC/DC setting, including three DC sources");
        }
        for (int source = 0; source < 3; ++source) {
            auto* voltage = qobject_cast<QLineEdit*>(table->indexWidget(table->model()->index(0, source + 3)));
            auto* current = qobject_cast<QLineEdit*>(table->indexWidget(table->model()->index(1, source + 3)));
            voltage->setText("20"); current->setText("2");
            require(vm.conditions().dcSourceRows(source)[0].vin == "20", "index routing");
            voltage->setFocus();
            QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
            QApplication::sendEvent(voltage, &down);
            require(current->hasFocus(), "voltage/current navigation");
        }
        auto* first = table->indexWidget(table->model()->index(0, 3));
        first->setFocus();
        QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(first, &right);
        require(table->indexWidget(table->model()->index(0, 4))->hasFocus(), "Index navigation");
        qobject_cast<QLineEdit*>(table->indexWidget(table->model()->index(0, 1)))->setText("Main & Aux");
        require(titles[0] == "Main & Aux" && vm.conditions().dcNames[0] == "Main & Aux", "custom DC name propagation");
        QString xml;
        QXmlStreamWriter writer(&xml); model.writeXml(writer);
        TestConditionsModel loaded;
        QXmlStreamReader reader(xml); reader.readNextStartElement(); loaded.loadXml(reader);
        require(!reader.hasError() && loaded.snapshot().dcRows3[0].currentLimit == "2" && loaded.snapshot().dcNames[0] == "Main & Aux", "group XML roundtrip");
        vm.addRow(TableKind::Dc);
        require(table->model()->rowCount() == 6 && vm.conditions().dcRows3.size() == 3, "add group");
        table->selectRow(2);
        vm.removeRow(TableKind::Dc);
        require(table->model()->rowCount() == 4 && vm.conditions().dcRows2.size() == 2, "delete whole group");
        vm.setMaxOutput(2);
        require(vm.conditions().inputRows[0].vin == "110" && vm.conditions().dcRows[0].vin == "20", "load resize erased DC");
        InstrumentSetupConfig config;
        for (int count : {1, 2, 3}) {
            config.dcInputs = count;
            vm.onInstrumentSetupConfigChanged(config);
            for (int source = 0; source < 3; ++source)
                require(table->isColumnHidden(source + 3) == (source >= count), "DC count visibility");
            require(vm.conditions().dcRows3[0].vin == "20", "DC count erased hidden data");
        }
        auto *ac = page.findChild<QTableView*>("acConditionTable");
        auto *relay = page.findChild<QTableView*>("relayConditionTable");
        require(ac && relay,"model-backed AC/Relay tables missing");
        auto *acModel = static_cast<ConditionRowsModel*>(ac->model());
        auto *relayModel = static_cast<ConditionRowsModel*>(relay->model());
        int commits=0;
        QObject::connect(&vm,&TestConditionsViewModel::conditionsChanged,[&]{++commits;});
        require(acModel->setData(acModel->index(0,3),"230"),"AC voltage edit rejected");
        require(commits==1 && vm.inputRows()[0].vin=="230" && acModel->data(acModel->index(0,1)).toString().contains("230"),"AC transaction duplicated or title stale");
        require(acModel->setData(acModel->index(0,5),"-90"),"negative phase regressed");
        commits=0;require(!acModel->setData(acModel->index(0,3),"NaN"),"invalid AC number accepted");
        require(commits==0,"invalid edit published data");
        const auto before=vm.inputRows().size();
        require(acModel->insertRecords(1,{acModel->editableRow(0),acModel->editableRow(0)}),"AC paste failed");
        require(commits==1 && vm.inputRows().size()==before+2,"paste did not commit once");
        acModel->removeRecords({2,1});
        require(vm.inputRows().size()==before,"delete removed wrong records");
        if (relayModel->rowCount()==0) relayModel->append();
        require(relayModel->rowCount()>0 && relayModel->setData(relayModel->index(0,1),"Discharge"),"Relay label edit failed");
        require(relayModel->setData(relayModel->index(0,2),"on") && vm.relayRows()[0].values[0]=="on","Relay index mapping failed");
        require(!relayModel->setData(relayModel->index(0,2),"invalid"),"invalid Relay state accepted");
        QApplication::processEvents();
        auto *draft=qobject_cast<QLineEdit*>(ac->indexWidget(acModel->index(0,3)));
        require(draft,"persistent AC editor missing");
        draft->setText("234.5");
        require(relayModel->setData(relayModel->index(0,1),"Another relay"),"relay rename failed");
        require(draft->text()=="234.5","unrelated Relay change erased AC draft");
        require(acModel->setData(acModel->index(0,4),"50"),"frequency edit failed");
        require(draft->text()=="234.5","same-table unrelated edit erased AC draft");
        draft->setText("230");
        relayModel->setData(relayModel->index(0,1),"Discharge");
        ac->selectRow(0);
        for(auto *shortcut:ac->findChildren<QShortcut*>()) {
            require(shortcut->context()==Qt::WidgetShortcut,"table shortcuts leak to other editors");
            if(shortcut->key()==QKeySequence::Copy) QMetaObject::invokeMethod(shortcut,"activated");
        }
        for(auto *shortcut:ac->findChildren<QShortcut*>())
            if(shortcut->key()==QKeySequence::Paste) QMetaObject::invokeMethod(shortcut,"activated");
        require(acModel->rowCount()==before+1 && vm.inputRows()[1].vin=="230","view copy/paste disconnected");
        acModel->removeRecords({1});
        page.syncUIToViewModel();
        require(vm.inputRows()[0].vin=="230" && vm.relayRows()[0].label=="Discharge","legacy table snapshot overwrote model-backed edits");
        QString updatedXml;QXmlStreamWriter updatedWriter(&updatedXml);model.writeXml(updatedWriter);
        TestConditionsModel restored;QXmlStreamReader updatedReader(updatedXml);updatedReader.readNextStartElement();restored.loadXml(updatedReader);
        require(!updatedReader.hasError() && restored.snapshot().inputRows[0].phase=="-90" && restored.snapshot().relayRows[0].values[0]=="on","AC/Relay XML roundtrip failed");
        for (const auto &name : {"acConditionTable", "dcSourceTable", "relayConditionTable", "loadConditionTable", "dynamicConditionTable"}) {
            auto *selectedTable = page.findChild<QTableView*>(name);
            selectedTable->selectRow(0);
            QApplication::processEvents();
            require(!selectedTable->property("conditionRangeSelection").toBool(), "ordinary selection is blue");
            for (int col = 0; col < selectedTable->model()->columnCount(); ++col)
                if (auto *editor = selectedTable->indexWidget(selectedTable->model()->index(0, col)))
                    require(!editor->property("conditionSelected").toBool(), "ordinary editor selection is blue");
            clickCell(selectedTable, 0);
            require(!selectedTable->property("conditionRangeSelection").toBool(), "click is blue");
            dragRows(selectedTable, 0, 0);
            require(selectedTable->property("conditionRangeSelection").toBool(), "drag does not highlight row");
            int editors = 0;
            for (int col = 0; col < selectedTable->model()->columnCount(); ++col) {
                auto *editor = selectedTable->indexWidget(selectedTable->model()->index(0, col));
                if (!editor) continue;
                ++editors;
                require(editor->property("conditionSelected").toBool(), "selected cell editor hides row highlight");
            }
            require(editors > 0, "selection test has no editors");
            if (selectedTable != ac) {
                clickCell(ac, 0);
                require(!selectedTable->property("conditionRangeSelection").toBool(), "other table retains blue range");
            }
            selectedTable->clearSelection();
            for (int col = 0; col < selectedTable->model()->columnCount(); ++col)
                if (auto *editor = selectedTable->indexWidget(selectedTable->model()->index(0, col)))
                    require(!editor->property("conditionSelected").toBool(), "cleared selection leaves stale highlight");
        }
        vm.setMaxOutput(20);
        vm.setMaxRelayOutput(20);
        for (int row = 0; row < 30; ++row) {
            vm.addRow(TableKind::Load);
            vm.addRow(TableKind::DyLoad);
        }
        QApplication::processEvents();
        for (const auto &name : {"loadConditionTable", "dynamicConditionTable", "relayConditionTable"}) {
            auto *wide = page.findChild<QTableView*>(name);
            require(wide && wide->horizontalScrollBar()->isVisible() && wide->horizontalScrollBar()->maximum() > 0,
                    "many output columns need a horizontal scrollbar");
            require(wide->columnWidth(2) >= (QString(name) == "relayConditionTable" ? 60 : 100), "output columns squeezed below readable width");
            require(wide->horizontalHeader()->sectionResizeMode(2) == QHeaderView::Interactive,
                    "user cannot resize output column");
            wide->horizontalScrollBar()->setValue(wide->horizontalScrollBar()->maximum());
            QApplication::processEvents();
            require(wide->columnViewportPosition(wide->model()->columnCount()-1) < wide->viewport()->width(),
                    "last output column unreachable");
            wide->horizontalScrollBar()->setValue(0);
        }
        for (const auto &name : {"loadConditionTable", "dynamicConditionTable"}) {
            auto *tall = page.findChild<QTableView*>(name);
            require(tall->verticalScrollBar()->isVisible() && tall->verticalScrollBar()->maximum() > 0,
                    "many condition rows need a vertical scrollbar");
            tall->verticalScrollBar()->setValue(tall->verticalScrollBar()->maximum());
            require(tall->rowViewportPosition(tall->model()->rowCount()-1) < tall->viewport()->height(),
                    "last condition row unreachable");
            tall->verticalScrollBar()->setValue(0);
        }
        if (argc > 1) {
            ac->selectRow(0);
            table->selectRow(0);
            page.resize(1400, 800);
            QApplication::processEvents();
            const auto path = QString::fromLocal8Bit(argv[1]);
            page.grab().save(path);
            dragRows(page.findChild<QTableView*>("loadConditionTable"), 5, 7);
            page.grab().save(path + ".drag.png");
        }
        std::cout << "PASS: indexed DC UI, navigation, grouping, ManualControl and XML\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
