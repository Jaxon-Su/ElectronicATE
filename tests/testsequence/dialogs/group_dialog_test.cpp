#include "../../support/test_evidence.h"
#include "staticdialog.h"
#include "dynamicdialog.h"
#include "turnonoffdialog.h"
#include "shorttestdialog.h"
#include "testsequencemodel.h"
#include "testsequencetaskkind.h"
#include "testsequenceleftpanel.h"
#include <QApplication>
#include <QCheckBox>
#include <QSpinBox>
#include <QFontDatabase>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeWidget>
#include <QJsonDocument>
#include <iostream>

void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
template<class Dialog> void check(Dialog &dialog, const QString &name, const QVariantMap &original, bool apply)
{
    const bool dynamic = name == "Dynamic Group Test";
    const auto key = dynamic ? QString("dyload") : QString("load");
    auto *ac = dialog.template findChild<QComboBox*>("acInputCombo");
    auto *dc = dialog.template findChild<QComboBox*>("dcInputCombo");
    require(ac && dc && ac->currentData()==0 && dc->currentData()==0, "group did not restore both inputs");
    ac->setCurrentIndex(0); require(dc->currentData()==0, "clearing AC cleared DC");
    ac->setCurrentIndex(1); dc->setCurrentIndex(0); require(ac->currentData()==0, "clearing DC cleared AC");
    dc->setCurrentIndex(1);
    auto *list = dialog.template findChild<QListWidget*>();
    require(list && list->count() == 29 && list->item(1)->data(Qt::UserRole).toInt() == 2, "blank index shifted");
    require(list->item(0)->checkState() == Qt::Checked && list->item(1)->checkState() == Qt::Checked, "load restore failed");
    list->item(0)->setCheckState(Qt::Unchecked); list->item(2)->setCheckState(Qt::Checked);
    require(dialog.windowTitle().contains("Group"), "group title missing");
    dialog.show(); QApplication::processEvents();
    require(list->verticalScrollBar()->maximum() > 0 && dialog.height() <= 900, "unbounded group layout");
    auto *period = dialog.template findChild<QCheckBox*>("autoPeriod");
    require(bool(period) == TestSequenceTaskKind::isSteady(TestSequenceTaskKind::fromName(name)), "period in wrong dialog");
    if (period) {
        auto *off = dialog.template findChild<QSpinBox*>("power_off_ms");
        auto *startup = dialog.template findChild<QSpinBox*>("startup_ms");
        require(off && startup && off->value()==5000 && startup->value()==5000, "missing/default transition waits");
        off->setValue(2300); startup->setValue(4700);
    }
    if (apply) {
        const auto path = TestEvidence::path(QString("group/%1.png").arg(TestSequenceTaskKind::settingsGroup(name)));
        require(dialog.grab().save(path), "screenshot failed");
    }
    dialog.template findChild<QPushButton*>(apply ? "applyBtn" : "closeBtn")->click();
    if (!apply) {
        require(dialog.result() == QDialog::Rejected && dialog.config() == original, "Close changed settings"); return;
    }
    require(dialog.result() == QDialog::Accepted && dialog.config()[key + "_indices"].toList() == QVariantList({2, 3}), "Apply lost multiselect");
    if (period) require(dialog.config()["power_off_ms"].toInt()==2300 && dialog.config()["startup_ms"].toInt()==4700, "Apply lost transition waits");
    require(dialog.config()["ac_input_index"]==0 && dialog.config()["dc_input_index"]==0, "group Apply lost one input");
    TestSequenceModel model; DutRowData row; row.item = name;
    row.settings = {{TestSequenceTaskKind::settingsGroup(name), dialog.config()}}; model.appendTask(row);
    QString xml; QXmlStreamWriter out(&xml); model.writeXml(out);
    TestSequenceModel restored; QXmlStreamReader in(xml); in.readNextStartElement(); restored.loadXml(in);
    require(!in.hasError() && restored.tasks().size() == 1 &&
        QJsonDocument::fromVariant(restored.tasks()[0].settings) == QJsonDocument::fromVariant(row.settings), "dialog XML mismatch");
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
    try {
        QStringList loads{"L1", "", "L3", "L4"};
        for (int i = 5; i <= 30; ++i) loads << QString("Load %1").arg(i);
        const InputConditionOptions inputs{{"1phase/90/47/0"}, {"DC1"}};
        const QVector<RelayOption> relays{{"Short", {"ON", "OFF"}}, {"Discharge", {"OFF", "ON"}}};
        using Selection = LoadConditionSelector::Selection;
        for (bool apply : {false, true}) {
            QVariantMap cfg{{"ac_input_index", 0}, {"dc_input_index", 0}, {"load_indices", QVariantList{0, 2}}, {"relay_index", 0}, {"discharge_index", 1}};
            auto basic = cfg; basic.remove("relay_index"); basic.remove("discharge_index");
            StaticDialog st(inputs, loads, basic, 1, nullptr, Selection::Group); check(st, "Static Group Test", basic, apply);
            auto dy = basic; dy.remove("load_indices"); dy["dyload_indices"] = QVariantList{0, 2};
            DynamicDialog dynamic(inputs, loads, dy, 1, nullptr, Selection::Group); check(dynamic, "Dynamic Group Test", dy, apply);
            auto onCfg = cfg; onCfg.remove("relay_index");
            TurnOnOffDialog on(TurnOnOffDialog::TurnOn, inputs, loads, relays, onCfg, 1, nullptr, Selection::Group);
            check(on, "Turn on Group Test", onCfg, apply);
            TurnOnOffDialog off(TurnOnOffDialog::TurnOff, inputs, loads, relays, basic, 1, nullptr, Selection::Group);
            check(off, "Turn off Group Test", basic, apply);
            ShortTestDialog first(ShortTestDialog::ShortThenTurnOn, inputs, loads, relays, cfg, 1, nullptr, Selection::Group);
            check(first, "Short then turn on Group Test", cfg, apply);
            ShortTestDialog last(ShortTestDialog::TurnOnThenShort, inputs, loads, relays, cfg, 1, nullptr, Selection::Group);
            check(last, "Turn on then short Group Test", cfg, apply);
        }
        TestSequenceLeftPanel panel;
        const auto names = panel.taskNames();
        require(names.size() == 18 && names.contains("Brown Out") &&
                names.contains("Turn off (descent)") && names.contains("Turn on then short (descent)") && !names.contains("Capture"),
                "catalog includes removed Capture or is missing entries");
        for (const auto &descriptor : TestSequenceTaskKind::descriptors)
            require(names.count(descriptor.name) == 1, "ambiguous task identifier");
        auto *tree = panel.findChild<QTreeWidget*>("stressTaskTree");
        require(tree && tree->topLevelItem(0)->text(0) == "Single Test" && tree->topLevelItem(1)->text(0) == "Group Test", "group section missing");
        require(tree->topLevelItem(0)->childCount()==8 && tree->topLevelItem(1)->childCount()==6,"descent not confined to Single Test");
        QString selected;
        QObject::connect(&panel, &TestSequenceLeftPanel::taskDoubleClicked, [&](const QString &name) { selected = name; });
        for (int i = 0; i < 6; ++i) {
            auto *item = tree->topLevelItem(1)->child(i);
            tree->itemDoubleClicked(item, 0);
            require(TestSequenceTaskKind::isGroup(TestSequenceTaskKind::fromName(selected)), "group displayed base title routed to single");
        }
        require(tree->findItems("Brown Out", Qt::MatchExactly | Qt::MatchRecursive).isEmpty(),
                "Brown Out still appears in Stress Test");
        auto* dellHp = panel.findChild<QTreeWidget*>("dellHpTaskTree");
        auto* selector = panel.findChild<QComboBox*>();
        require(dellHp && selector && dellHp->topLevelItemCount() == 1 &&
                dellHp->topLevelItem(0)->childCount() == 1, "Dell/HP still contains placeholders");
        selector->setCurrentIndex(selector->findText("Dell/HP Test"));
        require(panel.findChild<QStackedWidget*>()->currentWidget() == dellHp, "Dell/HP selector did not switch tree");
        auto* brownOut = dellHp->topLevelItem(0)->child(0);
        dellHp->itemDoubleClicked(brownOut, 0);
        require(selected == "Brown Out" && names.contains(selected), "Dell/HP task cannot be added or selected in the center panel");
        panel.setLocked(true); selected.clear(); dellHp->itemDoubleClicked(brownOut, 0);
        require(selected.isEmpty(), "Dell/HP bypasses task execution lock");
        std::cout << "PASS six group dialogs and catalog";
    } catch (const std::exception &e) { std::cerr << e.what(); return 1; }
}
