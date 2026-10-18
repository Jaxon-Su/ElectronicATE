#define main original_brownout_main
#include "brownout_test.cpp"
#undef main
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

class DiagnosticSession : public Session {
public:
    using Session::Session;
    BrownOutRecordStatus poll(BrownOutRecord &record,QString &error) override {
        const auto status=Session::poll(record,error);
        if(armed && status==BrownOutRecordStatus::Ready) {error="CURVe read failed";return BrownOutRecordStatus::Error;}
        return status;
    }
};
class CaptureScope final : public Scope {
public:
    CaptureScope(State &state, QString scenario) : Scope(state), scenario(scenario) {}
    std::unique_ptr<IScopeBrownOutSession> beginBrownOut(const BrownOut::Settings &cfg,QAtomicInt &stop,QString &error) override {
        if(scenario=="query_failure")return std::make_unique<DiagnosticSession>(s,cfg);
        return Scope::beginBrownOut(cfg,stop,error);
    }
    QString lastError() const override { return {}; }
    ScopeAcquisitionState acquisitionState() override { return ScopeAcquisitionState::Completed; }
    ScopeCaptureSource captureSource() override { return {"CH4", 4}; }
    QString captureSlope() override { return "FALL"; }
    double getTriggerLevel() override { return 8; }
    double getTimebase() override { return .01; }
    double getChannelScale(int) override { return 1; }
    double getChannelPosition(int) override { return 0; }
    bool readChannelOffset(int, double &value, QString &) override { value=0; return true; }
    int getTotalChannel() override { return 4; }
    bool isChannelEnabled(int channel) override { return channel==4; }
    bool waitForOperationComplete(int) override { return true; }
    QByteArray captureScreenshot(const QString &, const QString &) override {
        ++saved;
        if (saved==2 && scenario=="failure") return {};
        if (saved==2 && scenario=="cancel") cancel();
        return "png";
    }
    QString scenario;
    std::function<void()> cancel;
    int saved=0;
};
int main(int argc, char **argv) {
    QCoreApplication app(argc,argv);
    try {
        const QString scenario=app.arguments().value(1);
        State state; QAtomicInt stop{0}; state.stop=&stop;
        CaptureScope scope(state,scenario);
        QTemporaryDir directory;
        TestRunDependencies deps;
        deps.validateMeasurement=[](const auto&,auto,int,bool){};
        deps.prepareBrownOut=[&](const auto&,const auto&){return [&]{return std::make_unique<Io>(state);};};
        deps.strategy=makeMeasurementStrategyFactory({});
        TestRunService service(deps);
        TestSequenceExecutionContext context;
        context.scope=&scope;context.reportDirectory=directory.path();
        context.inputs={{"1phase","90","50","0"}};context.loads={{"Full / Load",{"1"}}};
        TaskPayload task;task.task.name="Brown Out";task.settings=settings();
        task.settings["capture_enabled"]=true;
        task.settings["capture_settings"]=QVariantMap{{"formats",QStringList{"PNG"}}};
        if(scenario=="track"||scenario=="query_failure")task.settings["mode"]="TRACK";
        if(scenario=="preflight")task.settings["capture_settings"]=QVariantMap{{"directory",directory.filePath("missing")}};
        QVector<TestSequenceResultRecord> records;
        QObject::connect(&service,&TestRunService::resultRecordsReady,[&](const auto &value){records=value;});
        // Cancellation is injected by the scope via the actual service stop callback below.
        scope.cancel=[&]{service.stop();};
        service.prepareRun();service.startTasks({task},context);
        expect(records.size()==1,"missing task result");
        const auto &result=records.first();
        if(scenario=="preflight") {
            expect(result.attempt==0 && state.cleaned==0 && scope.saved==0,"invalid capture directory reached hardware");
            return 0;
        }
        expect(state.cleaned==1 && state.finish==1,"capture bypassed output/scope cleanup");
        const bool success=scenario=="fixed" || scenario=="track";
        expect((result.status=="Pass")==success,qPrintable(result.error));
        if(scenario=="cancel")expect(result.status=="Cancelled","capture stop not propagated");
        expect(result.files.size()==(success ? state.records+state.baseline : scenario=="query_failure"?2:1),"completed records lost or partial save published");
        if(scenario=="query_failure")expect(result.error.contains("CURVe read failed"),"diagnostic save hid the original failure");
        for(int i=0;i<result.files.size();++i){
            QFile file(result.files[i]);expect(file.open(QIODevice::ReadOnly),"missing PNG");
            expect(file.readAll()=="png","PNG content changed");
            expect(file.fileName().endsWith(QString("_%1.png").arg(i+1,4,10,QChar('0'))),"non-monotonic capture number/extra format");
            expect(QFileInfo(file).fileName().startsWith("AC1_1phase_90_50_0_DC_None_Load1_Full_Load_Brown_Out_"),"Brown Out condition filename missing");
        }
        expect(QDir(QFileInfo(result.files.first()).absolutePath()).entryList(QDir::Files).size()==result.files.size(),"PNG-only leaked extra files");
        std::cout<<"PASS Brown Out capture "<<scenario.toStdString()<<'\n';
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
