#include "../../support/test_evidence.h"
#include "msoseries456writedialog.h"
#include "createwriteoscilloscopedialog.h"
#include <memory>
#include "testsequencemodel.h"
#include <QApplication>
#include <QFontDatabase>
#include <QCheckBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include "oscilloscopesettingrules.h"
#include <stdexcept>
int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/arial.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
    app.setFont(QFont("Arial", 10));
    for (const QString& name : {"DPO7000", "DPO4000", "MSO4000", "DPO5000", "unknown", ""}) {
        std::unique_ptr<OscWriteDialogBase> unavailable(
            createWriteOscilloscopeDialog(nullptr, name, {}, 1, "", nullptr));
        if (unavailable->findChild<QPushButton*>("applyBtn"))
            throw std::runtime_error("unavailable model exposed Apply");
        unavailable->setResult(QDialog::Accepted);
        unavailable->findChild<QPushButton*>("closeBtn")->click();
        if (unavailable->result() != QDialog::Rejected)
            throw std::runtime_error("unavailable Close must reject");
    }
    std::unique_ptr<OscWriteDialogBase> supported(
        createWriteOscilloscopeDialog(nullptr, "MSO44B", {}, 1, "", nullptr));
    if (!supported->findChild<QPushButton*>("applyBtn"))
        throw std::runtime_error("MSO model missing Apply");
    const QVariantMap original{{"timescale", "20ms"}};
    for (int action = 0; action < 3; ++action) {
        MSOSeries456WriteDialog cancelled(nullptr, "MSO44B", original, 1, "");
        cancelled.findChildren<QCheckBox*>().first()->setChecked(true);
        cancelled.show();
        app.processEvents();
        cancelled.setResult(QDialog::Accepted);
        if (action == 0) {
            cancelled.findChild<QPushButton*>("closeBtn")->click();
        } else if (action == 1) {
            cancelled.close();
        } else {
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(&cancelled, &escape);
        }
        if (cancelled.result() != QDialog::Rejected || cancelled.config() != original)
            throw std::runtime_error("Close/window close/Escape must discard edits");
    }
    MSOSeries456WriteDialog dialog(nullptr, "MSO44B", {}, 1, "");
    for(auto *spin:dialog.findChildren<QDoubleSpinBox*>())
        if(spin->decimals()!=4)throw std::runtime_error("Write dialog decimal precision is not four");
    const QVariantMap precise{{"horz_pos",50.1254},{"ch1_position",-1.1254},{"ch1_scale","0.0005V"},
                               {"ch2_scale","12.34567mA"},{"ch3_scale","0.0000000001V"},{"sample_rate","1.5625GS/s"}};
    MSOSeries456WriteDialog formatted(nullptr,"MSO44B",precise,1,"");
    auto *rateNumber=formatted.findChild<QComboBox*>("horizontalRate");
    auto *rateUnit=formatted.findChild<QComboBox*>("horizontalRateUnit");
    if(!rateNumber || !rateUnit || rateNumber->currentText()!="1.5625" || rateUnit->currentText()!="GS/s")
        throw std::runtime_error("separate sample-rate number/unit changed numeric value");
    formatted.findChild<QPushButton*>("applyBtn")->click();
    const auto formattedCfg=formatted.config();
    if(formattedCfg["horz_pos"].toDouble()!=50.1254||formattedCfg["ch1_position"].toDouble()!=-1.1254||
       formattedCfg["sample_rate"].toString()!="1.5625GS/s")throw std::runtime_error("TestSequence precision change lost settings");
    if(formattedCfg["ch1_scale"].toString()!="500uV"||formattedCfg["ch2_scale"].toString()!="12.3457mA")
        throw std::runtime_error("custom probe-unit scale formatting wrong");
    const auto small=OscilloscopeSettingRules::quantity(QJsonValue::fromVariant(formattedCfg["ch3_scale"]),"channel");
    if(!small||std::abs(*small-1e-10)>1e-20)throw std::runtime_error("small scale rounded to zero");
    auto* check = dialog.findChild<QCheckBox*>("autoPeriod");
    if (check) throw std::runtime_error("Write still exposes Auto Period");
    dialog.findChild<QPushButton*>("applyBtn")->click();
    if (dialog.result() != QDialog::Accepted || dialog.config().contains("auto_period"))
        throw std::runtime_error("Apply must accept and save edits");
    TestSequenceModel model;
    DutRowData row; row.item = "Write Oscilloscope"; row.settings = {{"osc", dialog.config()}};
    model.appendTask(row);
    QString xml; QXmlStreamWriter writer(&xml); model.writeXml(writer);
    TestSequenceModel loaded; QXmlStreamReader reader(xml); reader.readNextStartElement(); loaded.loadXml(reader);
    if (loaded.tasks().size() != 1 || loaded.tasks()[0].settings.value("osc").toMap().contains("auto_period"))
        throw std::runtime_error("XML lost Auto Period");
    MSOSeries456WriteDialog restored(nullptr, "MSO44B", loaded.tasks()[0].settings.value("osc").toMap(), 1, "");
    if (restored.findChild<QCheckBox*>("autoPeriod"))
        throw std::runtime_error("dialog lost Auto Period");
    restored.resize(880, 900);
    restored.show(); app.processEvents();
    restored.grab().save(TestEvidence::path("auto-period-dialog.png"));
    return 0;
}
