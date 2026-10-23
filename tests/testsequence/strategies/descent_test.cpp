#include "../../support/test_evidence.h"
#include "descentstrategy.h"
#include "oscilloscopestrategyfactory.h"
#include "oscilloscope.h"
#include "msoseries456.h"
#include "testrunservice.h"
#include "testsequencemodel.h"
#include "testsequencetaskdefaults.h"
#include "testsequencesettingsvalidation.h"
#include "testsequenceresultrows.h"
#include "../../../src/views/testsequence/dialogs/turnonoffdialogs/descentdialog.h"
#include <QApplication>
#include <QThread>
#include <QElapsedTimer>
#include <QDir>
#include <QJsonDocument>
#include <QFontDatabase>
#include <QScrollBar>
#include "scopemeasurementstrategy.h"
#include "../../../src/service/capture/strategywaveformcapture.h"
#include <QTemporaryDir>
#include <QKeyEvent>
#include <iostream>

void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
class Scope final : public Oscilloscope {
public:
    QString model() const override {return "MSOSeries456";}
    QString vendor() const override {return "offline";}
    QByteArray captureScreenshot(const QString &,const QString &) override {return "offline screenshot fixture";}
    ScopeCaptureSource captureSource() override {return {QString("CH%1").arg(source),source};}
    bool waitForOperationComplete(int) override {return true;}
    bool captureWaveformFileToHost(int channel,const QString &path,const QString &,const QString &,int,int) override {
        require(isChannelEnabled(channel),"Capture read a hidden channel");
        QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write("offline waveform fixture")>0;
    }
    QString lastError() const override {return error;}
    int getTotalChannel() override {return 4;}
    bool isChannelEnabled(int channel) override {
        error = displayWritten && displayFault=="read" ? "display read failed" : QString{};
        return !allHidden && channel != disabled && channel != otherDisabled;
    }
    bool setChannelDisplay(int channel,bool enabled,QString &e) override {
        require(channel==4 && !armed && !powerOn,"display before STOP/Input OFF or wrong CH");
        trace << (enabled ? "display on" : "display off"); displayWritten=true;
        if(displayFault=="throw")throw std::runtime_error("display exception");
        if(displayFault=="reject"){e="display rejected";return false;}
        if(displayFault!="mismatch")disabled=enabled?0:channel;
        if(displayStop)displayStop->storeRelease(1);
        return true;
    }
    double getChannelScale(int) override {return 10;}
    bool isClipping(int) override {if (clipThrow) throw std::runtime_error("clip query"); return clip;}
    bool setVerticalScale(int,double,QString &) override {throw std::runtime_error("descent changed scale");}
    int triggerChannel() override {return source;}
    bool selectEdgeTrigger(QString &) override {trace<<"edge";return true;}
    bool setEdgeTriggerChannel(int channel,QString &e) override {trace<<"source";source=channel;if(rejectSource){e="source rejected";return false;}return true;}
    bool setEdgeSlope(ScopeTriggerSlope value,QString &) override {require(value==ScopeTriggerSlope::Fall,"not Falling");return true;}
    bool setTriggerMode(ScopeTriggerMode value,QString &) override {require(value==ScopeTriggerMode::Normal,"AUTO used");return true;}
    bool setChannelTriggerLevel(int channel,double level,QString &) override {require(channel==4,"wrong trigger source");levels<<level;return true;}
    bool stopAcquisition(QString &e) override {armed=false;trace<<"stop";if(displayWritten&&displayFault=="cleanup"){e="cleanup stop failed";return false;}return true;}
    void stop() override {armed=false;}
    bool beginSingleAcquisition() override {
        trace<<"arm";armed=true;event=false;ready=false;++arms;armTimer.start();
        if(singleStarted)singleStarted->storeRelease(1);
        return !failArm;
    }
    ScopeTriggerState triggerState() override {
        if(measureSingleWait) {
            readinessTimes<<armTimer.elapsed();
            require(armTimer.elapsed()>=3000,"READY queried before 3000 ms processing wait");
        }
        if(early){event=true;return earlyState;}
        ready=true;trace<<"ready";return ScopeTriggerState::Ready;
    }
    ScopeAcquisitionState acquisitionState() override {
        if(commError)return ScopeAcquisitionState::Invalid;
        if(event&&late){QThread::msleep(30);late=false;}
        return event&&arms!=missArm?ScopeAcquisitionState::Completed:ScopeAcquisitionState::Running;
    }
    double readMeasurement(int channel,ScopeMeasurement metric) override {
        require(event,"read without event");require(!shortOn&&!powerOn,"read before OFF/short exit");
        require(isChannelEnabled(channel),"measurement read a hidden channel");
        trace<<"read";error.clear();
        if(channel==2&&metric==ScopeMeasurement::Mean&&readError&&arms>=readErrorArm){error="injected read";return NAN;}
        if(channel==2&&legalNa)return NAN;
        return channel*100+arms*10+int(metric);
    }
    bool power(bool on,QString &e) {
        trace<<(on?"on":"off");
        if(!on&&shortOn&&offFailure){e="OFF failure";return false;}
        powerOn=on;
        if(!on&&armed&&!event&&!shortMode)fire();
        return true;
    }
    bool relay(bool sh,bool dis,QString &e) {
        trace<<QString("relay %1 %2").arg(sh).arg(dis);
        if(dis&&dischargeFailure){e="discharge failure";return false;}
        shortOn=sh;
        if(sh&&!dis&&armed&&!event)fire();
        return true;
    }
    void fire() {require(ready,"event before READY");event=true;trace<<"event";if(cancel)cancel->storeRelease(1);}
    QStringList trace;QVector<double> levels;QString error;
    int source=1,disabled=0,otherDisabled=0,arms=0,missArm=0,readErrorArm=1;
    bool armed=false,event=false,ready=false,powerOn=false,shortOn=false,shortMode=false;
    bool rejectSource=false,failArm=false,early=false,commError=false,late=false,clip=false,clipThrow=false;
    bool readError=false,legalNa=false,offFailure=false,dischargeFailure=false;
    QAtomicInt *cancel=nullptr;
    QAtomicInt *displayStop=nullptr;
    QAtomicInt *singleStarted=nullptr;
    QElapsedTimer armTimer;
    QVector<qint64> readinessTimes;
    bool measureSingleWait=false;
    ScopeTriggerState earlyState=ScopeTriggerState::Triggered;
    bool displayWritten=false,allHidden=false;
    QString displayFault;
};
TransientContext configuration(Scope &scope)
{
    TransientContext c;c.descentChannel=4;c.descentLevels={15,5,10,5,0,-1.25};c.trialsPerLevel=2;
    c.settleMs=c.dischargeMs=c.powerOffMs=0;c.timeoutMs=20;c.phaseTimeoutMs=120000;
    c.power=[&](bool on,QString&e){return scope.power(on,e);};
    c.relays=[&](bool sh,bool dis,QString&e){return scope.relay(sh,dis,e);};
    c.load=[&](bool on,QString&){scope.trace<<(on?"load on":"load off");return true;};return c;
}

