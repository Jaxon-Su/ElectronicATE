#include "testrunservice.h"
#include "oscilloscope.h"
#include "testsequencemodel.h"
#include "testsequencesettingsvalidation.h"
#include "testsequencetaskdefaults.h"
#include "testsequenceconditionmapping.h"
#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer>
#include <atomic>
#include <iostream>
#include <thread>

void expect(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
QVariantMap settings() {
    auto v = BrownOut::defaults();
    v["ignore_advanced"]=false; // Exercise custom timing in runner scenarios.
    v["ac_input_index"]=0; v["load_index"]=0; v["delay_ms"]=0;
    v["step_interval_ms"]=100; v["voltage_step"]=1.;
    v["shutdown_confirm_ms"]=100; v["tail_ms"]=100;
    v["data_timeout_ms"]=1000;
    return v;
}
struct State {
    QString scenario;
    bool multiChannel = false;
    std::atomic_int steps{0}, cleaned{0};
    std::atomic_bool ready{false}, started{false}, closed{false};
    QVector<double> levels;
    int records=0, baseline=0, finish=0;
    QAtomicInt *stop=nullptr;
};
class Io : public IBrownOutIo {
public:
    Io(State &s) : s(s), owner(std::this_thread::get_id()) {}
    ~Io() override { expect(owner==std::this_thread::get_id(),"I/O destroyed on wrong thread"); }
    void start(const std::function<bool()> &) override { s.started=true; if(s.scenario=="startup")throw std::runtime_error("startup failed"); }
    double initialVoltage() const override { return 5; }
    void setVoltage(double v) override {
        expect(s.ready,"ramp started before scope ready");
        expect(owner==std::this_thread::get_id(),"I/O moved threads");
        expect(v>=0&&v<=4,"ramp voltage out of range");
        ++s.steps;
        if(s.scenario=="cancel")s.stop->storeRelease(1);
    }
    double outputVoltage() override {
        if(s.scenario=="already_off")return 0;
        if(s.scenario=="monitor_error"&&s.steps>0)throw std::runtime_error("monitor failed");
        if(s.scenario=="timeout")return 12;
        if(s.steps<3)return 12;
        if(s.scenario=="rebound"&&++lowReads>3)return 12;
        return 0;
    }
    bool shutdown(QString &error) override { ++s.cleaned; s.closed=true;
        if(s.scenario=="cleanup_empty")return false;
        if(s.scenario=="cleanup") { error="output cleanup failed"; return false; }return true; }
    State &s; std::thread::id owner; int lowReads=0;
};
class Session : public IScopeBrownOutSession {
public:
    Session(State &s,BrownOut::Settings c):s(s),cfg(c){elapsed.start();}
    bool arm(QString &) override {armed=true;return true;}
    bool readyToRamp(QString &error) override {error.clear(); s.ready=true;return true;}
    BrownOutRecordStatus poll(BrownOutRecord &r,QString &error) override {
        if(s.scenario=="delayed_reference"&&!armed&&elapsed.elapsed()<1150)return BrownOutRecordStatus::Pending;
        if(!armed) {
            ++s.baseline; r={1,{{4,s.scenario=="current_trigger"?"A":"V",12.2,11.8,12.,12.}}};
            if(s.multiChannel)r.channels.append({2,"A",1.5,-1.,.75,.25});
            return BrownOutRecordStatus::Ready;
        }
        if(s.scenario=="long_record"&&elapsed.elapsed()<1800)return BrownOutRecordStatus::Pending;
        if(s.scenario=="no_trigger"||s.steps==0)return BrownOutRecordStatus::Pending;
        if(s.scenario=="scope_error") {error="waveform read failed";return BrownOutRecordStatus::Error;}
        if(cfg.mode==BrownOut::Mode::FixedFall&&s.steps<3)return BrownOutRecordStatus::Pending;
        const int seq=s.steps.load();
        if(seq==last)return BrownOutRecordStatus::Pending;last=seq;++s.records;
        r.sequence=s.scenario=="stale_reference"?1:seq+1;
        r.channels={{4,s.scenario=="unit_changed"?"A":"V",12.+seq,-double(seq),double(seq),double(seq)}};
        if(s.multiChannel)r.channels.append({2,"A",3.141592653589793,-2.718281828459045,1.4142135623730951,0.});
        if(s.scenario=="invalid")r.channels[0].maximum=std::numeric_limits<double>::quiet_NaN();
        return BrownOutRecordStatus::Ready;
    }
    bool advanceLevel(double level,QString &) override {s.levels.append(level);return true;}
    double levelStep() const override{return .04;}
    int recordDurationMs() const override {
        if(s.scenario=="invalid_duration")return 0;
        return s.scenario=="delayed_reference"||s.scenario=="long_record"?2000:1;
    }
    bool finish(QString &error) override {
        ++s.finish;
        if(s.scenario=="slow_cleanup") {
            for(int i=0;i<100&&!s.closed.load();++i)QThread::msleep(10);
            expect(s.closed,"output OFF waited for slow scope cleanup");
        }
        if(s.scenario=="scope_cleanup"){error="scope cleanup failed";return false;}return true;
    }
    State&s;BrownOut::Settings cfg;bool armed=false;int last=0;QElapsedTimer elapsed;
};
class Scope:public Oscilloscope {
public:
    explicit Scope(State&s):s(s){}
    QString model()const override{return "MSOSeries456";}
    QString vendor()const override{return "Fake";}
    int getTotalChannel()override{return 4;}
    QByteArray captureScreenshot(const QString&,const QString&)override{throw std::runtime_error("Unexpected Capture");}
    std::unique_ptr<IScopeBrownOutSession> beginBrownOut(const BrownOut::Settings&cfg,QAtomicInt&,QString&error)override{
        expect(s.started,"reference started before input settling");
        if(s.scenario=="prepare_error"){error="preparation failed";return {};}
        return std::make_unique<Session>(s,cfg);
    }
    State&s;
};
void runner(const QString &scenario) {
    State s;s.scenario=scenario;QAtomicInt stop{0};s.stop=&stop;Scope scope(s);
    auto v=settings();
    if(scenario=="rise"||scenario=="fall"||scenario=="invalid") {v["mode"]="TRACK";v["direction"]=scenario=="rise"?"RISE":"FALL";}
    if(scenario=="rebound")v["tail_ms"]=500;
    BrownOut::Settings cfg(v);
    if(scenario=="timeout")cfg.timeoutMs=1500;
    if(scenario=="slow_ramp")cfg.timeoutMs=250;
    auto factory=makeMeasurementStrategyFactory({});
    auto strategy=factory(BrownOutMeasurementRequest{cfg,scope,[&]{return std::make_unique<Io>(s);}});
    expect(strategy&&strategy->name()=="Brown Out","Brown Out not exposed through common strategy");
    expect(!s.started&&!s.ready&&s.cleaned==0,"strategy construction operated hardware");
    auto result=strategy->execute(stop);
    expect(s.cleaned==1,"output cleanup not exactly once");
    const bool success=scenario=="fixed"||scenario=="rise"||scenario=="fall"||scenario=="slow_cleanup"||
                       scenario=="delayed_reference"||scenario=="long_record"||scenario=="slow_ramp";
    expect(result.success==success,qPrintable(result.error));
    if(success){
        expect(s.steps==3,"voltage continued descending during confirmation");
        expect(result.channels.size()==1&&result.channels[0].maximum==15&&result.channels[0].minimum==-3,"both extrema missing");
        expect(result.channels[0].rms==3&&result.channels[0].mean==3,"latest-record RMS/Mean missing");
        expect(!result.summary.isEmpty(),"result summary missing");
        if(cfg.mode==BrownOut::Mode::FixedFall)expect(s.levels.size()==1&&s.records==1,"Single repeated");
        else {expect(s.records>=2&&s.levels.size()==s.records+1,"tracking on stale record or missing update");
            for(int i=1;i<s.levels.size();++i)expect(scenario=="rise"?s.levels[i]>s.levels[i-1]:s.levels[i]<s.levels[i-1],"wrong direction");}
    }else expect(!result.error.isEmpty(),"failure has no diagnostic");
    if(scenario=="current_trigger")expect(s.steps==0&&result.channels.isEmpty(),"Fixed Vout Fall ramped with a current trigger");
    if(scenario=="unit_changed"||scenario=="stale_reference")
        expect(result.channels.isEmpty(),"invalid first event affected extrema");
    if(scenario=="timeout")expect(s.steps==5,"timeout did not permit descent to minimum Vin");
    if(scenario=="cleanup"||scenario=="cleanup_empty"||scenario=="scope_cleanup"||scenario=="prepare_error")expect(result.cleanupFailed,"cleanup failure lost");
    if(scenario=="cleanup"||scenario=="cleanup_empty")expect(result.outputCleanupFailed,"output cleanup uncertainty lost");
    if(scenario=="scope_cleanup"||scenario=="prepare_error")expect(!result.outputCleanupFailed,"scope-only failure marked outputs unknown");
}
void xml() {
    auto cfg=settings();QString error;
    expect(BrownOut::defaults()["voltage_step"].toDouble()==0.5,"new task step default wrong");
    expect(testSequenceDefaultSettings("Brown Out")["brownOut"].toMap()["ignore_advanced"].toBool(),"new task did not default to Ignore");
    auto implicitIgnore=cfg;implicitIgnore.remove("ignore_advanced");
    const BrownOut::Settings implicitDefaults(implicitIgnore);
    expect(implicitDefaults.dataTimeoutMs==10000&&implicitDefaults.confirmMs==500&&implicitDefaults.tailMs==1000,"missing Ignore did not use built-in timing");
    const BrownOut::Settings limits(cfg);
    expect(limits.minimumVin==0&&limits.timeoutMs==120000,"internal Brown Out limits changed");
    for(const auto &key:{"minimum_vin","timeout_ms","time_per_div","trigger_position"})
        expect(!cfg.contains(key),"removed field is still serialized by default");
    expect(TestSequenceTaskKind::fromName("Brown Out")==TestSequenceTaskKind::Kind::BrownOut,"catalog missing");
    const bool valid=TestSequenceSettingsValidation::config("brownOut",QJsonObject::fromVariantMap(cfg),error);
    expect(valid,qPrintable("config: "+error));
    expect(TestSequenceSettingsValidation::settings("Brown Out",QJsonObject::fromVariantMap(testSequenceDefaultSettings("Brown Out")),error),"draft invalid");
    TestSequenceExecutionContext ctx;ctx.inputs={{"1phase","90","50","0"}};ctx.loads={{"L1",{"1"}}};
    TestSequenceConditions::capture(cfg,ctx);
    TaskPayload payload;payload.task.name="Brown Out";payload.settings=cfg;
    const bool references=TestSequenceConditions::validate(payload,ctx,error);
    expect(references,qPrintable("references: "+error));
    cfg["ignore_advanced"]=true;
    cfg["data_timeout_ms"]=25000;cfg["shutdown_confirm_ms"]=1500;cfg["tail_ms"]=2500;
    TestSequenceModel model;DutRowData row;row.item="Brown Out";row.retry="0";row.settings={{"brownOut",cfg}};model.appendTask(row);
    QString text;QXmlStreamWriter writer(&text);model.writeXml(writer);
    TestSequenceModel restored;QXmlStreamReader reader(text);reader.readNextStartElement();restored.loadXml(reader);
    expect(!reader.hasError()&&restored.tasks().size()==1,"Brown Out XML load failed");
    expect(QJsonDocument::fromVariant(restored.tasks()[0].settings)==QJsonDocument::fromVariant(row.settings),"XML roundtrip differs");
    auto restoredCfg=restored.tasks()[0].settings["brownOut"].toMap();
    const BrownOut::Settings ignored(restoredCfg);
    expect(ignored.dataTimeoutMs==10000&&ignored.confirmMs==500&&ignored.tailMs==1000,"XML Ignore did not apply built-in timing");
    restoredCfg["ignore_advanced"]=false;
    const BrownOut::Settings custom(restoredCfg);
    expect(custom.dataTimeoutMs==25000&&custom.confirmMs==1500&&custom.tailMs==2500,"XML lost custom Advanced values");
    for(const QString mode:{"FIXED_FALL","TRACK"})for(bool ignore:{false,true}) {
        auto exact=cfg;exact["mode"]=mode;exact["direction"]="RISE";exact["ignore_advanced"]=ignore;
        exact["trigger_level"]=8.1234;exact["voltage_step"]=0.5678;exact["shutdown_voltage"]=0.1234;
        TestSequenceModel source;auto item=row;item.settings={{"brownOut",exact}};source.appendTask(item);
        source.reportName="Brown Out & 測試";source.reportDirectory="C:/report & data";
        QString text;QXmlStreamWriter out(&text);source.writeXml(out);
        expect(!out.hasError(),"Brown Out precision/Ignore save failed");
        TestSequenceModel target;QXmlStreamReader in(text);in.readNextStartElement();target.loadXml(in);
        expect(!in.hasError()&&target.tasks().size()==1&&target.tasks()[0].settings==item.settings&&
               target.reportName==source.reportName&&target.reportDirectory==source.reportDirectory,
               "Brown Out modes/precision/Ignore/report roundtrip changed");
        const BrownOut::Settings decoded(target.tasks()[0].settings["brownOut"].toMap());
        expect(decoded.rising==(mode=="TRACK")&&decoded.triggerLevel==8.1234&&decoded.voltageStep==0.5678&&
               decoded.dataTimeoutMs==(ignore?10000:25000),"roundtrip changed effective settings");
    }
    for(const auto &value:{QVariant(1),QVariant("true"),QVariant()}) {
        auto invalid=cfg;invalid["ignore_advanced"]=value;
        expect(!TestSequenceSettingsValidation::config("brownOut",QJsonObject::fromVariantMap(invalid),error),"non-boolean Ignore accepted");
    }
    for(const auto &key:{"auto_period","measure_target","obsolete"}){
        auto invalid=cfg;invalid[key]=true;expect(!TestSequenceSettingsValidation::config("brownOut",QJsonObject::fromVariantMap(invalid),error),"unsupported Brown Out setting accepted");}
    for(const auto &key:{"minimum_vin","timeout_ms","time_per_div","trigger_position"}) {
        auto invalid=cfg;invalid[key]=1.;
        expect(!TestSequenceSettingsValidation::config("brownOut",QJsonObject::fromVariantMap(invalid),error),"removed Brown Out field accepted in XML");
        TestSequenceModel rejected;DutRowData existing;existing.item="Delay";existing.retry="0";rejected.appendTask(existing);
        const auto before=rejected.tasks()[0].uid;
        auto invalidRow=row;invalidRow.settings={{"brownOut",invalid}};
        TestSequenceModel source;source.appendTask(invalidRow);QString badXml;QXmlStreamWriter badWriter(&badXml);source.writeXml(badWriter);
        QXmlStreamReader badReader(badXml);badReader.readNextStartElement();rejected.loadXml(badReader);
        expect(badReader.hasError()&&rejected.tasks().size()==1&&rejected.tasks()[0].uid==before,"invalid XML replaced live tasks");
    }
    for(const auto &value:{QVariant(-1),QVariant("1"),QVariant(true),QVariant(.5)}) {
        auto invalid=cfg;invalid["step_interval_ms"]=value;
        expect(!TestSequenceSettingsValidation::config("brownOut",QJsonObject::fromVariantMap(invalid),error),"invalid number accepted");}
    ctx.loads[0].values[0]="2";
    expect(!TestSequenceConditions::validate(payload,ctx,error),"changed load reference accepted");
}
void service(bool cleanupFailure) {
    State s;s.multiChannel=true;if(cleanupFailure)s.scenario="cleanup";
    QAtomicInt stop{0};s.stop=&stop;Scope scope(s);TestSequenceExecutionContext ctx;ctx.scope=&scope;
    ctx.loads.append(LoadDataRow{}); ctx.loads[0].label = "Brown Out load";
    TestRunDependencies dep; dep.validateMeasurement = [](const auto &, auto, int, bool) {};
    dep.strategy=makeMeasurementStrategyFactory({});
    dep.prepareBrownOut=[&](const auto&,const auto&) {return BrownOutIoFactory([&]{return std::make_unique<Io>(s);});};
    TestRunService run(dep);QVector<TestSequenceResultRecord> results;
    TestSequenceResultRecord presented;
    bool presentedSuccess=true;
    QObject::connect(&run,&TestRunService::measurementReady,[&](int,const TestSequenceResultRecord &r,bool success){presented=r;presentedSuccess=success;});
    QObject::connect(&run,&TestRunService::resultRecordsReady,[&](auto r){results=r;});
    TaskPayload p;p.task.name="Brown Out";p.settings=settings();
    run.startTasks({p},ctx);
    expect(results.size()==1&&results[0].status==(cleanupFailure?"Fail":"Pass")&&results[0].channels.size()==2&&results[0].files.isEmpty(),"Brown Out result/report integration failed");
    expect(presentedSuccess!=cleanupFailure,"measurementReady success flag lost");
    expect(presented.summary.contains("shutdown Vin setting")&&presented.summary==results[0].summary,"Tasks lost typed summary");
    if(cleanupFailure)expect(results[0].error.contains("output cleanup failed"),"cleanup error lost with partial values");
    for(const auto &record:{presented,results[0]}) {
        expect(record.channels.size()==2,"presented result lost channels");
        const auto &voltage=record.channels[0];const auto &current=record.channels[1];
        expect(voltage.channel==4&&voltage.unit=="V"&&voltage.maximum==15&&voltage.minimum==-3&&voltage.rms==3&&voltage.mean==3,"voltage channel mapping changed");
        expect(current.channel==2&&current.unit=="A"&&current.maximum==3.141592653589793&&current.minimum==-2.718281828459045&&current.rms==1.4142135623730951&&current.mean==0.,"current channel unit/precision/order/zero changed");
        expect(record.conditions.isEmpty(),"single-condition result unexpectedly has Group members");
        expect(record.loadCondition.index == 0 && record.loadCondition.label == "Brown Out load" &&
               record.loadCondition.kind == LoadConditionKind::Static, "Brown Out lost its load source");
    }
    dep.prepareBrownOut=[](const auto&,const auto&)->BrownOutIoFactory {throw std::invalid_argument("bad monitor");};
    int relay=0;dep.instruments.relay=[&](const auto&,const auto&,int,auto){++relay;return InstrumentOperationResult{};};
    TestRunService preflight(dep);TaskPayload earlier;earlier.task.name="Relay";earlier.settings={{"relay_index",0}};
    ctx.relays = {{"off", {"OFF"}}}; // Keep the earlier task valid so Brown Out preflight is exercised.
    preflight.startTasks({earlier,p},ctx);expect(relay==0,"preflight failed after earlier output control");
    dep.prepareBrownOut=[&](const auto&,const auto&) {return BrownOutIoFactory([&]{return std::make_unique<Io>(s);});};
    for (bool missingFactory : {true,false}) {
        dep.strategy=missingFactory ? MeasurementStrategyFactory{} : MeasurementStrategyFactory(
            [](const MeasurementStrategyRequest&)->std::unique_ptr<IMeasurementStrategy>{return {};});
        s.started=false;s.cleaned=0;
        TestRunService missing(dep);QString diagnostic;
        QObject::connect(&missing,&TestRunService::resultRecordsReady,[&](auto r){
            for(const auto &record:r)diagnostic+=record.error;
        });
        missing.startTasks(missingFactory?QVector<TaskPayload>{earlier,p}:QVector<TaskPayload>{p},ctx);
        expect(relay==0&&!s.started&&s.cleaned==0,"missing strategy enabled output");
        expect(diagnostic.contains(missingFactory?"Strategy factory":"missing strategy"),"missing strategy diagnostic lost");
    }
    // A future strategy may report valid values plus cleanup failure: never present Pass.
    struct CleanupResult final : IMeasurementStrategy {
        QString name() const override {return "Brown Out";}
        MeasurementResult execute(QAtomicInt &) override {
            MeasurementResult result{true,true,"cleanup failed","partial values",{{4,"V",15,-3,3,3}}};
            result.trials = {{1, MeasurementDirection::Fall, 1, 1, -1, result.channels}};
            return result;
        }
    };
    int attempts=0;
    dep.strategy=[&](const MeasurementStrategyRequest&)->std::unique_ptr<IMeasurementStrategy>{
        ++attempts;return std::make_unique<CleanupResult>();
    };
    TestRunService cleanupContract(dep);bool accepted=true;QVector<TestSequenceResultRecord> final;
    QObject::connect(&cleanupContract,&TestRunService::measurementReady,[&](int,const auto&,bool success){accepted=success;});
    QObject::connect(&cleanupContract,&TestRunService::resultRecordsReady,[&](auto records){final=records;});
    p.retry=2;
    cleanupContract.startTasks({p,earlier},ctx);
    expect(!accepted&&attempts==1&&relay==0&&final.size()==2&&final[0].status=="Fail"&&final[1].status=="Skipped",
           "cleanup failure presented Pass or continued retries/tasks");
    expect(final[0].channels.size()==1&&final[0].channels[0].maximum==15,"cleanup failure discarded partial values");
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{QString mode=app.arguments().value(1);if(mode=="xml")xml();else if(mode=="service"||mode=="service_cleanup")service(mode=="service_cleanup");else runner(mode);std::cout<<"OK\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
