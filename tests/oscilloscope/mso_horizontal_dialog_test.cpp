#include "msoseries456writedialog.h"
#include "testsequencemodel.h"
#include "oscilloscopesettingrules.h"
#include "horizontalsettingslimits.h"
#include "oscilloscopeconfiguration.h"
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>
#include <QTest>
#include <QFontDatabase>
#include <QScrollArea>
#include <QScrollBar>
#include <iostream>
#include <stdexcept>

void check(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
QComboBox *combo(MSOSeries456WriteDialog &d,const char *name) {
    auto *c=d.findChild<QComboBox*>(name);check(c,"missing horizontal input");return c;
}
QLineEdit *estimate(MSOSeries456WriteDialog &d,const char *name) {
    auto *e=d.findChild<QLineEdit*>(name);check(e && e->isReadOnly(),"estimate is editable");return e;
}
bool canApply(MSOSeries456WriteDialog &d) {return d.findChild<QPushButton*>("applyBtn")->isEnabled();}
void enter(MSOSeries456WriteDialog &d,const char *name,const char *number,const char *unit) {
    const QByteArray unitName=QByteArray(name)+"Unit";
    auto *u=combo(d,unitName.constData());
    check(u->findText(unit)>=0,"missing unit");u->setCurrentIndex(u->findText(unit));
    auto *c=combo(d,name);check(c->isEnabled(),"input disabled");c->lineEdit()->selectAll();
    QTest::keyClicks(c->lineEdit(),number);QTest::keyClick(c->lineEdit(),Qt::Key_Tab);
    check(c->currentText()==number,"custom number replaced on focus loss");
}
void manual(MSOSeries456WriteDialog &d,int pair=0) {
    combo(d,"horizontalMode")->setCurrentIndex(1);combo(d,"horizontalInputs")->setCurrentIndex(pair);
}
QVariantMap saveRoundtrip(MSOSeries456WriteDialog &d) {
    check(canApply(d),"valid settings rejected");d.findChild<QPushButton*>("applyBtn")->click();
    check(d.result()==QDialog::Accepted,"Apply rejected");
    TestSequenceModel model;DutRowData row;row.item="Write Oscilloscope";row.settings={{"osc",d.config()}};model.appendTask(row);
    QString xml;QXmlStreamWriter writer(&xml);model.writeXml(writer);
    TestSequenceModel restored;QXmlStreamReader reader(xml);reader.readNextStartElement();restored.loadXml(reader);
    check(!reader.hasError()&&restored.tasks().size()==1,"XML roundtrip failed");
    return restored.tasks()[0].settings.value("osc").toMap();
}
double parsed(const QVariantMap &cfg,const char *key,const char *kind) {
    auto value=OscilloscopeSettingRules::quantity(cfg[key].toString(),kind);check(bool(value),"saved number is invalid");return *value;
}
bool near(double a,double b) {return std::abs(a-b)<=std::abs(b)*1e-12;}
void reviewScreenshot(MSOSeries456WriteDialog &dialog, const QString &name) {
    const auto folder = qEnvironmentVariable("ATE_DIALOG_REVIEW_SCREENSHOTS");
    if (folder.isEmpty()) return;
    QDir().mkpath(folder); dialog.resize(900, 940); dialog.show(); QApplication::processEvents();
    auto *scroll = dialog.findChild<QScrollArea*>(); scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    QApplication::processEvents(); check(dialog.grab().save(QDir(folder).filePath(name + ".png")), "screenshot failed");
}
OscilloscopeSettings decode(const QVariantMap &cfg, int channels = 4) {
    OscilloscopeSettings result; QString error;
    check(prepareOscilloscopeSettings(cfg, "MSOSeries456", channels, result, error), qPrintable(error));
    return result;
}
void scopeReview(const QString &scenario) {
    if (scenario == "scope_tokens") {
        for (const QString edge : {"FALL", "BOTH", "EITHER", "falling"}) {
            QVariantMap cfg{{"acq_mode", "hires"}, {"trig_edge", edge}, {"trig_source", "AUXILIARY"},
                            {"ch1_enabled", true}, {"ch1_coupling", "ac"}, {"ch1_bandwidth", "37MHz"}};
            MSOSeries456WriteDialog dialog(nullptr, "MSO44B", cfg, 1, "");
            const auto restored = decode(saveRoundtrip(dialog));
            check(restored.acquisition.mode == "HIRES", "Apply changed acquisition mode");
            check(restored.trigger.slope == (edge.startsWith("F", Qt::CaseInsensitive) ? "FALL" : "EITHER"), "Apply changed trigger slope");
            check(restored.trigger.source == "AUX" || restored.trigger.source == "AUXILIARY", "Apply changed trigger source");
            check(restored.channels[0].coupling == "AC" && restored.channels[0].bandwidthHz == 37e6, "Apply changed channel coupling/bandwidth");
        }
    } else if (scenario == "scope_ignore") {
        MSOSeries456WriteDialog dialog(nullptr, "MSO44B", {{"ignore_basic", true}, {"timescale", "20us"}, {"acq_mode", "HIRes"}}, 1, "");
        check(!combo(dialog, "horizontalMode")->isEnabled(), "hidden ignore_basic leaves editor enabled");
        auto cfg = saveRoundtrip(dialog); auto settings = decode(cfg);
        check(!settings.horizontal.mode && !settings.acquisition.mode, "Apply lost basic Ignore behavior");
        MSOSeries456WriteDialog reopened(nullptr, "MSO44B", cfg, 1, "");
        for (auto *box : reopened.findChildren<QCheckBox*>())
            if (box->text() == "Ignore") box->setChecked(false);
        settings = decode(saveRoundtrip(reopened));
        check(settings.horizontal.secondsPerDivision == 20e-6 && settings.acquisition.mode == "HIRES", "visible Ignore cannot re-enable basic settings");
        reviewScreenshot(reopened, scenario);
    } else if (scenario == "scope_channels") {
        for (const QString model : {"MSO44B", "MSO46B", "MSO58B"}) {
            const int count = model == "MSO44B" ? 4 : model == "MSO46B" ? 6 : 8;
            const QString prefix = QString("ch%1_").arg(count);
            MSOSeries456WriteDialog dialog(nullptr, model, {{prefix+"enabled", true}, {prefix+"scale", "2A"}, {"trig_source", QString("CH%1").arg(count)}}, 1, "");
            auto *scale = dialog.findChild<QLineEdit*>(prefix+"customScale");
            if (!scale) for (auto *edit : dialog.findChildren<QLineEdit*>())
                if (edit->placeholderText() == "e.g. 30V / 2A" && edit->text() == "2A") scale = edit;
            check(scale, "supported channel has no editor"); scale->setText("3.75A");
            const auto settings = decode(saveRoundtrip(dialog), count);
            check(settings.channels.size() == count && settings.channels.last().scale == 3.75, "last channel edit not executed");
            check(settings.trigger.source == QString("CH%1").arg(count), "last channel trigger not preserved");
            reviewScreenshot(dialog, model);
        }
        MSOSeries456WriteDialog unsupported(nullptr, "MSO44B", {{"trig_source", "CH8"}}, 1, "");
        check(!canApply(unsupported), "unavailable saved trigger silently retargeted");
    } else if (scenario == "scope_scale") {
        MSOSeries456WriteDialog dialog(nullptr, "MSO44B", {{"ch1_enabled", true}, {"ch1_scale", "2A"}}, 1, "");
        QLineEdit *custom = nullptr;
        for (auto *edit : dialog.findChildren<QLineEdit*>()) if (edit->placeholderText() == "e.g. 30V / 2A" && edit->text() == "2A") custom = edit;
        check(custom, "custom scale missing");
        for (const QString bad : {"", "bad", "0V", "-1A"}) {
            custom->setText(bad); check(!canApply(dialog), "invalid channel scale can be applied");
        }
        reviewScreenshot(dialog, "invalid-scale");
        custom->setText("2.5A"); check(decode(saveRoundtrip(dialog)).channels[0].scale == 2.5, "valid custom scale lost");
        MSOSeries456WriteDialog ignored(nullptr, "MSO44B", {{"ignore_channels", true}, {"ch1_enabled", true}, {"ch1_scale", "2A"}, {"ch1_bandwidth", "37MHz"}}, 1, "");
        ignored.findChild<QLineEdit*>("ch1_customScale")->setText("bad");
        ignored.findChild<QComboBox*>("ch1_bandwidth")->setEditText("bad");
        const auto saved = saveRoundtrip(ignored);
        check(saved.value("ch1_scale") == "2A" && saved.value("ch1_bandwidth") == "37MHz", "ignored invalid draft corrupted saved channel");
        MSOSeries456WriteDialog bandwidth(nullptr, "MSO44B", {}, 1, "");
        bandwidth.findChild<QComboBox*>("ch1_bandwidth")->setEditText("bad");
        check(!canApply(bandwidth), "invalid active bandwidth accepted");
    }
}
void pairs() {
    for(bool slow:{false,true}) for(int pair:{0,1}) for(bool rateFirst:{false,true}) {
        MSOSeries456WriteDialog d(nullptr,"MSO44B",{},1,"");manual(d,pair);
        auto setRate=[&]{enter(d,"horizontalRate",slow?"625":"1.25",slow?"MS/s":"GS/s");};
        auto setOther=[&]{if(pair==0) enter(d,"horizontalTime",slow?"100":"16","us/div");
            else enter(d,"horizontalRecordLength",slow?"625":"200","k points");};
        if(rateFirst) {setRate();setOther();} else {setOther();setRate();}
        check(canApply(d),"user pair rejected");
        const auto cfg=saveRoundtrip(d);
        check(near(parsed(cfg,"timescale","time"),slow?100e-6:16e-6)&&
              near(parsed(cfg,"sample_rate","rate"),slow?625e6:1.25e9)&&
              parsed(cfg,"record_length","count")== (slow?625000:200000),"pair/order changed saved targets");
        check(cfg["horz_limits"]=="MSO44B","range profile lost");
        MSOSeries456WriteDialog restored(nullptr,"MSO44B",cfg,1,"");
        check(combo(restored,"horizontalInputs")->currentIndex()==pair&&canApply(restored),"reopen failed");
        const auto folder=qEnvironmentVariable("ATE_HORIZONTAL_SCREENSHOTS");
        if(!folder.isEmpty()&&rateFirst) {QDir().mkpath(folder);d.resize(820,940);d.show();QApplication::processEvents();
            check(d.grab().save(QDir(folder).filePath(QString("pair-%1-%2.png").arg(slow).arg(pair))),"screenshot failed");}
    }
}
void switchingAndCustom() {
    QVariantMap cfg{{"horz_mode","MANUAL"},{"horz_config","RECORDLENGTH"},
        {"timescale","37us"},{"sample_rate","2.73MS/s"},{"record_length","12345"},{"horz_pos",23.5}};
    MSOSeries456WriteDialog d(nullptr,"MSO44B",cfg,1,"");
    check(combo(d,"horizontalMode")->count()==2&&combo(d,"horizontalMode")->currentIndex()==1,"mode mapping");
    check(combo(d,"horizontalTime")->currentText()=="37"&&combo(d,"horizontalRate")->currentText()=="2.73","custom values lost");
    check(estimate(d,"estimatedRecordLength")->text()=="1010 points (Calculated)","rounded preview wrong");
    // The newly editable field starts at the current calculated value, not stale 12345.
    combo(d,"horizontalInputs")->setCurrentIndex(1);
    check(combo(d,"horizontalRecordLength")->currentText()=="1.01","pair switch did not carry preview");
    enter(d,"horizontalRecordLength","1001","points");
    check(canApply(d),"single-sample count rejected");
    combo(d,"horizontalMode")->setCurrentIndex(0);
    check(!combo(d,"horizontalInputs")->isEnabled()&&estimate(d,"estimatedRate")->text()=="Automatic","Auto ownership");
    combo(d,"horizontalMode")->setCurrentIndex(1);
    check(combo(d,"horizontalRecordLength")->currentText()=="1001","Auto destroyed manual input");
    const auto saved=saveRoundtrip(d);check(saved["horz_pos"]==23.5&&parsed(saved,"record_length","count")==1001,"saved custom values");
    MSOSeries456WriteDialog restored(nullptr,"MSO44B",saved,1,"");check(canApply(restored),"1001 points in k units rejected");
    MSOSeries456WriteDialog legacy(nullptr,"MSO44B",{{"horz_mode","MANUAL"}},1,"");
    check(combo(legacy,"horizontalInputs")->currentIndex()==1,"missing CONFIGure default changed");
    MSOSeries456WriteDialog close(nullptr,"MSO44B",cfg,1,"");manual(close);enter(close,"horizontalTime","50","us/div");close.reject();
    check(close.config()==cfg,"Cancel committed draft edits");
}
void invalidAndLimits() {
    MSOSeries456WriteDialog d(nullptr,"MSO44B",{},1,"");manual(d);
    for(const char *bad:{"","bad","625M","-1","0","1e40"}) {
        combo(d,"horizontalRate")->setEditText(bad);check(!canApply(d),"invalid numeric input accepted");
    }
    enter(d,"horizontalRate","625","kS/s");enter(d,"horizontalTime","100","us/div");
    check(!canApply(d)&&d.findChild<QLabel*>("horizontalHint")->text().contains("1k"),"sub-1k RL accepted");
    enter(d,"horizontalRate","625","MS/s");check(canApply(d),"corrected mega rate rejected");
    enter(d,"horizontalRate","6.26","GS/s");check(!canApply(d),"rate upper bound missing");
    enter(d,"horizontalRate","6.25","GS/s");enter(d,"horizontalTime","500","us/div");check(canApply(d),"31.25M boundary rejected");
    enter(d,"horizontalTime","1","ms/div");check(!canApply(d),"standard capacity exceeded");
    combo(d,"horizontalLimits")->setCurrentIndex(1);check(canApply(d),"62.5M option rejected");
    enter(d,"horizontalTime","1.01","ms/div");check(!canApply(d),"option capacity exceeded");
    enter(d,"horizontalTime","1","ms/div");combo(d,"horizontalInputs")->setCurrentIndex(1);
    enter(d,"horizontalRecordLength","1000.5","points");check(!canApply(d),"fractional count accepted");
    enter(d,"horizontalRecordLength","1000","points");check(canApply(d),"minimum count rejected");
    enter(d,"horizontalRate","1.5625","S/s");check(canApply(d),"minimum rate rejected");
    enter(d,"horizontalRate","1","S/s");check(!canApply(d),"rate lower bound missing");
    combo(d,"horizontalMode")->setCurrentIndex(0);
    enter(d,"horizontalTime","200","ps/div");check(canApply(d),"Auto lower time bound rejected");
    enter(d,"horizontalTime","199","ps/div");check(!canApply(d),"Auto lower time bound missing");
    enter(d,"horizontalTime","1000","s/div");check(canApply(d),"upper time boundary rejected");
    enter(d,"horizontalTime","1001","s/div");check(!canApply(d),"upper time boundary missing");
    // Other models are not incorrectly limited to MSO44B specifications.
    combo(d,"horizontalLimits")->setCurrentIndex(2);manual(d,1);
    enter(d,"horizontalRate","25","GS/s");enter(d,"horizontalRecordLength","100","M points");
    check(canApply(d),"unknown model inherited MSO44B limits");
    const auto saved=saveRoundtrip(d);check(saved["horz_limits"]=="UNSPECIFIED","other profile not saved");
    MSOSeries456WriteDialog ignored(nullptr,"MSO44B",{{"sample_rate","1.25GS/s"},{"record_length","200k"}},1,"");
    manual(ignored);combo(ignored,"horizontalRate")->setEditText("bad");
    check(!canApply(ignored),"bad active rate accepted");combo(ignored,"horizontalMode")->setCurrentIndex(0);
    auto preserved=saveRoundtrip(ignored);check(preserved["sample_rate"]=="1.25GS/s","invalid inactive edit corrupted saved setting");
}
int main(int argc,char **argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    app.setFont(QFont("Segoe UI",9));
    try {if (argc > 1) { scopeReview(argv[1]); return 0; } pairs();switchingAndCustom();invalidAndLimits();std::cout<<"PASS input pairs/order, units, keyboard, preview, range profiles, XML, Cancel and inactive preservation\n";}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