void singleWaitCase()
{
    for(bool shortTest:{false,true}) for(const QString mode:{"success","save","stop","phase","arm"}) {
        Scope scope;scope.shortMode=shortTest;scope.measureSingleWait=true;
        QAtomicInt stop{0},started{0};auto cfg=configuration(scope);
        cfg.descentLevels={10,5};cfg.trialsPerLevel=1;cfg.phaseTimeoutMs=60000;
        QStringList logs;cfg.log=[&](const QString &text){logs<<text;};
        if(mode=="save"){scope.early=true;scope.earlyState=ScopeTriggerState::Saving;}
        if(mode=="phase")cfg.phaseTimeoutMs=100;
        if(mode=="arm")scope.failArm=true;
        std::unique_ptr<QThread> cancelThread;
        if(mode=="stop") {
            scope.singleStarted=&started;
            cancelThread.reset(QThread::create([&]{
                QElapsedTimer waitForSingle;waitForSingle.start();
                while(!started.loadAcquire()&&waitForSingle.elapsed()<5000)QThread::msleep(1);
                QThread::msleep(100);stop.storeRelease(1);
            }));
            cancelThread->start();
        }
        DescentStrategy strategy(scope.shortMode,cfg);int diagnostics=0;
        strategy.setCaptureObserver([&](const ScopeCaptureRecord &record){
            if(mode=="save"){require(record.phase=="Before READY","SAVE diagnostic phase changed");++diagnostics;}
        });
        QElapsedTimer elapsed;elapsed.start();const auto result=strategy.execute(&scope,stop);
        if(cancelThread)cancelThread->wait();
        if(mode=="success") {
            require(result.success,qPrintable(result.errorMessage));
            require(result.trials.size()==2&&scope.arms==2&&scope.trace.count("event")==2,"wait lost/repeated Trial");
            require(scope.levels==QVector<double>({10,5}),"processing wait changed Level sequence");
            require(scope.readinessTimes.size()==2,"each Single must query READY");
            require(logs.count("Single processing wait: 3000 ms before READY")==2,"missing per-Single processing wait Log");
        } else {
            require(!result.success&&result.trials.isEmpty()&&scope.trace.count("event")==0,"wait failure produced event");
            if(mode=="save")require(result.errorMessage.contains("state=SAVE")&&diagnostics==1&&scope.arms==1,"SAVE accepted/rearmed or diagnostic lost");
            if(mode=="stop"||mode=="phase") {
                require(scope.readinessTimes.isEmpty()&&elapsed.elapsed()<2500,"Stop/Phase did not interrupt processing wait");
                require(result.errorMessage.contains(mode=="stop"?"Stopped by user":"phase time limit"),"wait failure reason lost");
            }
            if(mode=="arm")require(elapsed.elapsed()<2500&&!logs.contains("Single processing wait: 3000 ms before READY"),"failed Single entered processing wait");
        }
        require(!scope.powerOn&&!scope.armed&&!scope.shortOn&&scope.trace.last()=="load off"&&!result.cleanupFailed,"processing wait cleanup failed");
    }
}
void strategyCase(const QString &mode,bool shortTest)
{
    Scope scope;scope.shortMode=shortTest;QAtomicInt stop{0};auto cfg=configuration(scope);int captures=0;
    if(mode=="missing")cfg.descentLevels.clear();
    if(mode=="invalid")cfg.descentLevels={NAN};
    if(mode=="disabled")scope.disabled=4;
    if(mode=="channel")cfg.descentChannel=8;
    if(mode=="reject")scope.rejectSource=true;
    if(mode=="arm")scope.failArm=true;
    if(mode=="early")scope.early=true;
    if(mode=="miss")scope.missArm=2;
    if(mode=="comm")scope.commError=true;
    if(mode=="late")scope.late=true;
    if(mode=="clip")scope.clip=true;
    if(mode=="clip_query")scope.clipThrow=true;
    if(mode=="read")scope.readError=true;
    if(mode=="later_read"){scope.readError=true;scope.readErrorArm=2;}
    if(mode=="na")scope.legalNa=true;
    if(mode=="off")scope.offFailure=true;
    if(mode=="discharge")scope.dischargeFailure=true;
    if(mode=="cancel")scope.cancel=&stop;
    if(mode=="stopped")stop.storeRelease(1);
    if(mode=="deadline")cfg.phaseTimeoutMs=5000; // Reach the event before testing a late diagnostic Capture.
    std::unique_ptr<IOscilloscopeMeasureStrategy> strategy(OscilloscopeStrategyFactory::create(
        shortTest?"Turn on then short (descent)":"Turn off (descent)",&cfg));
    require(bool(strategy),"factory missing");
    strategy->setCaptureObserver([&](const ScopeCaptureRecord &r){
        require(!scope.shortOn&&(!shortTest||!scope.powerOn),"Capture before short exit");
        require(r.channels.size()==4,"enabled channels lost");++captures;scope.trace<<"capture";
        if(mode=="later_read"&&scope.arms==2) require(std::isnan(r.channels[2].maxVal),"diagnostic reused previous event values");
        if(mode=="capture")throw std::runtime_error("save failed");
        if(mode=="deadline")QThread::msleep(cfg.phaseTimeoutMs+10);
    });
    const auto result=strategy->execute(&scope,stop);
    if(mode=="success"||mode=="na"||(mode=="disabled"&&!shortTest)) {
        require(result.success&&!result.cleanupFailed,qPrintable(result.errorMessage));
        require(result.trials.size()==12&&scope.arms==12&&captures==12,"event counts");
        QVector<double> expected;for(double v:cfg.descentLevels)expected<<v<<v;
        require(scope.levels==expected,"levels sorted/deduplicated/modified");
        for(int i=0;i<result.trials.size();++i){const auto&t=result.trials[i];
            require(t.direction==MeasurementDirection::Fall&&t.step==i/2+1&&t.trial==i%2+1&&t.triggerLevel==expected[i],"provenance");
            require(t.channels.size()==4&&t.channels[0].maximum==100+(i+1)*10&&t.channels[0].mean==103+(i+1)*10,"four values mixed");
            if(mode=="na")require(std::isnan(t.channels[1].mean),"NA fabricated");
        }
    } else {
        require(!result.success,"fault passed");
        if(mode=="miss") require(result.trials.size()==1&&captures==1&&scope.arms==2,"miss lost partial data or continued");
        if(mode=="later_read") require(result.trials.size()==1&&captures==2&&scope.arms==2,"later read lost partial data/diagnostic");
        if(mode=="clip"||mode=="clip_query"||mode=="read"||mode=="late"||mode=="deadline")require(captures==1&&result.trials.isEmpty(),"diagnostic missing/fabricated valid trial");
        if(mode=="early")require(scope.trace.count("event")==0&&captures==1,"pre-READY event applied or diagnosis lost");
        if(mode=="off")require(result.cleanupFailed&&scope.shortOn&&captures==0&&scope.arms==1,"OFF failure released short");
        if(mode=="discharge")require(result.cleanupFailed&&scope.arms==0&&captures==0,"startup discharge failure continued");
        if(mode=="disabled"||mode=="channel"||mode=="missing"||mode=="invalid"||mode=="stopped")require(scope.trace.isEmpty(),"invalid settings operated outputs");
    }
    if(!scope.trace.isEmpty()) require(scope.trace.last()=="load off","cleanup missed Load OFF");
}
void serviceCase(const QString &mode, bool triggerEnabled = true, bool allHidden = false)
{
    for(bool sh:{false,true}) {
        if(sh&&allHidden)continue;
        if(mode=="service_off"&&!sh)continue;
        Scope scope;scope.shortMode=sh;scope.offFailure=mode=="service_off";
        scope.allHidden=allHidden;
        TestSequenceExecutionContext context;context.scope=&scope;
        context.inputs={{"1phase","120","60","0"}};context.loads={{"Load",{"1"}}};
        context.relays={{"Short",{"ON","OFF"}},{"Discharge",{"OFF","ON"}}};
        InstrumentConfig relay;relay.type="Relay";relay.enabled=true;relay.commConfig.protocol=ProtocolType::GPIB;
        relay.commConfig.gpibAddress=4;relay.channels={{"relay",1,0},{"relay",2,0}};
        context.instrumentSetup.instruments={relay};context.instrumentSetup.relayOutputs=2;
        const QString name=sh?"Turn on then short (descent)":"Turn off (descent)";
        TaskPayload task;task.task.name=name;task.settings=testSequenceDefaultSettings(name).first().toMap();
        task.settings["ac_input_index"]=0;task.settings["load_index"]=0;
        task.settings["delay_ms"]=0;task.settings["discharge_ms"]=1;
        task.settings["trig_timeout_ms"]=20;task.settings["phase_timeout_ms"]=60000;
        task.settings["trigger_levels"]=QVariantList{10.0,5.0};
        if(!sh)task.settings["trigger_channel_enabled"]=triggerEnabled;
        if(sh){task.settings["relay_index"]=0;task.settings["discharge_index"]=1;}
        TestRunDependencies deps;int creations=0;
        deps.validateMeasurement=[](const auto&,auto,int,bool){};
        deps.instruments.input=[&](const auto&,const auto&,InputAction action){
            QString error;return InstrumentOperationResult{scope.power(action==InputAction::PowerOn,error),error};};
        deps.instruments.load=[&](const auto&,const auto&,int,const auto&,LoadAction action,bool){
            scope.trace<<(action==LoadAction::LoadOn?"load on":"load off");return InstrumentOperationResult{};};
        deps.instruments.relay=[&](const auto&,const auto& rows,int index,RelayAction action){
            QString error;const bool off=action==RelayAction::RelayOff;
            return InstrumentOperationResult{scope.relay(!off&&rows[index].values[0]=="ON",!off&&rows[index].values[1]=="ON",error),error};};
        deps.strategy=[&](const MeasurementStrategyRequest &request)->std::unique_ptr<IMeasurementStrategy>{
            ++creations;const auto &r=std::get<ScopeMeasurementRequest>(request);
            if(!sh)require(r.context.descentChannelEnabled==triggerEnabled,"Service lost Trigger Enable");
            std::unique_ptr<IOscilloscopeMeasureStrategy> algorithm(OscilloscopeStrategyFactory::create(r.taskName,&r.context));
            require(bool(algorithm),"Service factory missing");algorithm->setCaptureObserver(r.capture);
            return std::make_unique<ScopeMeasurementStrategy>(std::move(algorithm),r.scope);};
        TestRunService service(deps);QVector<TestSequenceResultRecord> records;bool held=false;
        QObject::connect(&service,&TestRunService::logMessage,[](const QString &message){std::cerr<<message.toStdString()<<'\n';});
        QObject::connect(&service,&TestRunService::resultRecordsReady,[&](const auto&r){records=r;});
        QObject::connect(&service,&TestRunService::outputsUncertain,[&](const auto&,const auto&,const ShortCircuitProtection&p){held=p.active()&&p.shortMayBeOn;});
        QVector<TaskPayload> tasks{task};
        if(mode=="preflight") {auto invalid=task;invalid.settings["trigger_levels"]=QVariantList{"5"};tasks<<invalid;}
        if(mode=="service_off") {tasks[0].retry=2;tasks<<task;}
        service.startTasks(tasks,context);
        if(mode=="preflight")require(creations==0&&scope.trace.isEmpty()&&records.size()==2&&records[0].status=="Skipped"&&records[1].status=="Fail","bad later row allowed IO");
        else if(mode=="service_off")require(creations==1&&scope.arms==1&&scope.shortOn&&held&&records.size()==2&&records[0].status=="Fail"&&records[1].status=="Skipped","Service retried or bypassed short protection");
        else {
            require(records.size()==1&&records[0].status=="Pass"&&records[0].trials.size()==2,"Service dispatch/results");
            if(!sh)for(const auto &step:scope.trace)require(!step.startsWith("relay"),"Turn off descent operated Relay");
            if(allHidden) {
                require(records[0].channels.isEmpty(),"trigger-only Service fabricated summary");
                for(const auto &trial:records[0].trials)require(trial.channels.isEmpty(),"trigger-only Service fabricated Trial");
                continue;
            }
            const auto trials=TestSequenceResultRows::rows(records[0],false);
            const int channels=sh||triggerEnabled?4:3;
            require(trials.size()==channels*2&&trials[0][TestSequenceResultRows::Level].toDouble()==10&&trials[channels][TestSequenceResultRows::Level].toDouble()==5,"report order/values");
            if(!sh&&!triggerEnabled)for(const auto &trial:records[0].trials)for(const auto &ch:trial.channels)
                require(ch.channel!=4,"hidden Trigger CH leaked to report");
            const auto summary=TestSequenceResultRows::rows(records[0],true);bool notRun=false;
            for(const auto &row:summary)if(row[TestSequenceResultRows::Direction]=="Rise")notRun=row[TestSequenceResultRows::Max]=="Not run";
            require(notRun,"descent report claims Rise ran");
        }
    }
}
void uiCase()
{
    for(bool sh:{false,true}){
        const QString name=sh?"Turn on then short (descent)":"Turn off (descent)";
        const auto group=TestSequenceTaskKind::settingsGroup(name);
        auto cfg=testSequenceDefaultSettings(name).value(group).toMap();
        cfg["trigger_levels"]=QVariantList{10.0,15.0,0.0,-1.25,10.0};
        DescentDialog dialog(sh,{{"AC"},{"DC"}},{"Load"},{{"Short",{"ON","OFF"}},{"Discharge",{"OFF","ON"}}},cfg,1);
        auto *enabled = dialog.findChild<QCheckBox*>("triggerChannelEnabled");
        require(sh ? !enabled : enabled && enabled->isChecked(), "Trigger Enable missing/default/scope");
        if (enabled) enabled->setChecked(false);
        require(!dialog.findChild<QComboBox*>("measureTarget")&&!dialog.findChild<QWidget*>("ate-channels"),"extra channel/search selector");
        dialog.show();QApplication::processEvents();
        dialog.grab().save(TestEvidence::path(QString("descent/%1.png").arg(sh?"short-dialog":"off-dialog")));
        dialog.findChild<QToolButton*>("advancedToggle")->click();QApplication::processEvents();
        auto *scroll=dialog.findChild<QScrollArea*>();scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QApplication::processEvents();
        dialog.grab().save(TestEvidence::path(QString("descent/%1-advanced.png").arg(sh?"short-dialog":"off-dialog")));
        auto *table=dialog.findChild<QTableWidget*>("triggerLevels");
        require(table->item(0,1)->text()=="10"&&table->item(1,1)->text()=="15","initial order changed");
        for(auto *button:dialog.findChildren<QPushButton*>())
            require(button->text()!=QString::fromUtf8("↑")&&button->text()!=QString::fromUtf8("↓"),"move buttons remain");
        dialog.findChild<QPushButton*>("addLevel")->click();
        require(!dialog.findChild<QPushButton*>("applyBtn")->isEnabled(),"blank coerced to zero");
        table->item(5,1)->setText("1.23456");require(!dialog.findChild<QPushButton*>("applyBtn")->isEnabled(),"precision");
        table->item(5,1)->setText("-0.0001");
        dialog.findChild<QComboBox*>("triggerChannel")->setCurrentIndex(2);
        dialog.findChild<QPushButton*>("applyBtn")->click();
        require(dialog.result()==QDialog::Accepted,"Apply rejected valid");
        const auto saved=dialog.config();
        require(sh ? !saved.contains("trigger_channel_enabled") :
                    saved.value("trigger_channel_enabled").metaType().id()==QMetaType::Bool &&
                    !saved.value("trigger_channel_enabled").toBool(), "Enable Apply not saved");
        require(saved.value("trigger_channel")==3&&!saved.contains("measure_target"),"source not saved or search leaked");
        const QVariantList expected{10.0,15.0,0.0,-1.25,10.0,-0.0001};
        require(saved.value("trigger_levels").toList()==expected,"level order/precision changed");
        TestSequenceModel model;DutRowData row;row.item=name;row.settings={{group,saved}};model.appendTask(row);
        QString xml;QXmlStreamWriter out(&xml);model.writeXml(out);
        TestSequenceModel loaded;QXmlStreamReader in(xml);in.readNextStartElement();loaded.loadXml(in);
        require(!in.hasError()&&loaded.tasks().size()==1,"XML rejected new task");
        require(loaded.tasks()[0].settings[group].toMap()["trigger_levels"].toList()==expected,"XML changed list");
        if (!sh) {
            const auto restored = loaded.tasks()[0].settings[group].toMap();
            require(restored.value("trigger_channel_enabled").metaType().id()==QMetaType::Bool &&
                    !restored.value("trigger_channel_enabled").toBool(), "XML lost false Enable");
            DescentDialog reopened(false,{},{},{},restored,1);
            require(!reopened.findChild<QCheckBox*>("triggerChannelEnabled")->isChecked(), "Dialog restore lost false");
        }
        DescentDialog cancelled(sh,{},{},{},saved,1);cancelled.findChild<QComboBox*>("triggerChannel")->setCurrentIndex(0);
        if (!sh) cancelled.findChild<QCheckBox*>("triggerChannelEnabled")->setChecked(true);
        cancelled.findChild<QPushButton*>("deleteLevel_1")->click();
        auto *remaining=cancelled.findChild<QTableWidget*>("triggerLevels");
        require(remaining->rowCount()==5&&remaining->item(1,0)->text()=="2"&&remaining->item(1,1)->text()=="0","delete/renumber changed wrong level");
        cancelled.findChild<QPushButton*>("closeBtn")->click();require(cancelled.config()==saved,"Close committed changes");
        for(const QVariant &bad:{QVariant(QVariantList{true}),QVariant(QVariantList{"10"}),QVariant("10,5")}){
            auto invalid=saved;invalid["trigger_levels"]=bad;QString error;
            require(!TestSequenceSettingsValidation::config(group,QJsonObject::fromVariantMap(invalid),error),"XML accepted bad levels");
            bool rejected=false;try{TaskSettings::decode(name,invalid);}catch(...){rejected=true;}require(rejected,"Run accepted bad levels");
        }
        auto empty=saved;empty["trigger_levels"]=QVariantList{};bool rejected=false;
        try{TaskSettings::decode(name,empty);}catch(...){rejected=true;}require(rejected,"empty list executed");
    }
}
void triggerEnableSettings()
{
    const QString name="Turn off (descent)", group=TestSequenceTaskKind::settingsGroup(name);
    auto cfg=testSequenceDefaultSettings(name).value(group).toMap();
    for(bool enabled:{false,true}) {
        cfg["trigger_channel_enabled"]=enabled;
        const auto decoded=TaskSettings::decode(name,cfg);
        require(std::get<TaskSettings::Transient>(decoded.settings).descent->channelEnabled==enabled,"typed Enable changed");
        TestSequenceModel model;DutRowData row;row.item=name;row.settings={{group,cfg}};model.appendTask(row);
        QString xml;QXmlStreamWriter writer(&xml);model.writeXml(writer);
        TestSequenceModel loaded;QXmlStreamReader reader(xml);reader.readNextStartElement();loaded.loadXml(reader);
        require(!reader.hasError()&&loaded.tasks().first().settings[group].toMap()["trigger_channel_enabled"]==enabled,"bool XML roundtrip");
        const auto before=loaded.tasks();int notices=0;
        QObject::connect(&loaded,&TestSequenceModel::configLoaded,[&]{++notices;});
        const QString quote=xml.contains("&quot;trigger_channel_enabled&quot;")?"&quot;":"\"";
        const QString key=quote+"trigger_channel_enabled"+quote+":";
        const QString token=key+(enabled?"true":"false");
        for(const QString replacement:{key+"1",token+","+token}) {
            QString bad=xml;bad.replace(token,replacement);require(bad!=xml,"invalid XML fixture unchanged");
            QXmlStreamReader badReader(bad);badReader.readNextStartElement();loaded.loadXml(badReader);
            require(badReader.hasError()&&!notices&&loaded.tasks().first().uid==before.first().uid&&
                    loaded.tasks().first().settings==before.first().settings,"invalid/duplicate Enable modified model");
        }
        DescentDialog escaped(false,{},{},{},cfg,1);
        escaped.findChild<QCheckBox*>("triggerChannelEnabled")->setChecked(!enabled);
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&escaped,&escape);
        require(escaped.result()==QDialog::Rejected&&escaped.config()==cfg,"Escape committed Enable");
    }
    cfg.remove("trigger_channel_enabled");
    require(std::get<TaskSettings::Transient>(TaskSettings::decode(name,cfg).settings).descent->channelEnabled,"missing Enable not true");
    DescentDialog legacy(false,{},{},{},cfg,1);
    require(legacy.findChild<QCheckBox*>("triggerChannelEnabled")->isChecked(),"legacy dialog not checked");
    for(const QVariant bad:{QVariant(1),QVariant("false"),QVariant(QVariantList{}),QVariant()}) {
        cfg["trigger_channel_enabled"]=bad;QString error;
        require(!TestSequenceSettingsValidation::config(group,QJsonObject::fromVariantMap(cfg),error),"XML accepted non-bool Enable");
        bool rejected=false;try{TaskSettings::decode(name,cfg);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"typed accepted non-bool Enable");
    }
    const QString shortName="Turn on then short (descent)";
    cfg=testSequenceDefaultSettings(shortName).first().toMap();cfg["trigger_channel_enabled"]=false;QString error;
    require(!TestSequenceSettingsValidation::config(TestSequenceTaskKind::settingsGroup(shortName),QJsonObject::fromVariantMap(cfg),error),"short XML accepted Enable");
    bool rejected=false;try{TaskSettings::decode(shortName,cfg);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"short typed accepted Enable");
}

