#include "msoseries456.h"
#include "chroma63600.h"
#include "chroma63200a.h"
#include "dcloadfactory.h"
#include "loadcapabilitycatalog.h"
#include "brownoutio.h"
#include "chroma61505.h"
#include "chroma61509.h"
#include "chroma6530.h"
#include "chroma62000.h"
#include <QCoreApplication>
#include <QtEndian>
#include <iostream>

void expect(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
class Transport:public ICommunication {
public:
    bool offline=false;
    bool open()override{return !offline;}void close()override{}bool isOpen()const override{return !offline;}
    QString lastError()const override{return "injected failure";}
    int write(const QByteArray&b)override{
        last=QString::fromUtf8(b).trimmed();trace<<last;consumed=false;
        if(last==fail)return -1;
        if(last==shortWrite)return b.size()-1;
        if(last.startsWith("V "))values["VOLTage?"]=last.mid(2).toLatin1();
        if(!last.endsWith('?')){auto space=last.indexOf(' ');if(space>0)values[last.left(space)+'?']=last.mid(space+1).toLatin1();}
        return b.size();
    }
    int read(QByteArray&b,int)override{
        if(throwRead)throw std::runtime_error("injected read exception");
        if(consumed){b.clear();return 0;}consumed=true;
        if(!queued[last].isEmpty())b=queued[last].takeFirst();
        else if(values.contains(last))b=values[last];else return -1;
        return b.size();
    }
    int timeoutMs()const override{return timeout;}void setTimeoutMs(int value)override{timeout=value;}
    QString last,fail,shortWrite;QStringList trace;bool consumed=false,throwRead=false;int timeout=1234;
    QMap<QString,QList<QByteArray>>queued;
    QMap<QString,QByteArray>values{{"*ESR?","0"},{"*OPC?","1"},{"DISplay:GLObal:CH1:STATE?","ON"},
        {"DISplay:GLObal:CH2:STATE?","OFF"},{"DISplay:GLObal:CH3:STATE?","OFF"},{"DISplay:GLObal:CH4:STATE?","OFF"},
        {"HORizontal:FASTframe:STATE?","0"},{"HORizontal:RECOrdlength?","4"},{"CH1:SCAle?","1"},
        {"TRIGger:A:TYPe?","EDGE"},{"TRIGger:A:EDGE:SOUrce?","CH2"},{"TRIGger:A:EDGE:SLOpe?","RISE"},
        {"TRIGger:A:MODe?","AUTO"},{"TRIGger:A:LEVel:CH1?","3"},{"HORizontal:SCAle?",".01"},
        {"HORizontal:POSition?","40"},{"HORizontal:DIVisions?","10"},{"ACQuire:MODe?","HIRES"},{"ACQuire:STOPAfter?","RUNSTOP"},
        {"ACQuire:SEQuence:MODe?","NUMACQS"},{"ACQuire:SEQuence:NUMSEQuence?","3"},
        {"DATa:SOUrce?","CH2"},{"DATa:ENCdg?","ASCII"},{"DATa:WIDth?","1"},{"DATa:STARt?","2"},{"DATa:STOP?","3"},
        {"ACQuire:STATE?","0"},{"ACQuire:NUMACq?","1"},{"TRIGger:STATE?","READY"},
        {"WFMOutpre:NR_Pt?","4"},{"WFMOutpre:YMUlt?",".5"},{"WFMOutpre:YOFf?","1"},
        {"WFMOutpre:YZEro?","2"},{"WFMOutpre:BYT_Or?","MSB"},{"WFMOutpre:YUNit?","\"V\""},{"CH1:CLIPping?","0"},
        {"MEAS:VOLT?","12.5"},{"OUTPut?","0"},{"CONF:OUTP?","0"}};
};
QByteArray curve() {QByteArray result="#18";for(qint16 v:{-1,1,3,5}){char bytes[2];qToBigEndian(v,bytes);result.append(bytes,2);}result+='\n';return result;}
void scopeTest(const QString &scenario) {
    Transport t;t.values["CURVe?"]=curve();MSOSeries456 scope(&t);scope.connect();
    if(scenario=="long_timebase")t.values["HORizontal:SCAle?"]="2.5";
    if(scenario=="invalid_timebase")t.values["HORizontal:SCAle?"]="0";
    auto values=BrownOut::defaults();values["trigger_channel"]=1;
    if(scenario=="normal" || scenario.startsWith("capture"))values["mode"]="TRACK";
    if(scenario.startsWith("capture"))values["capture_enabled"]=true;
    BrownOut::Settings cfg(values);QString error;
    QAtomicInt stop{0};auto session=scope.beginBrownOut(cfg,stop,error);
    if(scenario=="invalid_timebase") {
        expect(!session&&!error.isEmpty(),"invalid scope timebase accepted");return;
    }
    expect(bool(session),qPrintable(error));
    expect(session->recordDurationMs()==(scenario=="long_timebase"?25000:100),"record duration did not use existing scope timebase");
    t.values["ACQuire:STATE?"]="0";
    BrownOutRecord record;expect(session->poll(record,error)==BrownOutRecordStatus::Ready,qPrintable(error));
    expect(record.channels[0].minimum==1&&record.channels[0].maximum==4&&record.channels[0].mean==2.5,"binary scaling wrong");
    expect(std::abs(record.channels[0].rms-std::sqrt(7.5))<1e-10,"RMS calculation wrong");
    expect(session->advanceLevel(1.25,error),qPrintable(error));expect(session->arm(error),qPrintable(error));
    t.values["ACQuire:NUMACq?"]="0";expect(session->readyToRamp(error),qPrintable(error));
    expect(session->poll(record,error)==BrownOutRecordStatus::Pending,"old record accepted after arm");
    t.values["ACQuire:NUMACq?"]="1";
    if(scenario!="normal")t.values["ACQuire:STATE?"]="0";
    if(scenario=="clipped"||scenario=="capture_clipped")t.values["CH1:CLIPping?"]="1";
    if(scenario=="partial")t.values["CURVe?"]="#14abcd\n";
    if(scenario=="encoding")t.values["WFMOutpre:YMUlt?"]="0";
    if(scenario=="mixed")t.queued["ACQuire:NUMACq?"]={"1","2"};
    const auto state=session->poll(record,error);
    if(scenario=="clipped"||scenario=="partial"||scenario=="encoding") {
        expect(state==BrownOutRecordStatus::Error,"invalid record accepted");
        expect(record.sequence==2&&record.channels.size()==1&&record.channels[0].channel==1,
               "failed frozen record lost diagnostic identity");
    }
    else if(scenario=="mixed")expect(state==BrownOutRecordStatus::Pending,"mixed records accepted");
    else expect(state==BrownOutRecordStatus::Ready,qPrintable(error));
    if(scenario.startsWith("capture")) {
        expect(t.values["ACQuire:STOPAfter?"]=="SEQUENCE","Track capture not frozen as Single");
        expect(record.clipped==(scenario=="capture_clipped"),"clipped diagnostic lost");
        const auto sequence=record.sequence;
        expect(session->poll(record,error)==BrownOutRecordStatus::Pending,"same record returned twice");
        const auto runs=t.trace.count("ACQuire:STATE RUN");
        expect(session->advanceLevel(1.5,error),qPrintable(error));
        expect(t.trace.count("ACQuire:STATE RUN")==runs+1,"next acquisition not rearmed after capture");
        t.values["ACQuire:STATE?"]="RUN";
        expect(session->poll(record,error)==BrownOutRecordStatus::Pending,"running waveform exported");
        t.values["ACQuire:STATE?"]="0";
        expect(session->poll(record,error)==BrownOutRecordStatus::Ready && record.sequence==sequence+1,
               "Single counter reset broke logical sequence");
    }
    expect(t.timeout==1234,"transfer timeout leaked");
    if(scenario=="restore_error")t.fail="ACQuire:MODe HIRES";
    const bool restored=session->finish(error);
    expect(restored!=(scenario=="restore_error"),"cleanup failure not surfaced");
    expect(t.values["DATa:ENCdg?"]=="ASCII"&&t.values["TRIGger:A:EDGE:SOUrce?"]=="CH2"&&t.values["ACQuire:STATE?"]=="STOP","scope settings not restored/stopped");
    const auto size=t.trace.size();session.reset();expect(t.trace.size()==size,"scope restored twice");
    expect(t.values["HORizontal:SCAle?"]==(scenario=="long_timebase"?"2.5":".01")&&
           t.values["HORizontal:POSition?"]=="40","horizontal settings changed");
    for(const auto &command:t.trace)
        expect(!command.startsWith("HORizontal:SCAle ")&&!command.startsWith("HORizontal:POSition "),
               "Brown Out wrote horizontal settings");
    const auto lastRun=t.trace.lastIndexOf("ACQuire:STATE RUN");
    for(int i=lastRun+1;i<t.trace.size();++i)expect(t.trace[i]!="ACQuire:STATE RUN","Normal restarted per record");
}
void monitorTest(){
    Transport t;Chroma63600 load("63640-80-80",&t);load.connect();QString error;double v=0;
    expect(load.readOutputVoltage(v,error)&&v==12.5,"63600 live voltage failed");
    Chroma63200A second("63204A-1200-160",&t);second.connect();
    expect(second.readOutputVoltage(v,error)&&v==12.5,"63200A live voltage failed");
    for(const auto&bad:{"NaN","garbage","9.9e37","invalid 0","12\n0","12,0",""}){
        t.values["MEAS:VOLT?"]=bad;v=99;
        expect(!load.readOutputVoltage(v,error)&&!second.readOutputVoltage(v,error)&&v==99&&!error.isEmpty(),
               "invalid monitor accepted or replaced the last value");
    }
    t.values["MEAS:VOLT?"]="0";
    expect(load.readOutputVoltage(v,error)&&v==0&&error.isEmpty()&&load.lastError().isEmpty(),"valid zero did not recover from monitor error");
    expect(second.readOutputVoltage(v,error)&&v==0&&error.isEmpty(),"63200A valid zero did not recover");
}
void monitor6310Test(bool seriesA)
{
    const auto &models = seriesA ? LoadCapabilityCatalog::Chroma6310AModels : LoadCapabilityCatalog::Chroma6310Models;
    for (const auto &model : models) {
        Transport t;
        std::unique_ptr<DCLoad> load(DCLoadFactory::createDCLoad(model, &t));
        expect(bool(load), "6310 family factory missing");
        load->connect();
        load->setChannel(6);
        QString error = "previous error";
        double voltage = 99;
        t.values["FETC:VOLT?"] = " 12.500\r\n";
        expect(load->readOutputVoltage(voltage, error) && voltage == 12.5 && error.isEmpty(), "6310 valid voltage failed");
        expect(t.trace == QStringList{"CHAN 6", "FETC:VOLT?"}, "wrong 6310 SCPI or extra state changes");
        for (double value : {0.25, 0.0, -0.01}) {
            t.values["FETC:VOLT?"] = QByteArray::number(value);
            expect(load->readOutputVoltage(voltage, error) && voltage == value, "6310 returned cached voltage or rejected zero");
        }
        const auto expectFailure = [&] {
            voltage = 99;
            error.clear();
            expect(!load->readOutputVoltage(voltage, error), "invalid 6310 monitor accepted");
            expect(voltage == 99 && !error.isEmpty(), "failed 6310 monitor replaced output or lost error");
        };
        for (const auto &bad : {"", " \r\n", "NaN", "Inf", "-Inf", "9.9e37", "-9.9e37", "garbage", "invalid 0", "12,0", "12\n0"}) {
            t.values["FETC:VOLT?"] = bad;
            expectFailure();
        }
        t.values.remove("FETC:VOLT?"); // Read timeout / communication failure.
        expectFailure();
        t.values["FETC:VOLT?"] = "8.12";
        t.fail = "FETC:VOLT?";
        expectFailure();
        t.fail.clear();
        t.shortWrite = "FETC:VOLT?";
        expectFailure();
        t.shortWrite.clear();
        t.throwRead = true;
        expectFailure();
        t.throwRead = false;
        expect(load->readOutputVoltage(voltage, error) && voltage == 8.12 && error.isEmpty() && load->lastError().isEmpty(),
               "6310 query did not recover or clear previous error");
        expect(!t.trace.contains("MEAS:VOLT?"), "6310 did not use documented real-time FETCH query");
    }
}

void adapter6310Test(const QString &scenario)
{
    const bool seriesA = scenario.contains("6310a");
    const QString model = seriesA ? "63102A" : "63102";
    TestSequenceExecutionContext context;
    context.inputs = {{"1phase", "90", "50", "0"}};
    context.loads = {{"L1", {"", "", "", "1"}}};
    InstrumentConfig input;
    input.name = "ACSource"; input.type = "InputSource"; input.modelName = "61509";
    input.commConfig = CommunicationConfig::fromResourceString("GPIB0::30::INSTR");
    InstrumentConfig load;
    load.name = "Load1"; load.type = "Load"; load.modelName = seriesA ? "6314A" : "6314";
    load.commConfig = CommunicationConfig::fromResourceString("GPIB0::1::INSTR");
    load.channels = {{model, 2}, {model, 4}};
    load.channelNumbers = {1, 6}; // Monitor Output Index 4 resolves to hardware CH6, not CH4.
    context.instrumentSetup.instruments = {input, load};
    auto values = BrownOut::defaults();
    values["ac_input_index"] = 0; values["load_index"] = 0; values["monitor_output"] = 4;
    BrownOut::Settings settings(values);
    int creates = 0, on = 0, off = 0;
    Transport *source = nullptr, *monitor = nullptr;
    InstrumentOperations operations;
    operations.load = [&](const auto &, const auto &, int index, const auto &, LoadAction action, bool) {
        expect(index == 0, "6310 selected load condition changed");
        if (action == LoadAction::LoadOn) ++on;
        if (action == LoadAction::LoadOff) ++off;
        return InstrumentOperationResult{};
    };
    const auto create = [&](const QString &address) -> ICommunication * {
        ++creates;
        auto *transport = new Transport;
        if (address == input.getResourceString()) source = transport;
        else {
            monitor = transport;
            monitor->values["FETC:VOLT?"] = scenario.endsWith("_startup") ? "invalid 0" : "12.5";
            if (scenario.endsWith("_channel")) monitor->fail = "CHAN 6";
        }
        return transport;
    };
    for (int channel : {0, 9}) {
        auto invalid = context;
        invalid.instrumentSetup.instruments[1].channelNumbers[1] = channel;
        bool rejected = false;
        try { prepareBrownOutIo(invalid, settings, operations, create); }
        catch (const std::exception &) { rejected = true; }
        expect(rejected && creates == 0 && on == 0, "invalid 6310 channel bypassed preflight");
    }
    auto factory = prepareBrownOutIo(context, settings, operations, create);
    expect(creates == 0 && on == 0, "6310 preflight performed I/O");
    auto io = factory();
    bool rejected = false;
    try { io->start([] { return false; }); }
    catch (const std::exception &) { rejected = true; }
    if (scenario.endsWith("_startup") || scenario.endsWith("_channel")) {
        expect(rejected && source && monitor && !source->trace.contains("OUTPut ON"), "failed monitor enabled DUT input");
        if (scenario.endsWith("_channel")) expect(!monitor->trace.contains("FETC:VOLT?"), "queried wrong channel after failed selection");
    } else {
        expect(!rejected && creates == 2 && on == 1, "6310 Brown Out startup failed");
        expect(monitor->trace == QStringList{"CHAN 6", "FETC:VOLT?"}, "Brown Out selected wrong channel or query");
        expect(source->trace.contains("OUTPut ON"), "valid monitor did not enable input");
        monitor->queued["FETC:VOLT?"] = {"0.25", "0"};
        expect(io->outputVoltage() == 0.25 && io->outputVoltage() == 0, "6310 live monitor did not propagate new readings");
        if (scenario.endsWith("_runtime")) {
            monitor->values.remove("FETC:VOLT?");
            try { (void)io->outputVoltage(); }
            catch (const std::exception &e) {
                rejected = true;
                expect(QString::fromUtf8(e.what()).contains("FETC:VOLT?"),"runtime error lost the failed voltage query");
            }
            expect(rejected, "6310 runtime read failure became a shutdown voltage");
        }
    }
    QString error;
    expect(io->shutdown(error) && off == 1 && source->trace.last() == "OUTPut?", "6310 path skipped source/load cleanup");
}

void adapterTest(const QString &scenario){
    TestSequenceExecutionContext c;c.inputs={{"3phase","90","50","0"}};c.loads={{"L1",{"1"}}};
    InstrumentConfig ac;ac.name="ACSource";ac.type="InputSource";ac.modelName="61509";
    ac.commConfig=CommunicationConfig::fromResourceString("GPIB0::30::INSTR");
    ac.commConfig.timeout=321;
    if(scenario=="adapter_6530"){ac.modelName="6530";c.inputs[0].phaseMode="1phase";}
    InstrumentConfig load;load.name="Load1";load.type="Load";load.modelName="63600-5";
    load.commConfig=CommunicationConfig::fromResourceString("GPIB0::1::INSTR");
    load.channels={{"63640-80-80",1}};load.channelNumbers={3};c.instrumentSetup.instruments={ac,load};
    auto v=BrownOut::defaults();v["ac_input_index"]=0;v["load_index"]=0;BrownOut::Settings cfg(v);
    int creates=0,on=0,off=0;Transport*source=nullptr;Transport*monitor=nullptr;bool stopped=false;
    InstrumentOperations ops;ops.load=[&](const auto&,const auto&,int,const auto&,LoadAction action,bool){if(action==LoadAction::LoadOn){++on;if(scenario=="adapter_cancel")stopped=true;}else if(action==LoadAction::LoadOff)++off;return InstrumentOperationResult{};};
    const auto create=[&](const QString&address)->ICommunication*{++creates;auto*t=new Transport;if(address.contains("30"))source=t;else monitor=t;return t;};
    auto factory=prepareBrownOutIo(c,cfg,ops,create);expect(creates==0,"preflight opened transport");
    auto io=factory();
    if(scenario=="adapter_cancel") {
        bool rejected=false;try{io->start([&]{return stopped;});}catch(...){rejected=true;}
        expect(rejected&&source&&!source->trace.contains("OUTPut ON"),"startup cancellation still enabled source");
        QString error;expect(io->shutdown(error)&&off==1,"cancelled startup skipped cleanup");return;
    }
    io->start([] { return false; });expect(creates==2&&on==1,"Brown Out input/load setup failed");
    expect(source->timeout==321 && monitor->timeout==1000,"Brown Out lost configured timeout or operation cap");
    if(scenario=="adapter_6530") {
        const auto before=source->trace.size();io->setVoltage(60);
        expect(source->trace.mid(before)==QStringList{"V 60.0","VOLTage?"},"6530 ramp did not change live voltage without toggling output");
        QString error;expect(io->shutdown(error)&&off==1,qPrintable(error));return;
    }
    if(scenario=="adapter_readback") {
        source->queued["SOURce:VOLTage:AC?"]={"51.962"};
        bool rejected=false;try{io->setVoltage(60);}catch(...){rejected=true;}
        expect(rejected,"rejected voltage step was accepted");
        QString error;expect(io->shutdown(error)&&off==1,"failed voltage step skipped cleanup");return;
    }
    source->queued["SOURce:VOLTage:AC?"]={"34.6"};
    io->setVoltage(60);expect(source->trace.contains("SOURce:VOLTage:AC 34.641"),"3phase line voltage not converted per step");
    expect(io->outputVoltage()==12.5,"monitor read failed");QString error;expect(io->shutdown(error)&&off==1,qPrintable(error));
    auto tooSmall=v;tooSmall["voltage_step"]=0.01;
    bool rejected=false;try{prepareBrownOutIo(c,BrownOut::Settings(tooSmall),ops,create);}catch(...){rejected=true;}
    expect(rejected&&creates==2,"sub-resolution step accepted or preflight sent commands");
    c.instrumentSetup.instruments.append(load);rejected=false;try{prepareBrownOutIo(c,cfg,ops,create);}catch(...){rejected=true;}
    expect(rejected&&creates==2,"ambiguous monitor accepted or preflight sent commands");
}
void dcAdapterTest() {
    TestSequenceExecutionContext c;c.loads={{"L1",{"1"}}};c.instrumentSetup.dcInputs=3;c.dcInputs.resize(1);
    for(int i=0;i<3;++i){
        InstrumentConfig dc;dc.name=QString("DC Source%1").arg(i+1);dc.type="InputDCSource";dc.modelName="62050H-40";
        dc.commConfig=CommunicationConfig::fromResourceString(QString("GPIB0::%1::INSTR").arg(i+5));
        c.instrumentSetup.instruments.append(dc);c.dcInputs[0][i].vin="24";c.dcInputs[0][i].currentLimit="2";
    }
    InstrumentConfig load;load.name="Load1";load.type="Load";load.modelName="63600-5";
    load.commConfig=CommunicationConfig::fromResourceString("GPIB0::1::INSTR");
    load.channels={{"63640-80-80",1}};load.channelNumbers={1};c.instrumentSetup.instruments.append(load);
    auto v=BrownOut::defaults();v["dc_input_index"]=0;v["load_index"]=0;v["dc_source"]=2;v["ramp_source"]="DC";
    QMap<QString,Transport*> ports;int off=0;
    InstrumentOperations ops;ops.load=[&](const auto&,const auto&,int,const auto&,LoadAction action,bool){if(action==LoadAction::LoadOff)++off;return InstrumentOperationResult{};};
    auto factory=prepareBrownOutIo(c,BrownOut::Settings(v),ops,[&](const QString &address)->ICommunication*{auto*t=new Transport;ports[address]=t;return t;});
    auto io=factory();io->start([]{return false;});io->setVoltage(20);
    for(int i=0;i<3;++i){auto*t=ports[QString("GPIB0::%1::INSTR").arg(i+5)];
        expect(t->values["SOUR:VOLT?"]==(i==1?"20":"24"),"wrong DC source ramped");
        expect(t->trace.contains("SOUR:VOLT?"),"DC setting not read back");}
    auto*selected=ports["GPIB0::6::INSTR"];selected->queued["SOUR:VOLT?"]={"20"};
    bool rejected=false;try{io->setVoltage(19);}catch(...){rejected=true;}expect(rejected,"DC voltage mismatch accepted");
    QString error;expect(io->shutdown(error)&&off==1,qPrintable(error));
    for(int i=0;i<3;++i)expect(ports[QString("GPIB0::%1::INSTR").arg(i+5)]->trace.last()=="CONF:OUTP?","DC cleanup/readback skipped");
}
void dualAdapterTest(const QString &scenario) {
    TestSequenceExecutionContext c;
    c.inputs={{"1phase","90","50","0"}}; c.loads={{"L1",{"1"}}};
    c.instrumentSetup.dcInputs=3; c.dcInputs.resize(1);
    InstrumentConfig ac; ac.name="ACSource"; ac.type="InputSource"; ac.modelName="61509";
    ac.commConfig=CommunicationConfig::fromResourceString("GPIB0::30::INSTR");
    c.instrumentSetup.instruments.append(ac);
    for (int i=0;i<3;++i) {
        InstrumentConfig dc; dc.name=QString("DC Source%1").arg(i+1); dc.type="InputDCSource"; dc.modelName="62050H-40";
        dc.commConfig=CommunicationConfig::fromResourceString(QString("GPIB0::%1::INSTR").arg(i+5));
        c.instrumentSetup.instruments.append(dc); c.dcInputs[0][i].vin="24"; c.dcInputs[0][i].currentLimit="2";
    }
    InstrumentConfig load; load.name="Load1"; load.type="Load"; load.modelName="63600-5";
    load.commConfig=CommunicationConfig::fromResourceString("GPIB0::1::INSTR");
    load.channels={{"63640-80-80",1}}; load.channelNumbers={1}; c.instrumentSetup.instruments.append(load);
    auto v=BrownOut::defaults(); v["ac_input_index"]=0; v["dc_input_index"]=0; v["load_index"]=0;
    v["dc_source"]=2; v["ramp_source"]=scenario=="dual_ac" ? "AC" : "DC";
    QMap<QString,Transport*> ports; int on=0,off=0;
    InstrumentOperations ops;
    ops.load=[&](const auto&,const auto&,int,const auto&,LoadAction a,bool) {
        if(a==LoadAction::LoadOn) ++on; else ++off; return InstrumentOperationResult{};
    };
    const auto create=[&](const QString &address)->ICommunication* {
        auto *t=new Transport; ports[address]=t;
        t->offline = (scenario=="dual_connect_failure" && address=="GPIB0::30::INSTR") || (scenario=="dual_dc_connect_failure" && address=="GPIB0::6::INSTR");
        if(scenario=="dual_startup_failure"&&address=="GPIB0::6::INSTR") t->fail="CONF:OUTP ON";
        return t;
    };
    if (scenario=="dual_preflight") {
        auto reject=[&](const TestSequenceExecutionContext &ctx,const QVariantMap &cfg) {
            bool failed=false; try {prepareBrownOutIo(ctx,BrownOut::Settings(cfg),ops,create);} catch(...) {failed=true;}
            expect(failed&&ports.isEmpty()&&on==0,"invalid dual configuration opened transport");
        };
        auto bad=v; bad["dc_input_index"]=1; reject(c,bad);
        bad=v; bad["dc_input_index"]=-1; reject(c,bad);
        bad=v; bad["ac_input_index"]=-1; bad["ramp_source"]="AC"; reject(c,bad);
        bad=v; bad["ramp_source"]="BOTH"; reject(c,bad);
        auto conflict=c; conflict.instrumentSetup.instruments[1].commConfig=ac.commConfig; reject(conflict,v);
        conflict=c; conflict.instrumentSetup.instruments[2].enabled=false; reject(conflict,v);
        conflict=c; conflict.instrumentSetup.instruments[0].enabled=false; reject(conflict,v);
        return;
    }
    auto factory=prepareBrownOutIo(c,BrownOut::Settings(v),ops,create);
    expect(ports.isEmpty(),"preflight opened dual sources");
    auto io=factory(); bool failed=false;
    try {io->start([&] {
        auto *source=ports.value("GPIB0::30::INSTR");
        return scenario=="dual_cancel" && source && source->trace.contains("OUTPut ON");
    });} catch(...) {failed=true;}
    if(scenario=="dual_connect_failure" || scenario=="dual_dc_connect_failure") {
        expect(failed && on==0,"connection failure started load");
        QString error; expect(!io->shutdown(error) && !error.isEmpty() && off==1,"early failure skipped cleanup/error");
        for(int address : {5,6,7}) {
            if(scenario=="dual_dc_connect_failure" && address==6) continue;
            auto *t=ports.value(QString("GPIB0::%1::INSTR").arg(address));
            expect(t && t->trace.contains("CONF:OUTP OFF") && t->trace.last()=="CONF:OUTP?","unstarted DC source was not released");
            expect(!t->trace.contains("CONF:OUTP ON"),"cleanup enabled an unstarted source");
        }
        return;
    }
    auto *source=ports.value("GPIB0::30::INSTR");
    expect(source&&source->trace.contains("OUTPut ON"),"dual start did not enable AC");
    if(scenario=="dual_startup_failure"||scenario=="dual_cancel") {
        expect(failed,"dual startup fault not reported");
        if(scenario=="dual_cancel") expect(!ports["GPIB0::5::INSTR"]->trace.contains("CONF:OUTP ON"),"cancel enabled DC");
    } else {
        expect(!failed&&on==1&&ports.size()==5,"dual startup failed");
        for(int i=5;i<=7;++i) expect(ports[QString("GPIB0::%1::INSTR").arg(i)]->trace.contains("CONF:OUTP ON"),"DC not enabled");
        const bool rampAc=scenario=="dual_ac";
        expect(io->initialVoltage()==(rampAc?90:24),"initial voltage from wrong source");
        const auto acBefore=source->trace.size();
        QVector<int> dcBefore;
        for(int i=5;i<=7;++i) dcBefore<<ports[QString("GPIB0::%1::INSTR").arg(i)]->trace.size();
        io->setVoltage(rampAc?80:20);
        expect(source->values["SOURce:VOLTage:AC?"].toDouble()==(rampAc?80:90),"AC did not retain/ramp correct voltage");
        if(!rampAc) expect(source->trace.size()==acBefore,"DC ramp sent extra AC commands");
        for(int i=5;i<=7;++i) {
            auto *t=ports[QString("GPIB0::%1::INSTR").arg(i)];
            expect(t->values["SOUR:VOLT?"].toDouble()==(!rampAc&&i==6?20:24),"wrong DC voltage during dual ramp");
            if(rampAc||i!=6) expect(t->trace.size()==dcBefore[i-5],"ramp sent commands to constant DC source");
        }
        if(scenario=="dual_cleanup_failure") source->fail="OUTPut OFF";
    }
    QString error; const bool clean=io->shutdown(error);
    expect(clean==(scenario!="dual_cleanup_failure")&&off==1,"dual cleanup result or Load OFF lost");
    expect(source->trace.contains("OUTPut OFF"),"AC OFF missing");
    for(int i=5;i<=7;++i) expect(ports[QString("GPIB0::%1::INSTR").arg(i)]->trace.last()=="CONF:OUTP?","DC cleanup skipped after failure");
}
void sourceTest() {
    Transport t;Chroma61505 a(&t);Chroma61509 b(&t);Chroma6530 c(&t);Chroma62000 d("62050H-40",&t);
    const auto check=[&](auto &source,const QString &query){
        source.connect();QString error;double value=0;t.values[query]="12.3";
        expect(source.readVoltageSetting(value,error)&&value==12.3&&t.trace.last()==query,"wrong model voltage-setting query");
        for(const auto &bad:{"NaN","invalid","9.9e37","invalid 0","invalid 12.3","12.3\n0",""}){
            t.values[query]=bad;value=99;
            expect(!source.readVoltageSetting(value,error)&&value==99&&!error.isEmpty(),"invalid source setting accepted or replaced caller value");
        }
        t.values[query]="0";
        t.shortWrite=query;expect(!source.readVoltageSetting(value,error)&&value==99,"partial source query write accepted");t.shortWrite.clear();
        t.throwRead=true;expect(!source.readVoltageSetting(value,error)&&value==99,"source read exception changed value");t.throwRead=false;
        expect(source.readVoltageSetting(value,error)&&value==0&&error.isEmpty()&&source.lastError().isEmpty(),"source query did not recover or clear error");
    };
    check(a,"SOURce:VOLTage:AC?");check(b,"SOURce:VOLTage:AC?");check(c,"VOLTage?");check(d,"SOUR:VOLT?");
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{const auto s=app.arguments().value(1);if(s.startsWith("dual_"))dualAdapterTest(s);else if(s=="source")sourceTest();else if(s=="monitor")monitorTest();else if(s.startsWith("monitor_6310"))monitor6310Test(s.endsWith('a'));else if(s.startsWith("adapter_6310"))adapter6310Test(s);else if(s=="adapter_dc")dcAdapterTest();else if(s.startsWith("adapter"))adapterTest(s);else scopeTest(s);std::cout<<"OK\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
