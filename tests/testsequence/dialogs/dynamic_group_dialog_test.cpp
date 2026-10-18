#include "../../support/test_evidence.h"
#include "dynamicdialog.h"
#include "testsequenceconditionmapping.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollBar>
#include <QFontDatabase>
#include <QLabel>
#include <iostream>

void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
    try {
        QStringList loads{"DY1", "", "DY3", "DY4"};
        for (int i = 5; i <= 30; ++i) loads << QString("DY%1").arg(i);
        const InputConditionOptions inputs{{"1phase/90/47/0"}, {"DC1"}};
        const QVariantMap original{{"ac_input_index", 0}, {"dc_input_index", 0}, {"dyload_indices", QVariantList{0, 2}},
                                   {"measure_target", "MIN"}, {"auto_period", true}, {"delay_ms", 1000}};
        DynamicDialog dialog(inputs, loads, original, 3, nullptr, DynamicDialog::Selection::Group);
        auto *list = dialog.findChild<QListWidget*>("dynamicConditions");
        require(list && list->count() == 29 && list->item(1)->data(Qt::UserRole).toInt() == 2, "blank row shifted indices");
        require(list->item(0)->checkState() == Qt::Checked && list->item(1)->checkState() == Qt::Checked, "selection restore failed");
        require(dialog.findChild<QComboBox*>("dcInputCombo")->currentData().toInt() == 0, "DC restore failed");
        require(dialog.findChild<QComboBox*>("measureTarget")->currentData().toString() == "MIN", "direction restore failed");
        list->item(0)->setCheckState(Qt::Unchecked);
        list->item(2)->setCheckState(Qt::Checked);
        dialog.findChild<QPushButton*>("applyBtn")->click();
        const auto saved = dialog.config();
        require(dialog.result() == QDialog::Accepted && saved["dyload_indices"].toList() == QVariantList({2, 3}), "Apply failed");
        require(saved["dyload_labels"].toStringList() == QStringList({"DY3", "DY4"}), "labels not saved");
        require(!saved["capture_enabled"].toBool() && saved["capture_settings"].toMap()["formats"].toStringList() == QStringList({"WFM", "PNG"}), "Capture defaults changed");
        DynamicDialog cancelled(inputs, loads, original, 3, nullptr, DynamicDialog::Selection::Group);
        cancelled.findChild<QListWidget*>("dynamicConditions")->item(0)->setCheckState(Qt::Unchecked);
        cancelled.findChild<QPushButton*>("closeBtn")->click();
        require(cancelled.result() == QDialog::Rejected && cancelled.config() == original, "Close saved unaccepted edits");
        DynamicDialog single(inputs, loads, {{"ac_input_index", 0}, {"dyload_index", 2}}, 1);
        require(!single.findChild<QListWidget*>(), "single Dynamic changed to group");
        single.findChild<QPushButton*>("applyBtn")->click();
        require(single.config()["dyload_index"].toInt() == 2 && !single.config().contains("dyload_indices"), "single-condition setting changed");
        dialog.show(); app.processEvents();
        require(list->verticalScrollBar()->maximum() > 0 && dialog.height() < 900, "group list not bounded/scrollable");
        dialog.grab().save(TestEvidence::path("dynamic-group-dialog.png"));
        for (auto *label : dialog.findChildren<QLabel*>("previewLbl"))
            if (label->text().contains("selected")) {
                require(list->geometry().bottom() < label->geometry().top(), "condition list overlaps count/next row");
            }
        std::cout << "PASS dynamic group dialog\n";
    } catch (const std::exception &error) { std::cerr << error.what(); return 1; }
}
