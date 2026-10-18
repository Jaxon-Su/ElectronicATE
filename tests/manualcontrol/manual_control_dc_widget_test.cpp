#include "../support/configuration_wait.h"
#include "manualcontrolpage.h"
#include <QApplication>
#include <QFontDatabase>
#include <QPushButton>
#include <QThreadPool>
#include <stdexcept>
#include <iostream>

// No scope or hardware is created by this UI test.
QWidget* TriggerWidgetFactory::createTriggerWidget(const QString&, QWidget*, QObject**, int) { return nullptr; }
void require(bool ok) { if (!ok) throw std::runtime_error("DC UI assertion failed"); }
void finish() { require(QThreadPool::globalInstance()->waitForDone(3000)); QCoreApplication::processEvents(); }
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    require(QFontDatabase::addApplicationFont("C:/Windows/Fonts/arial.ttf") >= 0);
    app.setFont(QFont("Arial", 9));
    try {
        ManualControlModel model;
        ManualControlOperations ops;
        int singles=0, groups=0, source=-1;
        QString voltage;
        InstrumentOperationResult nextResult;
        InputAction lastAction = InputAction::PowerOff;
        ops.connectScopes=[](const InstrumentSetupConfig&) { return OscilloscopeManager::OscMap{}; };
        ops.dcInput=[&](const InstrumentSetupConfig&, int s, const DcRow& row, InputAction action) {
            ++singles; source=s; voltage=row.vin; lastAction=action; return nextResult;
        };
        ops.dcGroup=[&](const InstrumentSetupConfig&, const DcGroup&, InputAction action) {
            ++groups; lastAction=action; return nextResult;
        };
        ManualControlViewModel vm(&model, ops);
        ManualControlPage page(&vm);
        InstrumentSetupConfig config;
        config.dcInputs=3;
        for(int i=0;i<3;++i) {
            InstrumentConfig instrument;
            instrument.type="InputDCSource";
            instrument.name=QString("DC Source%1").arg(i+1);
            config.instruments.append(instrument);
        }
        vm.onInstrumentSetupConfigChanged(config);
        startQueuedConfiguration(&vm);
        finish();
        TestConditionSnapshot conditions;
        conditions.dcNames={"", "Condition B"};
        conditions.dcRows={{}, {"12","2",""}};
        conditions.dcRows2={{}, {"5","1",""}};
        conditions.dcRows3={{}, {"24","3",""}};
        vm.onConditionsChanged(conditions);
        auto* condition=page.findChild<QComboBox*>("cmbDcInput1");
        auto* target=page.findChild<QComboBox*>("cmbDcTarget");
        auto* power=page.findChild<QPushButton*>("btnDcInput1");
        auto* change=page.findChild<QPushButton*>("btnDcChange1");
        require(condition && target && power && change);
        require(condition->count()==1 && condition->currentData().toInt()==1);
        target->setCurrentIndex(target->findData(1));
        power->click();
        require(!target->isEnabled());
        finish();
        require(singles==1 && groups==0 && source==1 && voltage=="5");
        require(power->isChecked() && !vm.dcOutputOn(0));
        target->setCurrentIndex(target->findData(0));
        require(!power->isChecked());
        target->setCurrentIndex(target->findData(-1));
        require(power->isChecked());
        power->click(); finish();
        require(groups==1 && !vm.hasActiveControl());
        power->click(); finish();
        require(groups==2 && vm.dcOutputOn(0) && vm.dcOutputOn(1) && vm.dcOutputOn(2));
        target->setCurrentIndex(target->findData(2));
        change->click(); finish();
        require(singles==2 && source==2 && voltage=="24");
        page.resize(1000,900); page.show(); app.processEvents();
        require(page.findChild<QGroupBox*>("grpDcInput1")->grab().save("manualControl-dc-controls.png"));
        target->setCurrentIndex(target->findData(-1));
        power->click(); finish();
        require(!vm.hasActiveControl());
        for (int selected : {1, -1}) {
            target->setCurrentIndex(target->findData(selected));
            nextResult = {false, "ON write/readback failed", false};
            power->click();
            require(power->property("outputStatus") == "busy" && power->text() == "Working...");
            require(!power->isEnabled());
            const auto busyFill = power->grab().toImage().pixelColor(8, 8);
            require(busyFill.red() == busyFill.green() && busyFill.green() == busyFill.blue());
            finish();
            require(vm.hasUnknownOutput() && power->isChecked() && power->isEnabled());
            require(power->text() == "UNKNOWN" && power->property("outputStatus") == "unknown");
            const auto fill = power->grab().toImage().pixelColor(8, 8);
            require(fill.red() > fill.blue() + 30 && fill.green() > fill.blue());
            require(page.findChild<QGroupBox*>("grpDcInput1")->grab().save("manualControl-dc-unknown.png"));
            nextResult = {};
            power->click(); finish();
            require(lastAction == InputAction::PowerOff && !vm.hasActiveControl());
            require(!power->isChecked() && power->property("outputStatus") == "normal");
        }
        std::cout << "PASS: DC UI target dispatch, condition indices, mixed states and busy controls\n";
    } catch(const std::exception& e) { std::cerr << e.what(); return 1; }
}
