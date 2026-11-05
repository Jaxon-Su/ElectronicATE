#include "../../support/test_evidence.h"
#include <QCheckBox>
#include "../../../src/views/testsequence/dialogs/delaydialogs/delaydialog.h"
#include <QLabel>
#include "turnonoffdialog.h"
#include "shorttestdialog.h"
#include "testsequencemodel.h"
#include "../../../src/views/testsequence/dialogs/stressdialogs/staticdialog.h"
#include "../../../src/views/testsequence/dialogs/stressdialogs/dynamicdialog.h"
#include <QApplication>
#include <QFontDatabase>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <stdexcept>
#include <memory>
#include "testsequencetaskdefaults.h"

template<class Factory>
void verifyTriggerSource(Factory factory, const QString& name)
{
    const auto group = TestSequenceTaskKind::settingsGroup(name);
    auto check = [](bool value, const char* error) { if (!value) throw std::runtime_error(error); };
    const auto defaults = testSequenceDefaultSettings(name).value(group).toMap();
    auto fresh = factory(defaults);
    auto* source = fresh->template findChild<QComboBox*>("triggerSource");
    check(source && source->currentData().toInt() == 1, "new task Trigger source must default to CH1");
    check(source->findData(8) >= 0, "CH8 source unavailable");
    source->setCurrentIndex(source->findData(2));
    fresh->template findChild<QPushButton*>("applyBtn")->click();
    check(fresh->config().value("trigger_channel").toInt() == 2, "Apply lost Trigger source");
    TestSequenceModel model; DutRowData row; row.item = name; row.settings = {{group, fresh->config()}}; model.appendTask(row);
    QString xml; QXmlStreamWriter writer(&xml); model.writeXml(writer);
    check(!writer.hasError(), "Trigger source XML save rejected");
    QXmlStreamReader reader(xml); reader.readNextStartElement(); model.loadXml(reader);
    check(!reader.hasError(), "Trigger source XML load rejected");
    const auto saved = model.tasks()[0].settings.value(group).toMap();
    auto reopened = factory(saved);
    check(reopened->template findChild<QComboBox*>("triggerSource")->currentData().toInt() == 2,
          "reopen lost Trigger source");
    for (bool escape : {false, true}) {
        auto cancelled = factory(saved);
        auto* combo = cancelled->template findChild<QComboBox*>("triggerSource");
        combo->setCurrentIndex(combo->findData(4));
        if (escape) { QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier); QApplication::sendEvent(cancelled.get(), &key); }
        else cancelled->template findChild<QPushButton*>("closeBtn")->click();
        check(cancelled->config() == saved, "Close/Escape committed source edit");
    }
    auto legacy = factory(QVariantMap{});
    auto* current = legacy->template findChild<QComboBox*>("triggerSource");
    check(current && !current->currentData().isValid(), "legacy dialog guessed a source");
    legacy->template findChild<QPushButton*>("applyBtn")->click();
    check(!legacy->config().contains("trigger_channel"), "legacy Apply changed source without selection");
    current->setCurrentIndex(current->findData(4));
    legacy->template findChild<QPushButton*>("applyBtn")->click();
    check(legacy->config().value("trigger_channel").toInt() == 4, "legacy explicit selection lost");
    for (const QVariant invalid : {QVariant(true), QVariant("2"), QVariant(9), QVariant(1.5), QVariant{}}) {
        auto bad = factory(QVariantMap{{"trigger_channel", invalid}});
        bad->template findChild<QPushButton*>("applyBtn")->click();
        check(bad->config().contains("trigger_channel") && bad->config().value("trigger_channel").metaType() == invalid.metaType() &&
              bad->config().value("trigger_channel") == invalid, "invalid source silently coerced");
        auto* repaired = bad->template findChild<QComboBox*>("triggerSource");
        repaired->setCurrentIndex(repaired->findData(3));
        bad->template findChild<QPushButton*>("applyBtn")->click();
        check(bad->config().value("trigger_channel").toInt()==3,"invalid source could not be corrected");
    }
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen"); QApplication app(argc,argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/arial.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
    app.setFont(QFont("Arial",10));
    for (bool grouped : {false, true}) {
        const auto selection = grouped ? StaticDialog::Selection::Group : StaticDialog::Selection::Single;
        verifyTriggerSource([&](const QVariantMap& cfg) { return std::make_unique<StaticDialog>(InputConditionOptions{},QStringList{},cfg,1,nullptr,selection); }, grouped ? "Static Group Test" : "Static Test");
        verifyTriggerSource([&](const QVariantMap& cfg) { return std::make_unique<DynamicDialog>(InputConditionOptions{},QStringList{},cfg,1,nullptr,selection); }, grouped ? "Dynamic Group Test" : "Dynamic Test");
        for (auto mode : {TurnOnOffDialog::TurnOn, TurnOnOffDialog::TurnOff}) {
            const QString name = mode == TurnOnOffDialog::TurnOn ? "Turn on" : "Turn off";
            verifyTriggerSource([&](const QVariantMap& cfg) { return std::make_unique<TurnOnOffDialog>(mode,InputConditionOptions{},QStringList{},QVector<RelayOption>{},cfg,1,nullptr,selection); }, name + (grouped ? " Group Test" : ""));
        }
        for (auto mode : {ShortTestDialog::TurnOnThenShort, ShortTestDialog::ShortThenTurnOn}) {
            const QString name = mode == ShortTestDialog::TurnOnThenShort ? "Turn on then short" : "Short then turn on";
            verifyTriggerSource([&](const QVariantMap& cfg) { return std::make_unique<ShortTestDialog>(mode,InputConditionOptions{},QStringList{},QVector<RelayOption>{},cfg,1,nullptr,selection); }, name + (grouped ? " Group Test" : ""));
        }
    }
    DelayDialog cancelled({{"delay_ms",5000}},1);
    cancelled.findChild<QSpinBox*>()->setValue(8000);
    cancelled.findChild<QPushButton*>("closeBtn")->click();
    if (cancelled.result()!=QDialog::Rejected || cancelled.config().value("delay_ms").toInt()!=5000)
        throw std::runtime_error("Delay Close saved changes");
    DelayDialog applied({{"delay_ms",5000}},1);
    applied.findChild<QSpinBox*>()->setValue(8000);
    applied.findChild<QPushButton*>("applyBtn")->click();
    if (applied.result()!=QDialog::Accepted || applied.config().value("delay_ms").toInt()!=8000)
        throw std::runtime_error("Delay Apply lost changes");
    TestSequenceModel model;
    auto save=[&](auto& dialog, const QString& item, const QString& group) {
        auto* target=dialog.template findChild<QComboBox*>("measureTarget");
        auto* edge=dialog.template findChild<QComboBox*>("triggerEdge");
        if(!target || target->currentText()!="BOTH") throw std::runtime_error("target default");
        if (!dialog.template findChild<QLabel*>("searchTolerance")) throw std::runtime_error("automatic tolerance missing");
        dialog.template findChild<QSpinBox*>("trialsPerLevel")->setValue(4);
        if(!target->isEnabled() || edge->isEnabled()) throw std::runtime_error("polarity must be separate and fixed");
        target->setCurrentText("Fall"); edge->setCurrentText("BOTH");
        dialog.template findChild<QPushButton*>("applyBtn")->click();
        DutRowData row; row.item=item;row.settings={{group,dialog.config()}};model.appendTask(row);
    };
    TurnOnOffDialog on(TurnOnOffDialog::TurnOn,{{"input"},{}},{"load"},{},{},1);save(on,"Turn on","turnOn");
    TurnOnOffDialog off(TurnOnOffDialog::TurnOff,{{"input"},{}},{"load"},{},{},1);save(off,"Turn off","turnOff");
    ShortTestDialog after(ShortTestDialog::TurnOnThenShort,{{"input"},{}},{"load"},{},{},1);save(after,"Turn on then short","onShort");
    ShortTestDialog before(ShortTestDialog::ShortThenTurnOn,{{"input"},{}},{"load"},{},{},1);save(before,"Short then turn on","shortOn");
    QString xml;QXmlStreamWriter writer(&xml);model.writeXml(writer);
    TestSequenceModel loaded;QXmlStreamReader reader(xml);reader.readNextStartElement();loaded.loadXml(reader);
    if(loaded.tasks().size()!=4)throw std::runtime_error("missing XML rows");
    for(const auto& row:loaded.tasks())
        if(row.settings.first().toMap().value("measure_target")!="MIN" || row.settings.first().toMap().value("trigger_edge")!="AUTO"
            || row.settings.first().toMap().contains("search_tolerance_v")
            || row.settings.first().toMap().value("trials_per_level").toInt()!=4)
            throw std::runtime_error("XML lost trigger settings");
    for (const QString targetValue : {QString("BOTH"), QString("MAX"), QString("MIN")}) {
        auto verifyTarget = [&](auto& dialog, const QString& item, const QString& group) {
            auto* margin=dialog.template findChild<QCheckBox*>("verticalMargin");
            if (!margin || margin->isChecked()) throw std::runtime_error("vertical margin default checkbox missing");
            margin->click();
            auto* target=dialog.template findChild<QComboBox*>("measureTarget");
            if (!target || !target->isEnabled() || target->count()!=3) throw std::runtime_error("target choices missing");
            if(dialog.template findChild<QComboBox*>("verticalAutoScale")) throw std::runtime_error("removed scale UI visible");
            if(dialog.template findChild<QCheckBox*>("autoPosition")) throw std::runtime_error("removed Auto Position still visible");
            auto* period=dialog.template findChild<QCheckBox*>("autoPeriod");
            if (group=="static"||group=="dynamic") {
                if (!period || period->isChecked()) throw std::runtime_error("Auto Period default missing");
                period->setChecked(true);
            } else if(period) throw std::runtime_error("transient exposes Auto Period");
            target->setCurrentIndex(target->findData(targetValue));
            if (target->currentText() != (targetValue=="MAX"?"Rise":targetValue=="MIN"?"Fall":"BOTH")) throw std::runtime_error("direction label mismatch");
            dialog.template findChild<QPushButton*>("applyBtn")->click();
            if (!dialog.config().value("vertical_margin").toBool()) throw std::runtime_error("vertical margin Apply lost selection");
            if (dialog.config().contains("auto_vertical_scale")) throw std::runtime_error("scale selection lost");
            if (dialog.config().value("measure_target")!=targetValue) throw std::runtime_error("target save failed");
            TestSequenceModel saved; DutRowData row; row.item=item; row.settings={{group,dialog.config()}}; saved.appendTask(row);
            QString xml; QXmlStreamWriter writer(&xml); saved.writeXml(writer);
            TestSequenceModel restored; QXmlStreamReader reader(xml); reader.readNextStartElement(); restored.loadXml(reader);
            if (restored.tasks().size()!=1 || !restored.tasks()[0].settings.value(group).toMap().value("vertical_margin").toBool())
                throw std::runtime_error("vertical margin XML roundtrip failed");
            if(restored.tasks().value(0).settings.value(group).toMap().contains("auto_position"))
                throw std::runtime_error("removed Auto Position serialized");
            if(period && !restored.tasks().value(0).settings.value(group).toMap().value("auto_period").toBool())
                throw std::runtime_error("Auto Period XML roundtrip failed");
            if(restored.tasks().size()!=1 || restored.tasks()[0].settings.value(group).toMap().contains("auto_vertical_scale") || restored.tasks()[0].settings.value(group).toMap().value("measure_target")!=targetValue)
                throw std::runtime_error("target XML roundtrip failed");
        };
        StaticDialog stat({}, {}, {}, 1); verifyTarget(stat,"Static Test","static");
        DynamicDialog dyn({}, {}, {}, 1); verifyTarget(dyn,"Dynamic Test","dynamic");
        TurnOnOffDialog on(TurnOnOffDialog::TurnOn,{},{},{},{},1); verifyTarget(on,"Turn on","turnOn");
        TurnOnOffDialog off(TurnOnOffDialog::TurnOff,{},{},{},{},1); verifyTarget(off,"Turn off","turnOff");
        ShortTestDialog after(ShortTestDialog::TurnOnThenShort,{},{},{},{},1); verifyTarget(after,"Turn on then short","onShort");
        ShortTestDialog before(ShortTestDialog::ShortThenTurnOn,{},{},{},{},1); verifyTarget(before,"Short then turn on","shortOn");
    }
    for(bool enabled : {false,true}) {
        QVariantMap cfg{{"auto_period",enabled},{"measure_target","MIN"},{"vertical_margin",enabled}};
        StaticDialog restoredStatic({},{},cfg,1);
        DynamicDialog restoredDynamic({},{},cfg,1);
        for(QDialog* dialog : {static_cast<QDialog*>(&restoredStatic),static_cast<QDialog*>(&restoredDynamic)}) {
            if(dialog->findChild<QCheckBox*>("autoPosition") ||
               dialog->findChild<QCheckBox*>("autoPeriod")->isChecked()!=enabled ||
               dialog->findChild<QCheckBox*>("verticalMargin")->isChecked()!=enabled ||
               dialog->findChild<QComboBox*>("verticalAutoScale") ||
               dialog->findChild<QComboBox*>("measureTarget")->currentData().toString()!="MIN")
                throw std::runtime_error("saved settings not reflected in reopened dialog");
        }
    }
    // Group uses the same setting; close/reject (including Escape) must not commit edits.
    for (bool enabled : {false,true}) for (bool apply : {false,true}) {
        const QVariantMap cfg{{"vertical_margin",enabled},{"load_indices",QVariantList{0,1}}};
        StaticDialog dialog({}, {"L1","L2"},cfg,1,nullptr,StaticDialog::Selection::Group);
        auto *margin=dialog.findChild<QCheckBox*>("verticalMargin");
        if (!margin || margin->isChecked()!=enabled) throw std::runtime_error("Group margin restore mismatch");
        margin->setChecked(!enabled);
        dialog.findChild<QPushButton*>(apply?"applyBtn":"closeBtn")->click();
        if (dialog.config().value("vertical_margin").toBool()!=(apply?!enabled:enabled))
            throw std::runtime_error("Group Apply/Close margin mismatch");
        TurnOnOffDialog reject(TurnOnOffDialog::TurnOff,{},{},{},{{"vertical_margin",enabled}},1);
        reject.findChild<QCheckBox*>("verticalMargin")->setChecked(!enabled);
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
        QApplication::sendEvent(&reject,&escape);
        if (reject.config().value("vertical_margin").toBool()!=enabled)
            throw std::runtime_error("Rejected transient committed margin edit");
    }
    auto verifyInputs = [](auto& dialog) {
        auto* ac=dialog.template findChild<QComboBox*>("acInputCombo");
        auto* dc=dialog.template findChild<QComboBox*>("dcInputCombo");
        if(!ac||!dc||ac->count()!=2||dc->count()!=2||dc->currentIndex()!=1)
            throw std::runtime_error("AC/DC split or DC restore failed");
        ac->setCurrentIndex(1);
        if(dc->currentIndex()!=1)throw std::runtime_error("AC unexpectedly cleared DC");
        dc->setCurrentIndex(1);
        if(ac->currentIndex()!=1)throw std::runtime_error("DC unexpectedly cleared AC");
        dialog.template findChild<QPushButton*>("applyBtn")->click();
        if(dialog.config().value("ac_input_index").toInt()!=0 || dialog.config().value("dc_input_index").toInt()!=0)throw std::runtime_error("AC/DC saved wrong independent indices");
    };
    const InputConditionOptions inputs{{"1phase/230/50/0"}, {"48V"}};
    const QVariantMap dcCfg{{"dc_input_index",0}};
    StaticDialog staticInput(inputs,{"load"},dcCfg,1);verifyInputs(staticInput);
    DynamicDialog dynamicInput(inputs,{"load"},dcCfg,1);verifyInputs(dynamicInput);
    TurnOnOffDialog onInput(TurnOnOffDialog::TurnOn,inputs,{"load"},{},dcCfg,1);verifyInputs(onInput);
    TurnOnOffDialog offInput(TurnOnOffDialog::TurnOff,inputs,{"load"},{},dcCfg,1);verifyInputs(offInput);
    ShortTestDialog afterInput(ShortTestDialog::TurnOnThenShort,inputs,{"load"},{},dcCfg,1);verifyInputs(afterInput);
    ShortTestDialog beforeInput(ShortTestDialog::ShortThenTurnOn,inputs,{"load"},{},dcCfg,1);verifyInputs(beforeInput);
    auto verifyEmpty = [](auto& dialog) {
        auto* ac=dialog.template findChild<QComboBox*>("acInputCombo");
        auto* dc=dialog.template findChild<QComboBox*>("dcInputCombo");
        if(ac->count()!=1||dc->count()!=1||ac->currentData().toInt()!=-1||dc->currentData().toInt()!=-1)
            throw std::runtime_error("empty input became selectable");
        dialog.template findChild<QPushButton*>("applyBtn")->click();
        const auto cfg=dialog.config();
        if(cfg.value("ac_input_index").toInt()!=-1)
            throw std::runtime_error("empty condition restored instead of None");
    };
    QVariantMap stale{{"ac_input_index",0},{"load_index",0},{"dyload_index",0}};
    StaticDialog emptyStatic({{""},{}},{""},stale,1);verifyEmpty(emptyStatic);
    DynamicDialog emptyDynamic({{""},{}},{""},stale,1);verifyEmpty(emptyDynamic);
    TurnOnOffDialog emptyOn(TurnOnOffDialog::TurnOn,{{""},{}},{""},{},stale,1);verifyEmpty(emptyOn);
    TurnOnOffDialog emptyOff(TurnOnOffDialog::TurnOff,{{""},{}},{""},{},stale,1);verifyEmpty(emptyOff);
    ShortTestDialog emptyShort(ShortTestDialog::ShortThenTurnOn,{{""},{}},{""},{},stale,1);verifyEmpty(emptyShort);
    ShortTestDialog emptyAfter(ShortTestDialog::TurnOnThenShort,{{""},{}},{""},{},stale,1);verifyEmpty(emptyAfter);
    staticInput.findChild<QCheckBox*>("autoPeriod")->setChecked(true);
    staticInput.show();app.processEvents();
    staticInput.grab().save(TestEvidence::path("ac-dc-input.png"));
    for(QDialog* dialog : {static_cast<QDialog*>(&staticInput),static_cast<QDialog*>(&dynamicInput)}) {
        dialog->resize(540,490); dialog->show(); app.processEvents();
        auto* scroll=dialog->findChild<QScrollArea*>();
        if(!scroll || scroll->verticalScrollBar()->maximum()<=0) throw std::runtime_error("compact steady settings cannot scroll");
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); app.processEvents();
        auto* hint=dialog->findChild<QLabel*>("hintLbl");
        const QRect visibleHint(hint->mapTo(scroll->viewport(),QPoint{}),hint->size());
        if(!scroll->viewport()->rect().contains(visibleHint) || !dialog->findChild<QPushButton*>("applyBtn")->isVisible())
            throw std::runtime_error("lower steady settings/footer not accessible");
        dialog->grab().save(TestEvidence::path(dialog==&staticInput?"static-settings-bottom.png":"dynamic-settings-bottom.png"));
        scroll->verticalScrollBar()->setValue(0);
    }
    auto *margin=staticInput.findChild<QCheckBox*>("verticalMargin");
    for (int state=0;state<3;++state) {
        margin->setChecked(state!=0); margin->setEnabled(state!=2); app.processEvents();
        staticInput.grab().save(TestEvidence::path(QString("margin-state-%1.png").arg(state)));
    }
    before.resize(600,800);before.show();app.processEvents();
    before.grab().save(TestEvidence::path("transient-dialog.png"));
}
