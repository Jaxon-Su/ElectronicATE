#include "brownoutdialog.h"
#include <QApplication>
#include <QTimer>
#include <QScrollBar>
#include <QFontDatabase>
#include <iostream>
void expect(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char**argv){QApplication app(argc,argv);try{
    if (argc > 1) {
        const QString scenario = argv[1];
        CaptureDialog dialog({{"directory", "captures"}}, nullptr);
        if (scenario == "capture_defaults") {
            const CaptureOptions actual(dialog.config());
            expect(actual.formats.contains("WFM") && actual.formats.contains("PNG"), "partial Capture settings lose runtime default formats");
        } else {
            for (auto *box : dialog.findChildren<QCheckBox*>()) box->setChecked(false);
            expect(!dialog.findChild<QPushButton*>("applyBtn")->isEnabled(), "Capture accepts no formats");
            dialog.findChildren<QCheckBox*>().first()->setChecked(true);
            expect(dialog.findChild<QPushButton*>("applyBtn")->isEnabled(), "Capture rejects selected format");
        }
        return 0;
    }
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
    auto v=BrownOut::defaults();v["ac_input_index"]=0;v["load_index"]=2;
    BrownOutDialog dialog({{"90 V",""},{"48 V"}},{"L1","","L3"},v,1);
    expect(dialog.findChild<QDoubleSpinBox*>("voltage_step")->value()==0.5,"Step down default is not 0.5 V");
    for(const auto &rule:BrownOut::rules)
        expect(dialog.findChild<QDoubleSpinBox*>(rule.key)->decimals()==(rule.integer?0:4),"wrong TestSequence decimal precision");
    auto *step=dialog.findChild<QDoubleSpinBox*>("voltage_step");
    expect(step->cleanText()=="0.5","unnecessary decimal zeros still displayed");
    step->setValue(0.1234);
    expect(step->cleanText()=="0.1234"&&dialog.config()["voltage_step"].toDouble()==0.1234,"fourth decimal lost");
    step->setValue(0.12346);
    expect(step->value()==0.1235,"fifth decimal not rounded");
    step->setLocale(QLocale(QLocale::German,QLocale::Germany));
    step->setValue(0.5);
    expect(step->cleanText()=="0,5","locale decimal separator lost");
    step->setLocale(QLocale::c());
    for(auto *label:dialog.findChildren<QLabel*>())
        expect(!label->text().contains("Vout monitor:")&&!label->text().contains("Uses the oscilloscope"),"removed footer still visible");
    for(const auto &key:{"minimum_vin","timeout_ms","time_per_div","trigger_position"}) {
        expect(!dialog.findChild<QDoubleSpinBox*>(key),"removed field still has an editor");
        expect(!dialog.config().contains(key),"removed field still saved by dialog");
    }
    auto*settle=dialog.findChild<QDoubleSpinBox*>("delay_ms");
    auto*conditions=settle?qobject_cast<QGroupBox*>(settle->parentWidget()):nullptr;
    expect(conditions&&conditions->title()=="Input / Load","Initial settle is not in Input / Load");
    settle->setValue(3000);
    expect(dialog.config()["delay_ms"].toInt()==3000,"Initial settle value did not persist");
    auto*mode=dialog.findChild<QComboBox*>("brownOutMode");
    auto*direction=dialog.findChild<QComboBox*>("brownOutDirection");
    expect(mode&&direction&&direction->isHidden(),"fixed Fall direction should be hidden");
    expect(dialog.findChild<QDoubleSpinBox*>("trigger_level")->suffix()==" V","Fixed Fall did not show voltage units");
    expect(dialog.findChild<QDoubleSpinBox*>("dc_source")->isHidden(),"AC mode still shows a DC source selector");
    const auto*minimumVin=dialog.findChild<QLabel*>("minimumVinSummary");
    expect(minimumVin&&minimumVin->text()=="0 V (fixed)","fixed minimum voltage summary missing");
    auto*advanced=dialog.findChild<QGroupBox*>("advancedSettings");
    auto*toggle=dialog.findChild<QToolButton*>("advancedToggle");
    auto *ignore=dialog.findChild<QCheckBox*>("ignoreAdvanced");
    expect(ignore&&ignore->isChecked(),"Advanced Ignore should be checked by default");
    for(const auto &key:{"tail_ms","data_timeout_ms","shutdown_confirm_ms"})
        expect(!dialog.findChild<QDoubleSpinBox*>(key)->isEnabled(),"default Ignore left custom timing enabled");
    BrownOutDialog emptyDraft({}, {}, {}, 2);
    expect(emptyDraft.findChild<QCheckBox*>("ignoreAdvanced")->isChecked(),"empty draft lost default Ignore");
    expect(advanced&&toggle&&advanced->isHidden(),"advanced settings not collapsed initially");
    expect(dialog.findChild<QDoubleSpinBox*>("data_timeout_ms")->parentWidget()==advanced&&
           dialog.findChild<QDoubleSpinBox*>("shutdown_confirm_ms")->parentWidget()==advanced&&
           dialog.findChild<QDoubleSpinBox*>("tail_ms")->parentWidget()==advanced,"advanced timing fields are not grouped");
    dialog.show();app.processEvents();dialog.grab().save("brownout-dialog-fixed.png");
    mode->setCurrentIndex(1);expect(!direction->isHidden(),"Track direction unavailable");
    expect(dialog.findChild<QDoubleSpinBox*>("trigger_level")->suffix()==" V / A","Track units changed");
    direction->setCurrentIndex(1);expect(dialog.config()["direction"]=="RISE","Rise serialization wrong");
    expect(dialog.config()["load_index"]==2,"filtered load index changed");
    auto*dc=dialog.findChild<QComboBox*>("dcInputCombo");dc->setCurrentIndex(1);
    expect(dialog.config()["ac_input_index"]==0 && dialog.config()["dc_input_index"]==0,"DC cleared AC selection");
    auto *ramp=dialog.findChild<QComboBox*>("rampSource");
    expect(ramp && ramp->currentData()=="AC", "adding DC changed selected AC ramp");
    ramp->setCurrentIndex(ramp->findData("DC"));
    expect(dialog.config()["ramp_source"]=="DC"&&!dialog.findChild<QDoubleSpinBox*>("dc_source")->isHidden(),"DC ramp selector wrong");
    expect(!dialog.config().value("capture_enabled").toBool()&&!dialog.config().contains("auto_period"),"unrequested feature enabled");
    auto *capture=dialog.findChild<QCheckBox*>("captureEnabled");
    expect(capture,"strategy capture checkbox missing");
    capture->setChecked(true);
    expect(BrownOut::Settings(dialog.config()).captureEnabled,"Brown Out Capture config not saved");
    dialog.show();app.processEvents();dialog.grab().save("brownout-dialog.png");
    QTimer::singleShot(0,[]{
        auto *captureDialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        expect(captureDialog,"Capture settings dialog did not open");
        captureDialog->grab().save("strategy-capture-dialog.png");
        captureDialog->reject();
    });
    for(auto *button:dialog.findChildren<QPushButton*>())
        if(button->text()=="Capture settings…") {button->click();break;}

    toggle->click();expect(!advanced->isHidden(),"advanced timing cannot be expanded");
    ignore->setChecked(false);
    dialog.findChild<QDoubleSpinBox*>("tail_ms")->setValue(2000);
    dialog.findChild<QDoubleSpinBox*>("data_timeout_ms")->setValue(20000);
    dialog.findChild<QDoubleSpinBox*>("shutdown_confirm_ms")->setValue(1500);
    ignore->setChecked(true);
    for(const auto &key:{"tail_ms","data_timeout_ms","shutdown_confirm_ms"})
        expect(!dialog.findChild<QDoubleSpinBox*>(key)->isEnabled(),"Ignore did not disable custom timing");
    const BrownOut::Settings ignored(dialog.config());
    expect(ignored.dataTimeoutMs==10000&&ignored.confirmMs==500&&ignored.tailMs==1000,"Ignore did not use built-in defaults");
    expect(dialog.config()["tail_ms"].toInt()==2000&&dialog.config()["ignore_advanced"].toBool(),"Ignore lost saved custom timing");
    BrownOutDialog reopened({{"90 V",""},{"48 V"}},{"L1","","L3"},dialog.config(),1);
    expect(reopened.config()["ac_input_index"]==0 && reopened.config()["dc_input_index"]==0 && reopened.config()["ramp_source"]=="DC", "dual input/ramp restore failed");
    auto *reopenedIgnore=reopened.findChild<QCheckBox*>("ignoreAdvanced");
    expect(reopenedIgnore->isChecked()&&!reopened.findChild<QDoubleSpinBox*>("tail_ms")->isEnabled(),"Ignore restore failed");
    reopenedIgnore->setChecked(false);
    const BrownOut::Settings custom(reopened.config());
    expect(custom.dataTimeoutMs==20000&&custom.confirmMs==1500&&custom.tailMs==2000,"custom timing did not restore");
    ignore->setChecked(false);
    expect(dialog.findChild<QDoubleSpinBox*>("tail_ms")->isEnabled(),"Ignore toggle left timing locked");
    BrownOutDialog reopenedCustom({}, {}, dialog.config(), 3);
    expect(!reopenedCustom.findChild<QCheckBox*>("ignoreAdvanced")->isChecked()&&
           reopenedCustom.findChild<QDoubleSpinBox*>("tail_ms")->isEnabled(),"explicit custom choice was overwritten by default");
    app.processEvents();
    auto*scroll=dialog.findChild<QScrollArea*>();scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    app.processEvents();dialog.grab().save("brownout-dialog-monitor.png");
    ignore->grab().save("brownout-ignore-unchecked.png");
    ignore->setChecked(true);app.processEvents();
    ignore->grab().save("brownout-ignore-checked.png");
    dialog.grab().save("brownout-dialog-ignore-checked.png");
    toggle->click();expect(advanced->isHidden()&&dialog.config()["tail_ms"].toInt()==2000,"collapsing advanced settings lost values");
    mode->setCurrentIndex(0);expect(direction->isHidden()&&dialog.config()["direction"]=="FALL","Fixed Fall retained Rise direction");
    dialog.reject();expect(dialog.result()==QDialog::Rejected,"Close committed settings");
    return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