void triggerDisplayCases()
{
    for(bool initial:{false,true})for(bool enabled:{false,true}) {
        Scope scope;scope.disabled=initial?0:4;scope.otherDisabled=2;QAtomicInt stop{0};auto cfg=configuration(scope);
        cfg.descentChannelEnabled=enabled;
        // Level order/repetition is covered by strategyCase; this matrix covers display state across attempts.
        cfg.descentLevels={5};cfg.trialsPerLevel=1;
        DescentStrategy strategy(false,cfg);int captures=0;
        strategy.setCaptureObserver([&](const ScopeCaptureRecord &r){
            require(r.channels.size()==(enabled?3:2),"Capture channel set differs from Enable");++captures;
        });
        // Reuse the same instrument for a second attempt, explicitly reapply the setting.
        for(int attempt=0;attempt<2;++attempt) {
            const auto result=strategy.execute(&scope,stop);
            require(result.success&&result.trials.size()==1,qPrintable(result.errorMessage));
            for(const auto &t:result.trials) {
                require(t.channels.size()==(enabled?3:2),"result channel set differs from Enable");
                for(const auto &ch:t.channels)require(ch.channel!=2&&(enabled||ch.channel!=4),"hidden CH measured");
            }
            require(scope.source==4&&scope.isChannelEnabled(4)==enabled,"trigger source/display not retained");
        }
        require(captures==2&&scope.trace.count(enabled?"display on":"display off")==2,"attempt did not reapply display");
        require(scope.trace.indexOf("stop")<scope.trace.indexOf(enabled?"display on":"display off")&&
                scope.trace.indexOf("source")<scope.trace.indexOf("on"),"setup order");
    }
    for(const QString fault:{"reject","throw","mismatch","read","cancel","cleanup"}) {
        Scope scope;QAtomicInt stop{0};auto cfg=configuration(scope);cfg.descentChannelEnabled=false;
        scope.displayFault=fault;if(fault=="cancel"||fault=="cleanup")scope.displayStop=&stop;
        DescentStrategy strategy(false,cfg);const auto result=strategy.execute(&scope,stop);
        require(!result.success&&scope.arms==0&&!scope.trace.contains("on")&&!scope.trace.contains("load on"),"display fault continued to event");
        require(scope.trace.last()=="load off"&&scope.trace.count("stop")>=2&&!scope.powerOn,"display fault cleanup missing");
        if(fault=="cleanup")require(result.cleanupFailed,"cleanup failure lost");
        if(fault=="cancel")require(result.errorMessage.contains("Stopped"),"display Stop lost");
        if(fault=="mismatch")require(result.errorMessage.contains("readback mismatch"),"mismatch not identified");
    }
    // No displayed channels is a valid trigger-only run, never fabricated measurement data.
    Scope scope;scope.allHidden=true;QAtomicInt stop{0};auto cfg=configuration(scope);cfg.descentChannelEnabled=false;
    cfg.descentLevels={5};cfg.trialsPerLevel=1;
    DescentStrategy strategy(false,cfg);const auto result=strategy.execute(&scope,stop);
    require(result.success&&result.trials.size()==1&&!scope.trace.contains("read"),"all-hidden trigger-only run failed");
    for(const auto &trial:result.trials)require(trial.channels.isEmpty(),"all-hidden fabricated values");
}

