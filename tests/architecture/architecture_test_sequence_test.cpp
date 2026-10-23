#include "../../src/views/testsequence/dialogs/taskdialogeditor.h"
#include "../../src/infrastructure/worker/testrunsession.h"
#include "reportwriter.h"
#include "testrunservicefactory.h"
#include <QThread>
#include "testsequenceviewmodel.h"
#include "testsequencemodel.h"
#include "xmlconfigstore.h"
#include "testsequencecenterpanel.h"
#include "testsequencepage.h"
#include "testsequencestyle.h"
#include "instrumentexecutor.h"
#include "dcinputoperation.h"
#include "communicationfactory.h"
#include "oscilloscopestrategyfactory.h"
#include "oscilloscopeconfiguration.h"
#include "oscilloscopeautoperiod.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include "taskresultformatter.h"
#include "tasksettings.h"
#include <QTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QFile>
#include <QPushButton>
#include <QShortcut>
#include <QKeyEvent>
#include <QClipboard>
#include <QFontDatabase>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QJsonObject>
#include <QStyleOptionButton>
#include <stdexcept>
#include <iostream>
ICommunication* CommunicationFactory::create(const QString&) { throw std::runtime_error("unexpected hardware"); }
InstrumentExecutor::Result InstrumentExecutor::runInput(const InstrumentSetupConfig&,const InputRow&,InputAction) { throw std::runtime_error("unexpected input"); }
InstrumentExecutor::Result InstrumentExecutor::runLoad(const InstrumentSetupConfig&,const QVector<LoadDataRow>&,int,const LoadMetaRow&,LoadAction,bool) { throw std::runtime_error("unexpected load"); }
InstrumentExecutor::Result InstrumentExecutor::runDynamicLoad(const InstrumentSetupConfig&,const QVector<DynamicDataRow>&,int,const DynamicMetaRow&,DynamicLoadAction,bool,bool) { throw std::runtime_error("unexpected dynamic"); }
InstrumentExecutor::Result InstrumentExecutor::runRelay(const InstrumentSetupConfig&,const QVector<RelayDataRow>&,int,RelayAction) { throw std::runtime_error("unexpected relay"); }
InstrumentOperationResult runDcGroup(const InstrumentSetupConfig&,const DcGroup&,InputAction,const std::function<ICommunication*(const QString&)>&) { throw std::runtime_error("unexpected dc"); }
IOscilloscopeMeasureStrategy* OscilloscopeStrategyFactory::create(const QString&,const TransientContext*) { return nullptr; }
bool prepareOscilloscopeSettings(const QVariantMap&,const QString&,int,OscilloscopeSettings&,QString&) { return false; }
bool adjustOscilloscopeAutoPeriod(IScopeAutoPeriod*,QAtomicInt&,QString&,const OscCaptureObserver&, const VerticalScaleOptions&) { return false; }
void require(bool condition,const char* error) { if (!condition) throw std::runtime_error(error); }
QPushButton *dialogButton(QDialog *dialog, const QString &text) {
    for (auto *button : dialog->findChildren<QPushButton*>())
        if (button->text() == text) return button;
    throw std::runtime_error("missing dialog button");
}
void dialogTransactionChecks() {
    TestSequenceModel model; TestSequenceViewModel vm(&model); TestSequenceCenterPanel panel(&vm);
    QObject::connect(&panel, &TestSequenceCenterPanel::dutRowsChanged, &vm, &TestSequenceViewModel::onDutRowsChanged);
    InstrumentSetupConfig setup; InstrumentConfig scope;
    scope.type = "Oscilloscope"; scope.enabled = true; scope.modelName = "MSO44B";
    setup.instruments.append(scope); vm.onInstrumentSetupConfigChanged(setup);
    // Without a live TestConditions provider, the VM reads its model fallback;
    // on*Changed slots only notify widgets and never copy condition data.
    model.inputRows = {{"1phase", "230", "50", "0"}};
    model.loadRows = {{"L1", {"1"}}, {"L2", {"2"}}};
    model.dynamicRows = {{"D1", {"1"}}, {"D2", {"2"}}};
    model.relayRows = {{"Short", {"ON", "OFF"}}, {"Discharge", {"OFF", "ON"}}};
    for (const auto &kind : TestSequenceTaskKind::descriptors) {
        panel.addDutTestRow(kind.name);
        const int uid = model.tasks().last().uid;
        const auto before = vm.taskConfig(uid, kind.group);
        for (const bool escape : {false, true}) {
            QTimer::singleShot(0, [escape] {
                auto *dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                require(dialog, "no task dialog");
                for (auto *spin : dialog->findChildren<QSpinBox*>()) spin->setValue(spin->minimum());
                if (escape) {
                    QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                    QApplication::sendEvent(dialog, &key);
                } else dialogButton(dialog, "Close")->click();
            });
            require(!TaskDialogEditor(vm, uid, 1, {}, &panel).edit(kind.name), "Close/Escape accepted task");
            require(vm.taskConfig(uid, kind.group) == before, "Close/Escape committed edits");
        }
        QVariantMap firstApplied;
        for (int pass = 0; pass < 2; ++pass) {
            QTimer::singleShot(0, [pass] {
                auto *dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                require(dialog, "no Apply dialog");
                if (pass == 0) {
                    for (auto *combo : dialog->findChildren<QComboBox*>())
                        if (combo->count() > 1 && combo->itemText(0).contains("None")) combo->setCurrentIndex(combo->count() - 1);
                    for (auto *list : dialog->findChildren<QListWidget*>())
                        for (int i = 0; i < list->count(); ++i) list->item(i)->setCheckState(Qt::Checked);
                    for (auto *spin : dialog->findChildren<QSpinBox*>()) spin->setValue(qMin(spin->maximum(), spin->minimum() + 7));
                    if (auto *target = dialog->findChild<QComboBox*>("measureTarget")) target->setCurrentIndex(target->findData("MIN"));
                    if (auto *capture = dialog->findChild<QCheckBox*>("captureEnabled")) capture->setChecked(true);
                    if (auto *threshold = dialog->findChild<QDoubleSpinBox*>("shutdown_voltage")) threshold->setValue(1.75);
                }
                dialogButton(dialog, "Apply")->click();
            });
            require(TaskDialogEditor(vm, uid, 1, {}, &panel).edit(kind.name), "real Apply rejected draft");
            const auto saved = vm.taskConfig(uid, kind.group);
            if (pass == 0) firstApplied = saved;
            else require(QJsonObject::fromVariantMap(firstApplied) == QJsonObject::fromVariantMap(saved), "reopening changed applied settings");
            QString xml; QXmlStreamWriter writer(&xml); model.writeXml(writer);
            TestSequenceModel restored; QXmlStreamReader reader(xml); reader.readNextStartElement(); restored.loadXml(reader);
            require(!reader.hasError(), "Dialog Apply produced invalid XML");
            const auto loaded = restored.tasks().last().settings.value(kind.group).toMap();
            require(QJsonObject::fromVariantMap(saved) == QJsonObject::fromVariantMap(loaded), "XML changed dialog settings");
            vm.setTaskConfig(uid, kind.group, loaded);
            const auto payloads = vm.taskPayloads({{uid, kind.name}});
            require(payloads.size() == 1, "saved selections cannot resolve at Run");
            auto resolved = payloads.first().settings; QString resolutionError;
            require(TestSequenceConditions::resolve(resolved, vm.executionContext(), resolutionError), qPrintable(resolutionError));
            const auto decoded = TaskSettings::decode(kind.name, resolved);
            if (const auto *group = std::get_if<TaskSettings::Group>(&decoded.settings))
                require(group->loads == QVector<int>{0, 1}, "Group lost condition execution order");
            if (const auto *brown = std::get_if<BrownOut::Settings>(&decoded.settings))
                require(brown->shutdownVoltage == 1.75, "Run ignored user shutdown threshold");
        }
        std::cout << "PASS dialog buttons/XML " << kind.name << '\n';
    }
}
class LateXmlPage : public IXmlSerializable {
public:
    QString value = "old";
    bool failApply = false;
    QString xmlTagName() const override { return "LatePage"; }
    void writeXml(QXmlStreamWriter &writer) const override { writer.writeTextElement(xmlTagName(), value); }
    void validateXml(QXmlStreamReader &reader) const override { reader.readElementText(); }
    void loadXml(QXmlStreamReader &reader) override {
        value = reader.readElementText();
        if (failApply && value == "new") reader.raiseError("injected late page failure");
    }
};
void xmlRollbackChecks() {
    QTemporaryDir directory;
    require(directory.isValid(), "rollback directory failed");
    for (const QString failure : {"apply", "synchronize", "empty_draft"}) {
        TestSequenceModel model;
        TestSequenceViewModel vm(&model);
        TestSequenceCenterPanel panel(&vm);
        QObject::connect(&panel, &TestSequenceCenterPanel::dutRowsChanged, &vm, &TestSequenceViewModel::onDutRowsChanged);
        QObject::connect(&vm, &TestSequenceViewModel::dutTableChanged, &panel, &TestSequenceCenterPanel::loadDutRows);
        panel.addDutTestRow("Delay");
        if (failure == "empty_draft") panel.addDutTestRow("Write Oscilloscope");
        vm.setReportFile("original", directory.path());
        const auto before = model.tasks();
        const int uid = before.first().uid;
        vm.setTaskConfig(uid, "delay", {{"delay_ms", 0}});
        const auto settings = vm.taskSettings(uid);
        int notifications = 0;
        QObject::connect(&vm, &TestSequenceViewModel::dutTableChanged, [&] { ++notifications; });
        TestSequenceModel incoming;
        incoming.reportName = "replacement";
        DutRowData row; row.item = "Delay"; row.settings = {{"delay", QVariantMap{{"delay_ms", 123}}}};
        incoming.appendTask(row);
        const auto path = directory.filePath("rollback.xml");
        QFile file(path); require(file.open(QIODevice::WriteOnly), "rollback fixture open failed");
        QXmlStreamWriter writer(&file); writer.writeStartElement("loodGUI"); incoming.writeXml(writer);
        writer.writeTextElement("LatePage", "new"); writer.writeEndElement(); file.close();
        LateXmlPage late; late.failApply = failure == "apply";
        int synchronizations = 0;
        const auto result = XmlConfigStore::loadAllFromXml(path, {&vm, &late}, [&] {
            if (!late.failApply && ++synchronizations == 1) throw std::runtime_error("injected synchronization failure");
        });
        require(!result.succeeded() && !result.detail.contains("rollback failed"), "XML rollback itself failed");
        require(notifications == 0 && late.value == "old" && vm.reportName() == "original" &&
                vm.reportDirectory() == directory.path(), "failed transaction published or retained replacement state");
        require(model.tasks().size() == before.size() && model.tasks().first().uid == uid &&
                vm.taskSettings(uid) == settings, "XML rollback invalidated stable task UID");
        require(vm.taskPayloads({{uid, "Delay"}}).size() == 1, "existing Run task lost its identity after rollback");
        vm.setTaskConfig(uid, "delay", {{"delay_ms", 42}});
        require(vm.taskConfig(uid, "delay").value("delay_ms").toInt() == 42, "existing edit UID no longer targets original task");
        late.failApply = false;
        require(XmlConfigStore::loadAllFromXml(path, {&vm, &late}).succeeded(), "subsequent valid load failed");
        require(notifications == 1 && vm.reportName() == "replacement" &&
                model.tasks().size() == 1 && model.tasks().first().uid != uid &&
                vm.taskPayloads({{uid, "Delay"}}).isEmpty(),
                "successful load reused old identity or invoked rollback");
    }
}
void reportSelectionChecks(QApplication& app) {
    TestSequenceModel model;
    TestSequenceViewModel vm(&model);
    TestSequencePage page(&vm);
    auto* panel = page.findChild<TestSequenceCenterPanel*>();
    auto* all = page.findChild<QPushButton*>("reportAllButton");
    auto* none = page.findChild<QPushButton*>("reportNoneButton");
    require(panel && all && none, "Report bulk controls missing");
    int changes = 0;
    QObject::connect(&vm, &TestSequenceViewModel::dutTableChanged, [&] { ++changes; });
    all->click(); none->click();
    require(changes == 0 && model.tasks().isEmpty(), "Empty report action changed tasks");
    panel->addDutTestRow("Delay"); panel->addDutTestRow("Delay");
    require(!model.tasks()[0].report && !model.tasks()[1].report, "New tasks report by default");
    auto rows = model.tasks();
    rows[0].ext = "Keep name"; rows[0].retry = "2";
    rows[1].active = false;
    vm.onDutRowsChanged(rows); panel->loadDutRows();
    const int firstUid = rows[0].uid;
    vm.setTaskConfig(firstUid, "delay", {{"delay_ms", 100}});
    const auto settings = vm.taskSettings(firstUid);
    all->click();
    require(changes == 1 && model.tasks()[0].report && model.tasks()[1].report,
            "Report All did not update active and inactive rows in one batch");
    all->click();
    require(changes == 1, "Repeated Report All unnecessarily reloaded table");
    require(model.tasks()[0].uid == firstUid && model.tasks()[0].ext == "Keep name" &&
            model.tasks()[0].retry == "2" && !model.tasks()[1].active && vm.taskSettings(firstUid) == settings,
            "Bulk report action changed unrelated task data");
    QTableWidget* table = nullptr;
    for (auto* t : panel->findChildren<QTableWidget*>()) if (t->columnCount() == 6) table = t;
    require(table, "DUT table missing");
    auto reportCheck = [&](int row) { return table->cellWidget(row, 5)->findChild<QPushButton*>(); };
    require(reportCheck(0)->isChecked() && reportCheck(1)->isChecked(), "Report All not rendered");
    reportCheck(0)->click();
    require(!model.tasks()[0].report && model.tasks()[1].report, "Individual report edit failed");
    none->click();
    require(!model.tasks()[0].report && !model.tasks()[1].report && !reportCheck(1)->isChecked(),
            "Report None failed on mixed rows");
    reportCheck(0)->click();
    table->selectRow(0);
    for (auto* shortcut : table->findChildren<QShortcut*>())
        if (shortcut->key() == QKeySequence::Copy) QMetaObject::invokeMethod(shortcut, "activated");
    for (auto* shortcut : table->findChildren<QShortcut*>())
        if (shortcut->key() == QKeySequence::Paste) QMetaObject::invokeMethod(shortcut, "activated");
    require(model.tasks().size() == 3 && model.tasks()[1].report && !model.tasks()[2].report,
            "Copy changed explicit Report flag");
    QByteArray xml; QXmlStreamWriter writer(&xml); vm.writeXml(writer);
    QXmlStreamReader reader(xml); reader.readNextStartElement(); vm.loadXml(reader);
    require(!reader.hasError() && model.tasks()[0].report && model.tasks()[1].report && !model.tasks()[2].report,
            "Report flags did not round trip through current XML");
    none->click();
    const int loadedUid = model.tasks()[0].uid;
    require(vm.reportName().isEmpty() && vm.reportDirectory().isEmpty(), "No-report fixture has destination");
    require(vm.startExecution(vm.taskPayloads({{loadedUid, "Delay"}})), "Unchecked Report required a file path");
    require(!all->isEnabled() && !none->isEnabled(), "Report bulk controls enabled during run");
    vm.setAllTasksReport(true);
    require(!model.tasks()[0].report && !model.tasks()[1].report, "Bulk API changed active run settings");
    QEventLoop loop;
    QObject::connect(&vm, &TestSequenceViewModel::runningChanged, &loop, [&](bool running) { if (!running) loop.quit(); });
    QTimer::singleShot(3000, &loop, &QEventLoop::quit); loop.exec();
    require(!vm.isRunning() && all->isEnabled() && none->isEnabled() &&
            !vm.resultHistory().isEmpty() && !vm.resultHistory().last().report,
            "No-report result or bulk-control unlock failed");
    panel->addDutTestRow("Static Test");
    require(!model.tasks().last().report, "New task inherited earlier bulk Report action");
    page.findChild<QTabWidget*>()->setCurrentIndex(1);
    page.resize(1200, 800); page.show(); app.processEvents();
    require(page.grab().save("testSequence-report-selection.png"), "Report selection preview failed");
    // Render actual button styles in each interaction state for visual review.
    QPixmap states(360, 220); states.fill(Qt::white);
    QPainter painter(&states);
    const QStringList labels={"Normal", "Hover", "Pressed", "Keyboard focus", "Disabled"};
    for(int i=0;i<labels.size();++i) {
        QStyleOptionButton option;option.initFrom(all);option.text=all->text();
        option.rect=QRect(150,8+i*42,all->width(),all->height());
        option.state=QStyle::State_Enabled|QStyle::State_Active|QStyle::State_Raised;
        if(i==1)option.state|=QStyle::State_MouseOver;
        if(i==2){option.state&=~QStyle::State_Raised;option.state|=QStyle::State_Sunken;}
        if(i==3)option.state|=QStyle::State_HasFocus|QStyle::State_KeyboardFocusChange;
        if(i==4)option.state&=~QStyle::State_Enabled;
        painter.setPen(Qt::black);painter.drawText(QRect(8,8+i*42,138,all->height()),Qt::AlignVCenter,labels[i]);
        all->style()->drawControl(QStyle::CE_PushButton,&option,&painter,all);
    }
    painter.end();
    require(states.save("testSequence-report-button-states.png"),"Report button state preview failed");
}
void reportPanelChecks(QApplication& app) {
    TestSequenceModel model;
    TestSequenceViewModel vm(&model);
    TestSequenceRightPanel panel(&vm);
    panel.setStyleSheet(TestSequenceStyle::MAIN);
    auto* name = panel.findChild<QLineEdit*>("reportNameEdit");
    auto* folder = panel.findChild<QLineEdit*>("reportDirectoryEdit");
    auto* browse = panel.findChild<QPushButton*>("reportBrowseButton");
    require(name && folder && browse && !folder->isReadOnly(), "Report form controls missing");
    QTemporaryDir directory;
    require(directory.isValid(), "Report fixture directory failed");
    name->setText(QString::fromUtf8("測試 & report.xlsx"));
    app.clipboard()->setText(directory.path());
    folder->paste();
    require(vm.reportName() == name->text() && vm.reportDirectory() == directory.path(),
            "Report edits did not reach ViewModel");
    QByteArray xml;
    QXmlStreamWriter writer(&xml); vm.writeXml(writer);
    name->clear(); folder->clear();
    QXmlStreamReader reader(xml); reader.readNextStartElement(); vm.loadXml(reader);
    require(!reader.hasError() && name->text() == QString::fromUtf8("測試 & report.xlsx") &&
            folder->text() == directory.path() && folder->toolTip() == directory.path(),
            "Report XML reload did not restore editable fields");
    panel.resize(278, 600); panel.show(); app.processEvents();
    for (int width : {278, 620}) {
        panel.resize(width, 600); app.processEvents();
        auto* scroll = panel.findChild<QScrollArea*>();
        const QRect viewport = scroll->viewport()->rect();
        for (auto* control : {static_cast<QWidget*>(name), static_cast<QWidget*>(folder),
                              static_cast<QWidget*>(browse)}) {
            const QRect visible(control->mapTo(scroll->viewport(), QPoint()), control->size());
            require(viewport.contains(visible), "Report field clipped at supported panel width");
        }
        require(scroll->verticalScrollBar()->maximum() == 0, "Report form has unnecessary scrollbar");
        require(panel.grab().save(QString("report-panel-%1.png").arg(width)), "Report preview save failed");
    }
    panel.setExpanded(false, false); app.processEvents();
    panel.setExpanded(true, false); app.processEvents();
    require(folder->isVisible() && folder->text() == directory.path(), "Collapse lost report settings");
    DutRowData row; row.item = "Delay"; row.report = false;
    const int uid = vm.appendTask(row);
    vm.setTaskConfig(uid, "delay", {{"delay_ms", 100}});
    auto tasks = vm.taskPayloads({{uid, "Delay"}});
    require(vm.startExecution(tasks), "Report lock fixture could not start");
    require(!name->isEnabled() && !folder->isEnabled() && !browse->isEnabled(), "Running form still editable");
    TestSequenceRightPanel latePanel(&vm);
    require(!latePanel.findChild<QLineEdit*>("reportNameEdit")->isEnabled(), "Late panel missed initial run lock");
    QEventLoop loop;
    QObject::connect(&vm, &TestSequenceViewModel::runningChanged, &loop, [&](bool running) { if (!running) loop.quit(); });
    QTimer::singleShot(3000, &loop, &QEventLoop::quit); loop.exec();
    require(!vm.isRunning() && name->isEnabled() && folder->isEnabled() && browse->isEnabled(), "Report form did not unlock");
    panel.hide();
    TestSequenceModel previewModel;
    TestSequenceViewModel previewVm(&previewModel);
    previewVm.setReportFile("BrownOut_12V", "C:/ATE/Reports/PowerSupply/2026-10-11");
    TestSequencePage page(&previewVm);
    for (int width : {1100, 1750}) {
        page.resize(width, 800); page.show(); app.processEvents();
        require(page.grab().save(QString("testSequence-report-%1.png").arg(width)), "TestSequence preview save failed");
    }
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        if (argc > 1 && QString(argv[1]) == "xml_rollback") { xmlRollbackChecks(); return 0; }
        if (argc > 1 && QString(argv[1]) == "dialog_transactions") { dialogTransactionChecks(); return 0; }
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
        reportPanelChecks(app);
        reportSelectionChecks(app);
        {
            TestSequenceModel dialogsModel; TestSequenceViewModel dialogsVm(&dialogsModel); TestSequenceCenterPanel dialogsPanel(&dialogsVm);
            QObject::connect(&dialogsPanel,&TestSequenceCenterPanel::dutRowsChanged,&dialogsVm,&TestSequenceViewModel::onDutRowsChanged);
            InstrumentSetupConfig scopeConfig; InstrumentConfig instrument; instrument.type="Oscilloscope"; instrument.enabled=true; instrument.modelName="MSO44B";
            scopeConfig.instruments.append(instrument); dialogsVm.onInstrumentSetupConfigChanged(scopeConfig);
            const QMap<QString,QString> tasks{{"Delay","delay"},{"Write Oscilloscope","osc"},{"Static Test","static"},
                {"Dynamic Test","dynamic"},{"Relay","relay"},{"Turn on","turnOn"},{"Turn off","turnOff"},
                {"Short then turn on","shortOn"},{"Turn on then short","onShort"}};
            for(auto it=tasks.cbegin();it!=tasks.cend();++it) {
                dialogsPanel.addDutTestRow(it.key()); const int id=dialogsModel.tasks().last().uid;
                const auto before=dialogsVm.taskConfig(id,it.value());
                bool uiRules = true;
                QTimer::singleShot(0,[&]{
                    auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    if (!d) { uiRules = false; return; }
                    if (QStringList{"Delay", "Static Test", "Dynamic Test"}.contains(it.key())) {
                        const auto spins = d->findChildren<QSpinBox*>();
                        const auto &rule = TaskSettingRules::settle;
                        uiRules = spins.size() == (it.key()=="Delay" ? 1 : 3);
                        for (auto *spin : spins)
                            uiRules = uiRules && spin->minimum() == rule.minimum &&
                                      spin->maximum() == rule.maximum && spin->value() == rule.initial;
                    }
                    d->reject();
                });
                require(!TaskDialogEditor(dialogsVm,id,1,{},&dialogsPanel).edit(it.key()),"Close accepted");
                require(uiRules, "Dialog numeric limits/default differ from shared rule");
                require(dialogsVm.taskConfig(id,it.value())==before,"Close changed settings");
                QTimer::singleShot(0,[]{ auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget()); if(d)d->accept(); });
                require(TaskDialogEditor(dialogsVm,id,1,{},&dialogsPanel).edit(it.key()),"Apply rejected");
            }
        }
        TestSequenceModel model; TestSequenceViewModel vm(&model); TestSequenceCenterPanel panel(&vm);
        QObject::connect(&panel,&TestSequenceCenterPanel::dutRowsChanged,&vm,&TestSequenceViewModel::onDutRowsChanged);
        QObject::connect(&vm,&TestSequenceViewModel::dutTableChanged,&panel,&TestSequenceCenterPanel::loadDutRows);
        panel.addDutTestRow("Delay"); panel.addDutTestRow("Delay");
        vm.setAllTasksReport(true); // This fixture explicitly exercises Excel output.
        require(model.tasks().size()==2,"view/model row mismatch");
        const int uid = model.tasks()[0].uid;
        require(uid>=0 && model.tasks()[1].uid!=uid,"UID collision");
        require(vm.taskConfig(uid,"delay").value("delay_ms").toInt()==5000,"default settings lost");
        vm.setTaskConfig(uid,"delay",{{"delay_ms",0}});
        auto payloads=vm.taskPayloads({{uid,"Delay"}});
        require(payloads.size()==1 && payloads[0].settings.value("delay_ms").toInt()==0,"run read stale view settings");
        require(vm.taskPayloads({{uid,"Relay"}}).isEmpty(),"mismatched task identity accepted");
        QByteArray xml; QXmlStreamWriter writer(&xml); model.writeXml(writer);
        QXmlStreamReader reader(xml); reader.readNextStartElement(); model.loadXml(reader);
        require(!reader.hasError(),"XML roundtrip failed");
        panel.loadDutRows();
        require(model.tasks().size()==2,"load duplicated model rows");
        const int loadedUid=model.tasks()[0].uid;
        require(vm.taskConfig(loadedUid,"delay").value("delay_ms").toInt()==0,"XML settings lost");
        panel.addDutTestRow("Relay");
        require(model.tasks().size()==3 && model.tasks().last().uid!=loadedUid,"append after load failed");
        QTableWidget* table=nullptr;
        for (auto* candidate:panel.findChildren<QTableWidget*>())
            if(candidate->columnCount()==6) table=candidate;
        require(table,"DUT table missing");
        table->selectRow(0);
        for(auto* shortcut:table->findChildren<QShortcut*>())
            if(shortcut->key()==QKeySequence::Copy) QMetaObject::invokeMethod(shortcut,"activated");
        for(auto* shortcut:table->findChildren<QShortcut*>())
            if(shortcut->key()==QKeySequence::Paste) QMetaObject::invokeMethod(shortcut,"activated");
        require(model.tasks().size()==4,"paste lost row");
        require(model.tasks()[1].uid!=loadedUid && model.tasks()[1].settings==model.tasks()[0].settings,"paste identity or settings wrong");
        QKeyEvent removeEvent(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);
        QApplication::sendEvent(table,&removeEvent);
        require(model.tasks().size()==3 && model.tasks()[0].uid==loadedUid,"delete removed wrong model row");
        QTemporaryDir directory; require(directory.isValid(),"temporary folder failed");
        vm.setReportFile("Results",directory.path());
        payloads=vm.taskPayloads({{loadedUid,"Delay"}});
        QEventLoop loop; bool finished=false;
        QObject::connect(&vm,&TestSequenceViewModel::runningChanged,&loop,[&](bool running) { if(!running){finished=true;loop.quit();} });
        QTimer::singleShot(5000,&loop,&QEventLoop::quit);
        require(vm.startExecution(payloads),"delay rejected");loop.exec();
        require(finished && QFileInfo::exists(directory.filePath("Results.xlsx")),"async report not completed before unlock");
        const auto originalSize=QFileInfo(directory.filePath("Results.xlsx")).size();
        finished=false;require(vm.startExecution(payloads),"second run rejected");loop.exec();
        require(finished && QFileInfo(directory.filePath("Results.xlsx")).size()>originalSize,"report did not append");
        TestSequenceResultRecord formatted;
        formatted.taskName = "Static Test";
        formatted.channels.append({2,"A",55.68,-1.5,3,2});
        formatted.trials = {{1,MeasurementDirection::Rise,1,1,1,{{2,"A",55.68,-1.5,3,2}}}};
        const auto summary = TaskResultFormatter::measurement(formatted,true);
        require(summary.contains("1 directions") && summary.contains("1 CH") && summary.contains("1 trials") &&
                summary.contains("View details") && !summary.contains("55.68"), "Tasks summary expanded measurement values");
        payloads[0].settings["delay_ms"]=500;
        require(vm.startExecution(payloads),"shutdown run rejected");
        QEventLoop shutdownLoop;bool shutdown=false;int ticks=0;
        QTimer heartbeat;heartbeat.setInterval(1);
        QObject::connect(&heartbeat,&QTimer::timeout,[&]{++ticks;});heartbeat.start();
        QObject::connect(&vm,&TestSequenceViewModel::shutdownFinished,&shutdownLoop,[&]{shutdown=true;shutdownLoop.quit();});
        QElapsedTimer requested;requested.start();vm.requestShutdown();vm.requestShutdown();
        require(requested.elapsed()<100,"shutdown blocked UI thread");
        require(!vm.startExecution(payloads),"shutdown admitted another run");
        QTimer::singleShot(5000,&shutdownLoop,&QEventLoop::quit);shutdownLoop.exec();
        require(shutdown && ticks>0 && !vm.isRunning() && vm.isShutdownComplete(),"shutdown did not wait for run/report/thread");
        auto session=std::make_unique<TestRunSession>(
            std::make_unique<TestRunService>(TestRunDependencies{},nullptr),
            std::make_unique<ReportWriter>([](const QString&,const QString&,const QVector<TestSequenceResultRecord>&) {
                QThread::msleep(100);return ReportWriteResult{false,{},"injected report failure"};
            },nullptr));
        bool reportFailed=false;
        QObject::connect(session.get(),&TestRunSession::reportCompleted,[&](bool ok,const QString&,const QString&){reportFailed=!ok;});
        TestSequenceViewModel injected(&model,std::move(session));
        injected.setReportFile("Injected",directory.path());payloads[0].settings["delay_ms"]=0;
        QEventLoop injectedLoop;bool injectedStopped=false;
        QObject::connect(&injected,&TestSequenceViewModel::shutdownFinished,&injectedLoop,[&]{injectedStopped=true;injectedLoop.quit();});
        require(injected.startExecution(payloads),"injected session rejected task");
        injected.requestShutdown();
        QTimer::singleShot(5000,&injectedLoop,&QEventLoop::quit);injectedLoop.exec();
        require(reportFailed && injectedStopped && !injected.isRunning(),"report failure blocked session shutdown");
        std::cout<<"PASS architecture testsequence\n";return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}

// These fault fixtures isolate orchestration; measurement_preflight_test uses the real validator.
void validateMeasurementConditions(const TestSequenceExecutionContext &, InputSelection, int, bool) {}
