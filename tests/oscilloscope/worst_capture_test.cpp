#include "strategywaveformcapture.h"
#include "oscilloscopeacquisition.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QThread>
#include <future>
#include <iostream>
#include <vector>
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
class Scope:public IScopeCapture, public IScopeMeasurement {
public:
    bool waitForOperationComplete(int) override { return true; }
    QString model()const override{return "MSOSeries456";}
    QString lastError() const override { return {}; }
    bool isConnected() const override { return true; }
    bool beginSingleAcquisition() override { return true; }
    ScopeTriggerState triggerState() override { return ScopeTriggerState::Ready; }
    ScopeAcquisitionState acquisitionState() override { return running?ScopeAcquisitionState::Running:ScopeAcquisitionState::Completed; }
    int triggerChannel() override { return 1; }
    bool selectEdgeTrigger(QString&) override { return true; }
    bool setEdgeSlope(ScopeTriggerSlope,QString&) override { return true; }
    bool setTriggerMode(ScopeTriggerMode,QString&) override { return true; }
    bool setChannelTriggerLevel(int,double value,QString&) override { level=value;return true; }
    bool stopAcquisition(QString&) override { running=false;return true; }
    bool setVerticalScale(int channel,double value,QString&) override { ++changes;scales[channel]=value;return true; }
    void stop() override {}
    ScopeCaptureSource captureSource()override{return {"CH1",1};}
    QString captureSlope()override{return "RISE";}
    double getTriggerLevel()override{return level;}
    double getTimebase()override{return .01;}
    double getChannelScale(int ch)override{return scales.value(ch,scale);}
    double getChannelPosition(int ch)override{return positions.value(ch);}
    int getTotalChannel()override{return 2;}
    bool isChannelEnabled(int ch)override{return ch!=disabled;}
    bool isClipping(int)override{return clipped;}
    bool readChannelOffset(int,double& offset,QString&)override {offset=0;return true;}
    double readMeasurement(int,ScopeMeasurement type)override{return type==ScopeMeasurement::Minimum?-10:100;}
    QByteArray captureScreenshot(const QString&,const QString&)override{++screenshots;QThread::msleep(captureDelayMs);return "screen";}
    bool captureWaveformFileToHost(int,const QString& path,const QString&,const QString&,int,int)override {
        ++waves;if(fail)return false;
        return BinaryFileStore::save(path,QByteArray::number(level)).succeeded();
    }
    QMap<int,double> scales,positions;
    double level=98,scale=100;
    int screenshots=0,waves=0,changes=0,captureDelayMs=0;
    int disabled=0;
    bool fail=false,running=false,clipped=false;
};