void triggerCaptureCases()
{
    for(const QString format:{"PNG","AllWFM","AllCSV","WFM","CSV"})for(int selected:{0,1,4}) {
        QTemporaryDir dir;require(dir.isValid(),"temporary capture directory");
        Scope scope;QAtomicInt stop{0};auto cfg=configuration(scope);cfg.descentChannelEnabled=false;
        cfg.descentLevels={5};cfg.trialsPerLevel=1;
        CaptureOptions options;options.formats={format};options.channel=selected;
        StrategyWaveformCapture capture(options,dir.path(),stop,"AC","DC","Turn off (descent)",
                                       {1,1,QDateTime::currentDateTime()});
        DescentStrategy strategy(false,cfg);
        strategy.setCaptureObserver([&](const ScopeCaptureRecord &r){capture.record(&scope,r,{});});
        const auto result=strategy.execute(&scope,stop);
        const bool unavailable=(format=="WFM"||format=="CSV")&&selected!=1;
        require(result.success!=unavailable,"hidden trigger Capture result wrong");
        if(unavailable)require(result.errorMessage.contains("selected channel is unavailable")&&capture.files().isEmpty(),"hidden trigger silently captured");
        else require(capture.files().size()==(format.startsWith("All")?3:1),"visible channel Capture files missing");
        require(scope.disabled==4&&!scope.powerOn&&scope.trace.last()=="load off","Capture changed display or lost cleanup");
    }
}

