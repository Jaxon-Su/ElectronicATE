#include "manualscopecontrol.h"
#include "iscopemanualcontrol.h"
#include "msoseries456.h"
#include "oscilloscopeconfiguration.h"
#include "strategywaveformcapture.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <cmath>

void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
template<class F> void until(F predicate) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed()<4000) { QCoreApplication::processEvents(); QThread::msleep(1); }
    require(predicate(),"async operation timed out");
}
class Transport : public ICommunication {
  public:
    bool open() override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    QString lastError() const override { return "injected transport failure"; }
    int write(const QByteArray &bytes) override {
        last=QString::fromUtf8(bytes).trimmed(); writes<<last; consumed=false;
        if(last==cancelAfter && stop) stop->storeRelease(1);
        return last==fail ? -1 : bytes.size();
    }
    int read(QByteArray &bytes,int) override {
        if(consumed) { bytes.clear();return 0; } consumed=true;
        if(!queued[last].isEmpty()) bytes=queued[last].takeFirst();
        else if(values.contains(last)) bytes=values[last];
        else return -1;
        return bytes.size();
    }
    QStringList writes;
    QString last,fail,cancelAfter;
    QAtomicInt *stop=nullptr;
    bool consumed=false;
    QMap<QString,QList<QByteArray>> queued;
    QMap<QString,QByteArray> values{{"*ESR?","0"},{"*OPC?","1"},
        {"TRIGger:A:EDGE:SOUrce?","CH2"},{"DISplay:GLObal:CH2:STATE?","1"},
        {"ACQuire:STATE?","0"},{"TRIGger:A:LEVel:CH2?","1.25"},
        {"HORizontal:SCAle?","0.01"},{"TRIGger:A:EDGE:SLOpe?","RISe"},
        {"CH2:OFFSet?","0.5"},{"CH2:SCAle?","2"},{"ACQuire:STOPAfter?","RUNSTOP"},
        {"MEASUrement:IMMed:VALue?","3.5"}};
};
QStringList checked(const QStringList &commands) {
    QStringList trace; for(const auto &cmd:commands) trace<<"*ESR?"<<cmd<<"*ESR?"; return trace;
}
void configurationContract() {
    Transport transport; MSOSeries456 scope(&transport); scope.connect();
    transport.values["HORizontal:MODe?"]="MANUAL";
    transport.values["HORizontal:MODe:MANual:CONFigure?"]="RECORDLENGTH";
    transport.values["HORizontal:MODe:SAMPLERate?"]="6250000000";
    transport.values["HORizontal:MODe:RECOrdlength?"]="62500000";
    transport.values["HORizontal:SCAle?"]="0.001";
    transport.values["HORizontal:DIVisions?"]="10";
    IScopeConfiguration &port=scope;
    QAtomicInt stop{0}; QString error; OscilloscopeSettings settings;
    const QVariantMap cfg{{"horz_mode","MANUAL"},{"timescale","20ms"},{"horz_pos",50},
        {"horz_config","RECORDLength"},{"sample_rate","6.25GS/s"},{"record_length","62.5M"},
        {"acq_mode","SAMple"},{"fastacq",true},{"ch1_enabled",false},{"ch2_enabled",true},
        {"ch2_termination","1MΩ"},{"ch2_coupling","DC"},{"ch2_bandwidth","FULL"},
        {"ch2_scale","100mV"},{"ch2_position",-2},{"trig_source","CH2"},{"trig_edge","RISING"}};
    require(prepareOscilloscopeSettings(cfg,scope.model(),scope.getTotalChannel(),settings,error),qPrintable(error));
    const QStringList expected{"ACQuire:MODe SAMPLE","ACQuire:FASTAcq:STATE ON",
        "DISplay:GLObal:CH1:STATE OFF","DISplay:GLObal:CH2:STATE ON","CH2:TERmination 1000000",
        "CH2:COUPling DC","CH2:BANdwidth FULl","CH2:SCAle 0.1","CH2:POSition -2",
        "HORizontal:MODe MANUAL","HORizontal:MODe:MANual:CONFigure RECORDLENGTH",
        "HORizontal:MODe:SAMPLERate 6250000000","HORizontal:SCAle 0.02","HORizontal:POSition 50",
        "TRIGger:A:TYPe EDGE","TRIGger:A:EDGE:SOUrce CH2","TRIGger:A:EDGE:SLOpe RISE"};
    const QStringList readback{"*ESR?","HORizontal:MODe?","HORizontal:MODe:MANual:CONFigure?",
        "HORizontal:SCAle?","HORizontal:MODe:SAMPLERate?","HORizontal:MODe:RECOrdlength?","HORizontal:DIVisions?","*ESR?"};
    require(port.applySettings(settings,stop,error),qPrintable(error));
    if(transport.writes!=checked(expected)+readback) std::cerr<<transport.writes.join('\n').toStdString()<<'\n';
    require(transport.writes==checked(expected)+readback,"configuration SCPI sequence changed");
    transport.writes.clear(); stop.storeRelease(1);
    require(!port.applySettings(settings,stop,error)&&transport.writes.isEmpty(),"cancel before write ignored");
    stop.storeRelease(0); transport.stop=&stop;transport.cancelAfter=expected[0];
    require(!port.applySettings(settings,stop,error)&&transport.writes==checked({expected[0]}),"cancel between commands ignored");
    stop.storeRelease(0);transport.writes.clear();transport.cancelAfter=expected.last();
    require(!port.applySettings(settings,stop,error)&&transport.writes==checked(expected),"cancel after final command ignored");
    stop.storeRelease(0);transport.cancelAfter.clear();transport.writes.clear();transport.queued["*ESR?"]={"0","32"};
    require(!port.applySettings(settings,stop,error)&&transport.writes==checked({expected[0]}),"device rejection continued writes");
    transport.writes.clear();transport.fail=expected[0];
    require(!port.applySettings(settings,stop,error)&&transport.writes==QStringList{"*ESR?",expected[0]},"failed write continued");
    transport.fail.clear();transport.writes.clear();
    require(!prepareOscilloscopeSettings({{"timescale","bad"}},scope.model(),4,settings,error),"invalid settings accepted");
    require(transport.writes.isEmpty(),"preflight performed I/O");
    require(prepareOscilloscopeSettings({{"ignore_basic",true},{"timescale","bad"},{"ignore_channels",true},
            {"ch2_scale","bad"},{"ignore_trigger",true},{"trig_source","bad"}},scope.model(),4,settings,error),"ignored fields parsed");
    require(port.applySettings(settings,stop,error)&&transport.writes.isEmpty(),"ignored groups wrote commands");
}
class CaptureScope : public MSOSeries456 {
  public:
    explicit CaptureScope(Transport &comm):MSOSeries456(&comm),transport(comm){}
    QByteArray captureScreenshot(const QString&,const QString&)override { return "png"; }
    bool captureWaveformFileToHost(int,const QString& path,const QString&,const QString&,int,int)override {
        if(resumeDuringSave) transport.values["ACQuire:STATE?"]="1";
        return BinaryFileStore::save(path,"waveform").succeeded();
    }
    Transport &transport; bool resumeDuringSave=false;
};
void captureContract() {
    Transport transport; CaptureScope scope(transport);scope.connect();
    IScopeCapture &port=scope;
    QTemporaryDir dir;QAtomicInt stop{0};
    StrategyWaveformCapture capture(CaptureOptions({{"formats",QStringList{"WFM","PNG"}}},true),dir.path(),stop,"AC1_230V/50Hz","DC_None","Static Test",{1,1,QDateTime::currentDateTimeUtc()});
    QVector<OscChannelMeasure> values{{2,5,-1,3,2}};
    capture.record(&port,{values},{0,"Full",LoadConditionKind::Static});
    const QStringList expected{"ACQuire:STATE?","TRIGger:A:EDGE:SOUrce?","*OPC?","ACQuire:STATE?"};
    require(transport.writes==expected,"capture performed unnecessary metadata queries");
    require(capture.files().size()==2,"unselected format saved");
    const auto winners=capture.files(); values[0].maxVal=6;
    auto fails=[&] {bool rejected=false;try{capture.record(&port,{values},{0,"Full",LoadConditionKind::Static});}catch(const std::exception&){rejected=true;}
        require(rejected&&capture.files()==winners&&QDir(QFileInfo(winners.first()).absolutePath()).entryList(QDir::Files).size()==2,"invalid capture changed winners/leaked files");};
    transport.values["ACQuire:STATE?"]="1";fails();transport.values["ACQuire:STATE?"]="0";
    transport.values["TRIGger:A:EDGE:SOUrce?"]="AUX";fails();transport.values["TRIGger:A:EDGE:SOUrce?"]="CH2";
    transport.fail="TRIGger:A:EDGE:SOUrce?";fails();transport.fail.clear();
    scope.resumeDuringSave=true;fails();scope.resumeDuringSave=false;transport.values["ACQuire:STATE?"]="0";
    transport.values["TRIGger:A:EDGE:SOUrce?"]=" :TRIGGER:A:EDGE:SOURCE   CH2 ";
    auto source=port.captureSource();require(source.channel==2&&source.name=="CH2","source header not decoded by driver");
    transport.values["TRIGger:A:EDGE:SOUrce?"]="AUX";require(port.captureSource().channel==0,"AUX guessed CH1");
    transport.writes.clear();
    StrategyWaveformCapture png(CaptureOptions({{"formats",QStringList{"PNG"}}}),dir.path(),stop,"AC_None","DC_None","Static Test",{1,1,QDateTime::currentDateTimeUtc()});
    png.record(&port,{},{});
    require(png.files().size()==1 && png.files().first().endsWith(".png"),"PNG-only produced a sidecar");
    require(transport.writes==QStringList{"ACQuire:STATE?","*OPC?","ACQuire:STATE?"},"PNG required channel/metadata queries");
    require(QDir(QFileInfo(png.files().first()).absolutePath()).entryList(QDir::Files).size()==1,"PNG-only leaked extra files");
    transport.values["TRIGger:A:EDGE:SLOpe?"]=" :TRIGGER:A:EDGE:SLOPE   FALL ";
    require(port.captureSlope()=="FALL","slope header not decoded");
    QString error;double offset=0;transport.values["CH2:OFFSet?"]=":CH2:OFFSET 0.75";
    require(port.readChannelOffset(2,offset,error)&&offset==.75,"offset header not decoded");
    for(const auto &reply:{"0","STOP","OFF",":ACQUIRE:STATE 0"}) {transport.values["ACQuire:STATE?"]=reply;require(port.acquisitionState()==ScopeAcquisitionState::Completed,"stopped response lost");}
    transport.values["ACQuire:STATE?"]="bad";require(port.acquisitionState()==ScopeAcquisitionState::Invalid,"invalid state accepted");
}
void manualDriverContract() {
    Transport transport;MSOSeries456 scope(&transport);scope.connect();
    ManualScopeControl control;control.setSuspended(true);
    control.setLeaseFactory([] {return std::make_shared<int>(1);});control.bind(&scope);
    QStringList errors;QObject::connect(&control,&ManualScopeControl::errorOccurred,[&](const QString&e){errors<<e;});
    auto perform=[&](auto action) {control.setSuspended(false);action();control.setSuspended(true);until([&]{return !control.isBusy();});};
    perform([&]{control.setType("EDGE");});perform([&]{control.setSource("CH2");});
    perform([&]{control.setSlope("RISING");});perform([&]{control.setSlope("FALLING");});perform([&]{control.setSlope("BOTH");});
    perform([&]{control.setLevel(1.25);});perform([&]{control.automatic();});perform([&]{control.normal();});
    perform([&]{control.toggleRun();});transport.values["ACQuire:STATE?"]="1";perform([&]{control.toggleRun();});
    perform([&]{control.single();});
    QStringList expected{"TRIGger:A:TYPe EDGE","TRIGger:A:EDGE:SOUrce CH2","TRIGger:A:EDGE:SLOpe RISe",
        "TRIGger:A:EDGE:SLOpe FALL","TRIGger:A:EDGE:SLOpe EITher","TRIGger:A:LEVel:CH2 1.25",
        "TRIGger:A:MODe AUTO","TRIGger:A:MODe NORMal","ACQuire:STATE?","ACQuire:STOPAfter?",
        "ACQuire:STOPAfter RUNSTop","ACQuire:STATE RUN","ACQuire:STATE?","ACQuire:STATE STOP"};
    expected+=checked({"ACQuire:STATE STOP","ACQuire:STOPAfter SEQuence","ACQuire:STATE RUN"});
    require(errors.isEmpty()&&transport.writes==expected,"manual control SCPI sequence changed");
    transport.writes.clear();bool measured=false;
    perform([&]{control.measure(2,[&](const ManualScopeControl::Result&r){measured=r.measurements==QVector<double>{3.5,3.5,3.5,3.5};});});
    expected.clear();for(const auto* type:{"MAXIMUM","MINIMUM","RMS","MEAN"}) {expected+=checked({"MEASUrement:IMMed:SOUrce1 CH2",QString("MEASUrement:IMMed:TYPe ")+type});expected<<"MEASUrement:IMMed:VALue?";}
    require(measured&&transport.writes==expected,"manual measurement protocol changed");
    transport.writes.clear();double step=0;
    perform([&]{control.readStep(2,100,[&](const ManualScopeControl::Result&r){step=r.levelStep;});});
    require(step==.08&&transport.writes==QStringList{"CH2:SCAle?"},"manual step changed");
    transport.fail="TRIGger:A:MODe AUTO";perform([&]{control.automatic();});
    require(!errors.isEmpty()&&!control.isBusy()&&!control.hasActiveCommand(),"manual failure stuck busy");
}
// This fake intentionally has no Oscilloscope/transport inheritance.
class ManualPort : public IScopeManualControl {
  public:
    bool isConnected()const override{return true;}void disconnect()override{++disconnects;}
    QString lastError()const override{return error;}bool isRunning()override{return false;}
    bool beginSingleAcquisition()override{return true;}void run()override{}void stop()override{}
    void automode()override{}void normal()override{}
    void setTriggerLevel(double v)override{level=v;}void setTriggerType(const QString&)override{}
    void setTriggerSource(const QString&)override{}void setTriggerSlope(const QString&)override{}
    int getTotalChannel()override{return 2;}double getChannelScale(int)override{return 2;}
    double readMeasurement(int,ScopeMeasurement type)override{types<<type;return 3;}
    QString error;double level=0;int disconnects=0;QList<ScopeMeasurement> types;
};
void manualCapabilityContract() {
    ManualPort port;ManualScopeControl control;control.bind(&port);control.setLeaseFactory([]{return std::make_shared<int>(1);});
    control.setLevel(2.5);until([&]{return !control.isBusy();});require(port.level==2.5,"pure capability not usable");
    bool measured=false;control.measure(2,[&](const ManualScopeControl::Result&r){measured=r.measurements.size()==4;});
    until([&]{return !control.isBusy();});require(measured&&port.types==QList<ScopeMeasurement>{ScopeMeasurement::Maximum,ScopeMeasurement::Minimum,ScopeMeasurement::Rms,ScopeMeasurement::Mean},"semantic measurement order changed");
    port.error="poll failure";control.poll();until([&]{return !control.isBusy();});
    require(!control.isConnected()&&port.disconnects==1,"poll failure not isolated");control.setSuspended(true);
}
int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    try {const QString scenario=argc>1?argv[1]:"";
        if(scenario=="configuration")configurationContract();else if(scenario=="capture")captureContract();
        else if(scenario=="manual_driver")manualDriverContract();else if(scenario=="manual_capability")manualCapabilityContract();
        else throw std::runtime_error("unknown scenario");
        std::cout<<"PASS scope port contract\n";
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