void checkAttemptDirectories()
{
    QTemporaryDir directory;
    QTemporaryDir fallback;
    require(directory.isValid() && fallback.isValid(), "temporary directories unavailable");
    CaptureOptions options;
    options.directory = directory.path(); options.channel = 1; options.formats = {"PNG", "WFM"};
    QAtomicInt stop(0);
    Scope scope;
    const ScopeCaptureRecord record{{{1, 100, -10, 20, 5}}};
    const auto startedAt = QDateTime(QDate(2026, 10, 15), QTime(17, 30, 25)).toUTC();
    const StrategyWaveformCapture::Attempt attempt{3, 1, startedAt};
    const QString base = "20261015_173025_Task03_Turn_on_then_short_Attempt01";
    const QDir parent(directory.path());
    auto makeCapture = [&](const StrategyWaveformCapture::Attempt &identity) {
        return StrategyWaveformCapture(options, fallback.path(), stop, "AC1", "DC_None", "Turn on then short", identity);
    };
    auto folderOf = [](const StrategyWaveformCapture &capture) {
        return QFileInfo(capture.files().first()).dir().dirName();
    };
    auto first = makeCapture(attempt);
    require(parent.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty(), "directory created before a completed record");
    first.record(&scope, record, {});
    first.record(&scope, record, {2, "Full", LoadConditionKind::Static});
    require(folderOf(first) == base && first.files().size() == 4, "readable name or per-attempt grouping incorrect");
    for (const auto &path : first.files())
        require(QFileInfo(path).absolutePath() == parent.filePath(base), "one attempt used multiple directories");
    require(QDir(fallback.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty(), "explicit Capture directory ignored");

    // Save the exact old contents, then exercise every new-attempt path against them.
    QMap<QString, QByteArray> oldFiles;
    for (const auto &path : first.files()) {
        QFile file(path); require(file.open(QIODevice::ReadOnly), "old capture missing");
        oldFiles.insert(path, file.readAll());
    }
    auto retry = makeCapture({3, 2, startedAt});
    retry.record(&scope, record, {});
    require(folderOf(retry) == "20261015_173025_Task03_Turn_on_then_short_Attempt02", "retry number not reflected in folder");
    auto collision = makeCapture(attempt);
    collision.record(&scope, record, {});
    require(folderOf(collision) == base + "_02", "same-second attempt reused an existing folder");

    const auto occupiedFile = parent.filePath(base + "_03");
    const QByteArray sentinel("existing file must survive");
    require(BinaryFileStore::save(occupiedFile, sentinel).succeeded(), "cannot create collision fixture");
    oldFiles.insert(occupiedFile, sentinel);
    auto fileCollision = makeCapture(attempt);
    fileCollision.record(&scope, record, {});
    require(folderOf(fileCollision) == base + "_04", "existing file collision was not skipped");
    require(parent.mkdir(base + "_05"), "cannot create empty folder collision");
    auto emptyCollision = makeCapture(attempt);
    emptyCollision.record(&scope, record, {});
    require(folderOf(emptyCollision) == base + "_06", "existing empty folder was reused");
    require(QDir(parent.filePath(base + "_05")).entryList(QDir::Files).isEmpty(), "old empty folder modified");

    auto failed = makeCapture(attempt);
    scope.fail = true;
    bool rejected = false;
    try { failed.record(&scope, record, {}); } catch (const std::exception &) { rejected = true; }
    require(rejected && failed.files().isEmpty(), "failed save reported files");
    require(QDir(parent.filePath(base + "_07")).entryList(QDir::Files).isEmpty(), "failed save left partial files");
    scope.fail = false;
    auto afterFailure = makeCapture(attempt);
    afterFailure.record(&scope, record, {});
    require(folderOf(afterFailure) == base + "_08", "failed attempt folder was reused");

    // Simultaneous attempts must claim different directories even with identical identities.
    std::promise<void> start;
    const auto ready = start.get_future().share();
    std::vector<std::future<QString>> pending;
    for (int i = 0; i < 4; ++i) {
        pending.push_back(std::async(std::launch::async, [&, ready] {
            Scope ownScope; QAtomicInt ownStop(0);
            StrategyWaveformCapture capture(options, fallback.path(), ownStop, "AC1", "DC_None", "Turn on then short", attempt);
            ready.wait();
            capture.record(&ownScope, record, {});
            return folderOf(capture);
        }));
    }
    start.set_value();
    QSet<QString> folders;
    for (auto &result : pending) folders.insert(result.get());
    require(folders == QSet<QString>{base + "_09", base + "_10", base + "_11", base + "_12"}, "parallel attempts shared or lost a folder");
    for (auto it = oldFiles.cbegin(); it != oldFiles.cend(); ++it) {
        QFile file(it.key()); require(file.open(QIODevice::ReadOnly) && file.readAll() == it.value(), "old data overwritten or removed");
    }

    // A real directory-creation error must fail promptly, without saving elsewhere.
    QTemporaryDir gone;
    CaptureOptions png; png.formats = {"PNG"};
    StrategyWaveformCapture unavailable(png, gone.path(), stop, "AC", "DC", "Static Test", attempt);
    require(QDir().rmdir(gone.path()), "cannot remove empty parent fixture");
    const auto screenshots = scope.screenshots;
    rejected = false;
    try { unavailable.record(&scope, record, {}); }
    catch (const std::exception &error) { rejected = QString::fromUtf8(error.what()).contains("cannot create attempt directory"); }
    require(rejected && scope.screenshots == screenshots, "directory failure ignored or capture attempted");
}

int main(int argc,char**argv){
 QCoreApplication app(argc,argv);
 try{
 checkAttemptDirectories();
 QTemporaryDir dir;QAtomicInt stop(0);Scope scope;
 const StrategyWaveformCapture::Attempt attempt{1,1,QDateTime::currentDateTimeUtc()};
 StrategyWaveformCapture capture(CaptureOptions({{"formats",QStringList{"WFM","PNG"}}},true),dir.path(),stop,"AC1_230V/50Hz","DC_None","Static Test",attempt);
 QVector<OscChannelMeasure> values{{1,100,-10,20,5},{2,50,-5,10,2}};
 capture.record(&scope,{values},{0,"Full",LoadConditionKind::Static});
 auto first=capture.files();require(first.size()==2,"first snapshot files");
 capture.record(&scope,{values},{0,"Full",LoadConditionKind::Static});require(scope.waves==2,"identical completed record was omitted");
 scope.level=101;values[0].maxVal=105;capture.record(&scope,{values},{0,"Full",LoadConditionKind::Static});
 require(capture.files().size()==6 && capture.summary().contains("3 completed records"),"completed records missing");
 require(QFileInfo(first.first()).fileName()=="AC1_230V_50Hz_DC_None_Load1_Full_Static_Test_0001.png","condition filename order/sanitization changed");
 auto winners=capture.files();scope.fail=true;values[0].maxVal=110;bool failed=false;
 try{capture.record(&scope,{values},{0,"Full",LoadConditionKind::Static});}catch(...){failed=true;}
 require(failed && capture.files()==winners && QDir(QFileInfo(first.first()).absolutePath()).entryList(QDir::Files).size()==6,"failed save lost winner or leaked partial files");
 scope.fail=false;scope.running=true;failed=false;
 try{capture.record(&scope,{values},{0,"Full",LoadConditionKind::Static});}catch(...){failed=true;}require(failed,"running capture accepted");scope.running=false;
 stop.storeRelease(1);failed=false;try{capture.record(&scope,{values},{0,"Full",LoadConditionKind::Static});}catch(...){failed=true;}require(failed,"cancel ignored");stop.storeRelease(0);
 OscilloscopeAcquisition::VerticalScaleState vertical;int observed=0,adjustments=0;
 vertical.captureRecord=[&](const ScopeCaptureRecord& data){require(scope.changes==0 && data.channels.size()==2,"capture after scale write");++observed;};
 require(!OscilloscopeAcquisition::expandVerticalRange(&scope,stop,adjustments,&vertical) && observed==1,"missing pre-adjust capture");
 scope.clipped=true;
 vertical.captureRecord=[&](const ScopeCaptureRecord &data){
     require(scope.changes==0 && data.clipped && data.phase=="Baseline","clipping not archived before scale write");
     capture.record(&scope,data,{2,"Fault / load",LoadConditionKind::Static});++observed;
 };
 require(OscilloscopeAcquisition::expandVerticalRange(&scope,stop,adjustments,&vertical,"Baseline") && observed==2 && scope.changes==2,"clipped record omitted");
 require(capture.files().size()==8,"clipped capture files missing");
 StrategyWaveformCapture retry(CaptureOptions({{"formats",QStringList{"PNG"}}},true),dir.path(),stop,"AC1_230V/50Hz","DC_None","Static Test",{1,2,attempt.startedAt});
 retry.record(&scope,{values},{0,"Full",LoadConditionKind::Static});
 require(retry.files().first()!=first.first() && QFile::exists(first.first()),"retry overwrote earlier acquisition");
 for(const QString format : {"PNG","CSV","WFM","AllCSV","AllWFM"}) {
     Scope fresh;
     StrategyWaveformCapture all(CaptureOptions({{"formats",QStringList{format}}},true),dir.path(),stop,"AC_None","DC1_48V","Turn on",attempt);
     all.record(&fresh,{values},{1,"Half",LoadConditionKind::Static});
     all.record(&fresh,{values},{1,"Half",LoadConditionKind::Static});
     const int perRecord=format.startsWith("All")?2:1;
     require(all.files().size()==perRecord*2,"format omitted completed record/channel");
     for(const auto &path:all.files()) {
         require(QFile::exists(path),"reported file not saved");
         require(QFileInfo(path).suffix()==(format.startsWith("All")?format.mid(3):format).toLower(),"unselected format saved");
     }
     require(QDir(QFileInfo(all.files().first()).absolutePath()).entryList(QDir::Files).size()==all.files().size(),"unreported sidecar file saved");
 }
 Scope delayed;delayed.captureDelayMs=250;
 StrategyWaveformCapture deadline(CaptureOptions({{"formats",QStringList{"PNG"}}}),dir.path(),stop,"AC_None","DC_None","Static Test",attempt);
 OscilloscopeAcquisition::VerticalScaleState delayedObserver;
 delayedObserver.captureRecord=[&](const ScopeCaptureRecord& record){deadline.record(&delayed,record,{});};
 const auto timedOut=OscilloscopeAcquisition::captureAutoReference(&delayed,stop,100,&delayedObserver);
 require(!timedOut.success && timedOut.errorMessage.contains("timeout") && deadline.files().size()==1,
         "completed reference lost on file-transfer deadline or timeout falsely passed");
 // Failed queries still archive the frozen record once, then propagate the original error.
 class QueryFailureScope : public Scope {
 public:
     bool failClipping=false;
     bool isClipping(int)override {if(failClipping)throw std::runtime_error("clipping query failed");return false;}
     double readMeasurement(int,ScopeMeasurement)override {throw std::runtime_error("measurement query failed");}
 } queryFailure;
 for(bool clippingFailure : {false,true}) {
     queryFailure.failClipping=clippingFailure;
     bool delivered=false,rejected=false;
     StrategyWaveformCapture diagnostic(CaptureOptions({{"formats",QStringList{"PNG","AllWFM"}}}),dir.path(),stop,"AC","DC","Failed query",attempt);
     OscilloscopeAcquisition::VerticalScaleState observer;
     observer.captureRecord=[&](const ScopeCaptureRecord& record){
         require(record.channels.size()==2,"failed query lost channel identity");
         diagnostic.record(&queryFailure,record,{});delivered=true;
     };
     try {OscilloscopeAcquisition::expandVerticalRange(&queryFailure,stop,adjustments,&observer);}
     catch(const std::runtime_error& error) {rejected=QString::fromUtf8(error.what()).contains(clippingFailure?"clipping query failed":"measurement query failed");}
     require(rejected && delivered && diagnostic.files().size()==3,"failed query lost diagnostics or original failure");
     for(const auto &path:diagnostic.files())require(QFile::exists(path),"diagnostic file missing");
 }
 bool bothFailed=false;OscilloscopeAcquisition::VerticalScaleState failingSave;
 failingSave.captureRecord=[](const ScopeCaptureRecord&){throw std::runtime_error("SAVE rejected");};
 try {OscilloscopeAcquisition::expandVerticalRange(&queryFailure,stop,adjustments,&failingSave);}
 catch(const std::runtime_error& error) {const QString text=error.what();bothFailed=text.contains("clipping query failed")&&text.contains("SAVE rejected");}
 require(bothFailed,"diagnostic SAVE hid the original query failure");
 std::cout<<"PASS every completed acquisition capture\n";
 }catch(const std::exception& ex){std::cerr<<ex.what();return 1;}
}