void precisionCase()
{
    const QVariantList values{0.0, 0.0001, -0.0001, 1.2345, -1.2345, 999999999.9999, 1e9, -1e9, 0.0001};
    for (bool sh : {false, true}) {
        const QString name = sh ? "Turn on then short (descent)" : "Turn off (descent)";
        const auto group = TestSequenceTaskKind::settingsGroup(name);
        auto cfg = testSequenceDefaultSettings(name).value(group).toMap(); cfg["trigger_levels"] = values;
        DescentDialog dialog(sh, {}, {}, {}, cfg, 1);
        auto *table = dialog.findChild<QTableWidget*>("triggerLevels");
        auto *apply = dialog.findChild<QPushButton*>("applyBtn");
        require(apply->isEnabled(), "legal four-decimal XML cannot be applied");
        for (int i = 0; i < values.size(); ++i) {
            require(!table->item(i, 1)->text().contains('e', Qt::CaseInsensitive), "legal level restored as exponent");
            require(table->item(i, 1)->text().toDouble() == values[i].toDouble(), "restored level lost precision");
        }
        apply->click(); require(dialog.result() == QDialog::Accepted, "real Apply failed");
        require(dialog.config()["trigger_levels"].toList() == values, "Apply rounded values");
        TestSequenceModel source; DutRowData row; row.item = name; row.settings = {{group, dialog.config()}}; source.appendTask(row);
        QString xml; QXmlStreamWriter writer(&xml); source.writeXml(writer);
        TestSequenceModel loaded; QXmlStreamReader reader(xml); reader.readNextStartElement(); loaded.loadXml(reader);
        require(!reader.hasError() && loaded.tasks()[0].settings[group].toMap()["trigger_levels"].toList() == values, "precision XML roundtrip");
        DescentDialog reopened(sh, {}, {}, {}, loaded.tasks()[0].settings[group].toMap(), 1);
        reopened.findChild<QPushButton*>("applyBtn")->click();
        require(reopened.result() == QDialog::Accepted && reopened.config()["trigger_levels"].toList() == values, "reopened XML is not editable");
        for (double bad : {0.00001, -0.00001, 1.23456, std::nextafter(0.0001, 1.0), 1e9 + 0.0001}) {
            auto invalid = cfg; invalid["trigger_levels"] = QVariantList{bad}; QString error;
            require(!TestSequenceSettingsValidation::config(group, QJsonObject::fromVariantMap(invalid), error), "XML accepted extra precision");
            bool rejected = false; try { TaskSettings::decode(name, invalid); } catch (...) { rejected = true; }
            require(rejected, "Run accepted extra precision");
            DescentDialog invalidDialog(sh, {}, {}, {}, invalid, 1);
            require(!invalidDialog.findChild<QPushButton*>("applyBtn")->isEnabled(), "invalid level rounded by dialog");
            Scope scope; auto context = configuration(scope); context.descentLevels = {bad}; QAtomicInt stop{0};
            DescentStrategy strategy(sh, context); const auto result = strategy.execute(&scope, stop);
            require(!result.success && scope.trace.isEmpty(), "direct strategy accepted invalid precision");
        }
        // Invalid precision must leave existing UID/data intact (JSON may serialize small values as exponents).
        QString invalidXml = xml; invalidXml.replace("1.2345", "1.23456");
        require(invalidXml != xml, "invalid XML fixture did not change");
        const auto before = loaded.tasks(); int notifications = 0;
        QObject::connect(&loaded, &TestSequenceModel::configLoaded, [&] { ++notifications; });
        QXmlStreamReader invalidReader(invalidXml); invalidReader.readNextStartElement(); loaded.loadXml(invalidReader);
        require(invalidReader.hasError() && notifications == 0 && loaded.tasks()[0].uid == before[0].uid &&
                loaded.tasks()[0].settings == before[0].settings, "invalid precision XML changed current model");
    }
}
class Transport final:public ICommunication {
public:
    bool open()override{return true;}void close()override{}bool isOpen()const override{return true;}
    QString lastError()const override{return "injected write failure";}
    int write(const QByteArray&bytes)override{last=QString::fromUtf8(bytes).trimmed();commands<<last;consumed=false;return last==fail?-1:bytes.size();}
    int read(QByteArray&bytes,int)override{if(consumed){bytes.clear();return 0;}consumed=true;if(last!="*ESR?")return -1;bytes=rejected?"32":"0";return bytes.size();}
    QString last,fail;QStringList commands;bool consumed=false,rejected=false;
};
void driverCase(){
    Transport comm;MSOSeries456 scope(&comm);QString error;
    require(scope.setEdgeTriggerChannel(4,error),qPrintable(error));require(comm.commands.contains("TRIGger:A:EDGE:SOUrce CH4"),"source SCPI");
    const auto size=comm.commands.size();require(!scope.setEdgeTriggerChannel(0,error)&&comm.commands.size()==size,"bad channel sent");
    comm.fail="TRIGger:A:EDGE:SOUrce CH4";require(!scope.setEdgeTriggerChannel(4,error),"write error ignored");
    comm.fail.clear();comm.rejected=true;require(!scope.setEdgeTriggerChannel(4,error),"ESR rejection ignored");
    for(bool enabled:{false,true}) {
        Transport display;MSOSeries456 driver(&display);
        const QString command=QString("DISplay:GLObal:CH4:STATE %1").arg(enabled?"ON":"OFF");
        require(driver.setChannelDisplay(4,enabled,error)&&display.commands.contains(command),"display command missing");
        const auto count=display.commands.size();
        require(!driver.setChannelDisplay(0,enabled,error)&&!driver.setChannelDisplay(5,enabled,error)&&display.commands.size()==count,"invalid display CH sent");
        display.fail=command;require(!driver.setChannelDisplay(4,enabled,error)&&!error.isEmpty(),"display write failure ignored");
        display.fail.clear();display.rejected=true;
        require(!driver.setChannelDisplay(4,enabled,error)&&!error.isEmpty(),"display ESR rejection ignored");
    }
}
int main(int argc,char**argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/arial.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
    app.setFont(QFont("Arial",10));
    try{const QString mode=argc>1?argv[1]:"success";
        if(mode=="single_wait")singleWaitCase();
        else if(mode=="ui"){uiCase();triggerEnableSettings();}else if(mode=="precision")precisionCase();else if(mode=="driver")driverCase();
        else if(mode=="service"||mode=="preflight"||mode=="service_off"){
            serviceCase(mode);if(mode=="service"){serviceCase(mode,false);serviceCase(mode,false,true);}}
        else for(bool sh:{false,true}){if(!sh&&(mode=="off"||mode=="discharge"))continue;strategyCase(mode,sh);}
        if(mode=="disabled")triggerDisplayCases();
        if(mode=="capture")triggerCaptureCases();
        std::cout<<"PASS "<<mode.toStdString()<<'\n';return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
