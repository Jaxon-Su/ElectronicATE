#include "msoseries456.h"
#include "oscilloscopeconfiguration.h"
#include <QCoreApplication>
#include <QMap>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const QString &message) { if (!ok) throw std::runtime_error(message.toStdString()); }
bool near(double a, double b) { return std::abs(a-b) <= std::abs(b)*1e-10; }
// Model the manual's coupled controls, independently of command compilation.
class Instrument final : public ICommunication {
public:
    double rate=6.25e9, scale=.001, points=62500000, divisions=10, capacity=62500000;
    QString mode="AUTO", adjustment="HORIZONTALSCALE", last, fail, cancelAfter;
    QStringList commands;
    QMap<QString,QByteArray> overrides;
    QAtomicInt *stop=nullptr;
    bool headers=false, consumed=false, readbackStarted=false, rejectReadback=false;
    bool open() override {return true;}
    void close() override {}
    bool isOpen() const override {return true;}
    QString lastError() const override {return "injected transport error";}
    void fit() { points=std::clamp(std::round(points),1000.0,capacity); scale=points/rate/divisions; }
    int write(const QByteArray &bytes) override {
        last=QString::fromUtf8(bytes).trimmed(); commands<<last;consumed=false;
        if (last==fail) return -1;
        if (last==cancelAfter && stop) stop->storeRelease(1);
        const auto verb=last.section(' ',0,0).toUpper();
        const auto value=last.section(' ',1).toUpper();
        if (verb=="HORIZONTAL:MODE") mode=value;
        else if (verb=="HORIZONTAL:MODE:MANUAL:CONFIGURE") adjustment=value;
        else if (verb=="HORIZONTAL:MODE:SAMPLERATE") {
            rate=std::min(value.toDouble(),6.25e9);
            if(adjustment=="RECORDLENGTH") points=rate*scale*divisions;
            fit();
        } else if (verb=="HORIZONTAL:SCALE") {scale=value.toDouble(); points=scale*rate*divisions;fit();}
        else if (verb=="HORIZONTAL:MODE:RECORDLENGTH") {points=value.toDouble();fit();}
        return bytes.size();
    }
    int read(QByteArray &bytes,int) override {
        if(consumed) {bytes.clear();return 0;} consumed=true;
        if(overrides.contains(last)) {bytes=overrides[last];return bytes.size();}
        QString value;
        if(last=="*ESR?") value=readbackStarted&&rejectReadback?"32":"0";
        else if(last=="HORizontal:MODe?") {readbackStarted=true;value=mode;}
        else if(last=="HORizontal:MODe:MANual:CONFigure?") value=adjustment;
        else if(last=="HORizontal:SCAle?") value=QString::number(scale,'g',16);
        else if(last=="HORizontal:MODe:SAMPLERate?") value=QString::number(rate,'g',16);
        else if(last=="HORizontal:MODe:RECOrdlength?") value=QString::number(points,'g',16);
        else if(last=="HORizontal:DIVisions?") value=QString::number(divisions);
        else return -1;
        if(headers) value=":"+last.chopped(1).toUpper()+" "+value;
        bytes=value.toUtf8();return bytes.size();
    }
};
OscilloscopeSettings decode(QString priority="RECORDLength") {
    return decodeOscilloscopeSettings({{"horz_mode","MANual"},{"horz_config",priority},
        {"timescale","20ms"},{"sample_rate","1MS/s"},{"record_length","200k"}},"MSOSeries456",4);
}
void successfulPairs() {
    for (bool slow : {false,true}) for (const QString &priority : {"RECORDLength","HORIZontalscale"}) {
        Instrument transport;MSOSeries456 scope(&transport);QAtomicInt stop{0};QString error;
        OscilloscopeConfigurationResult result;
        const auto settings=decodeOscilloscopeSettings({{"horz_mode","MANual"},{"horz_config",priority},
            {"timescale",slow ? "100us" : "16us"},{"sample_rate",slow ? "625M" : "1.25G"},
            {"record_length",slow ? "625k" : "200K"}},"MSOSeries456",4);
        check(scope.applySettings(settings,stop,error,&result),error);
        check(result.horizontal && near(result.horizontal->secondsPerDivision,slow ? 100e-6 : 16e-6)
              && result.horizontal->sampleRate==(slow ? 625000000 : 1.25e9)
              && result.horizontal->recordLength==(slow ? 625000 : 200000), "user requested pair not applied/read back");
        check(transport.commands.contains(slow ? "HORizontal:MODe:SAMPLERate 625000000" : "HORizontal:MODe:SAMPLERate 1250000000"),
              "shorthand rate not compiled as samples/s");
        check(transport.commands.filter("HORizontal:SCAle ").size()==(priority=="RECORDLength"),"user pair wrote dependent time");
        check(transport.commands.filter("HORizontal:MODe:RECOrdlength ").size()==(priority=="HORIZontalscale"),"user pair wrote dependent RL");
    }
    for (bool header : {false,true}) for (const QString &priority : {"RECORDLength","HORIZontalscale"}) {
        Instrument transport;transport.headers=header;
        MSOSeries456 scope(&transport);QAtomicInt stop{0};QString error;OscilloscopeConfigurationResult result;
        const auto settings=decode(priority);
        check(scope.applySettings(settings,stop,error,&result),error);
        check(result.horizontal.has_value(),"missing fresh snapshot");
        const auto a=*result.horizontal;
        check(near(a.secondsPerDivision,.02)&&near(a.sampleRate,1e6)&&a.recordLength==200000,"coupled pair not applied");
        check(transport.commands.filter("HORizontal:SCAle ").size()==(priority=="RECORDLength"),"wrong time ownership");
        check(transport.commands.filter("HORizontal:MODe:RECOrdlength ").size()==(priority=="HORIZontalscale"),"wrong RL ownership");
        check(!transport.commands.join('\n').contains("ACQuire:STATE"),"Write acquired/stopped a waveform");
    }
    Instrument transport;transport.divisions=8; MSOSeries456 scope(&transport);QAtomicInt stop{0};QString error;
    OscilloscopeConfigurationResult result;
    check(scope.applySettings(decode(),stop,error,&result),error);
    check(result.horizontal->divisions==8 && result.horizontal->recordLength==160000,"hardcoded actual divisions");
    transport.capacity=100000;
    check(scope.applySettings(decode(),stop,error,&result),error);
    check(result.horizontal->recordLength==100000 && near(result.horizontal->secondsPerDivision,.0125),"adjusted actual hidden");
    const auto automatic=decodeOscilloscopeSettings({{"horz_mode","AUTO"},{"timescale","1ms"},
        {"sample_rate","bad"},{"record_length","bad"}},"MSOSeries456",4);
    transport.commands.clear();
    check(scope.applySettings(automatic,stop,error,&result),error);
    check(result.horizontal->mode=="AUTO" && transport.commands.filter("SAMPLERate ").isEmpty()
          && transport.commands.filter("RECOrdlength ").isEmpty(),"Auto wrote manual values");
    transport.commands.clear();
    check(scope.applySettings({},stop,error,&result)&&!result.horizontal&&transport.commands.isEmpty(),"ignored configuration performed IO/reused snapshot");
}
void readbackFailures() {
    const QMap<QString,QStringList> invalid{
        {"HORizontal:MODe?",{"UNKNOWN"}},
        {"HORizontal:MODe:MANual:CONFigure?",{"UNKNOWN"}},
        {"HORizontal:SCAle?",{"0","nan","inf","9.91e37","garbage 0.02"}},
        {"HORizontal:MODe:SAMPLERate?",{"-1","bad"}},
        {"HORizontal:MODe:RECOrdlength?",{"1.5","0","9007199254740992"}},
        {"HORizontal:DIVisions?",{"0","bad"}}};
    for(auto it=invalid.begin();it!=invalid.end();++it) for(const auto &bad:it.value()) {
        Instrument transport;transport.overrides[it.key()]=bad.toUtf8();MSOSeries456 scope(&transport);
        QAtomicInt stop{0};QString error;OscilloscopeConfigurationResult result;result.horizontal.emplace();
        check(!scope.applySettings(decode(),stop,error,&result)&&!result.horizontal&&!error.isEmpty(),"invalid readback accepted: "+it.key()+"="+bad);
    }
    for(const QString &query : invalid.keys()) {
        Instrument transport;transport.fail=query;MSOSeries456 scope(&transport);QAtomicInt stop{0};QString error;
        check(!scope.applySettings(decode(),stop,error),"failed readback passed without result pointer");
        check(transport.commands.last()==query,"continued after read failure");
    }
    Instrument transport;transport.rejectReadback=true;MSOSeries456 scope(&transport);QAtomicInt stop{0};QString error;
    check(!scope.applySettings(decode(),stop,error)&&error.contains("ESR=32"),"readback ESR error ignored");
}
void cancellationAndDecode() {
    QStringList stages{"HORizontal:MODe MANUAL","HORizontal:MODe:MANual:CONFigure RECORDLENGTH",
        "HORizontal:MODe:SAMPLERate 1000000","HORizontal:SCAle 0.02","HORizontal:MODe?",
        "HORizontal:MODe:MANual:CONFigure?","HORizontal:SCAle?","HORizontal:MODe:SAMPLERate?",
        "HORizontal:MODe:RECOrdlength?","HORizontal:DIVisions?"};
    for(const auto &stage:stages) {
        Instrument transport;QAtomicInt stop{0};transport.stop=&stop;transport.cancelAfter=stage;
        MSOSeries456 scope(&transport);QString error;OscilloscopeConfigurationResult result;result.horizontal.emplace();
        check(!scope.applySettings(decode(),stop,error,&result)&&!result.horizontal&&error.contains("Stopped"),"Stop ignored at "+stage);
    }
    Instrument transport;MSOSeries456 scope(&transport);QAtomicInt stop{1};QString error;
    check(!scope.applySettings(decode(),stop,error)&&transport.commands.isEmpty(),"pre-stop performed IO");
    for(const QString &priority : {"RECORDLength","HORIZontalscale"}) {
        auto cfg=QVariantMap{{"horz_mode","MANUAL"},{"horz_config",priority},{"timescale","37us"},
            {"sample_rate","2.7MS/s"},{"record_length","12345"}};
        const auto settings=decodeOscilloscopeSettings(cfg,"MSOSeries456",4);
        check(settings.horizontal.sampleRate==std::optional<double>(2.7e6),"custom rate lost");
        if(priority=="RECORDLength") check(!settings.horizontal.recordLength,"preview RL treated as requested value");
        else check(!settings.horizontal.secondsPerDivision,"preview time treated as requested value");
    }
}
void rangePreflight() {
    QVariantMap cfg{{"horz_mode","MANUAL"},{"horz_config","RECORDLength"},
        {"horz_limits","MSO44B"},{"timescale","100us"},{"sample_rate","625MS/s"}};
    OscilloscopeSettings out;QString error;
    auto prepare=[&]{return prepareOscilloscopeSettings(cfg,"MSOSeries456",4,out,error);};
    check(prepare(),error);
    cfg["sample_rate"]="625kS/s";check(!prepare()&&error.contains("1k"),"runtime bypassed minimum RL");
    cfg["sample_rate"]="6.25GS/s";cfg["timescale"]="1ms";
    check(!prepare()&&error.contains("31.25M"),"runtime bypassed standard capacity");
    cfg["horz_limits"]="MSO44B_62M";check(prepare(),error);
    cfg["horz_config"]="HORIZontalscale";cfg["record_length"]="62.500001M";
    check(!prepare()&&error.contains("62.5M"),"runtime bypassed option capacity");
    cfg["horz_limits"]="UNSPECIFIED";check(prepare(),error);
    cfg["horz_limits"]="bogus";check(!prepare(),"invalid profile accepted");
    cfg["ignore_horizontal"]=true;check(prepare()&&!out.horizontal.mode,"ignored profile/settings decoded");
    cfg.remove("ignore_horizontal");cfg["horz_limits"]="MSO44B";cfg["horz_mode"]="AUTO";
    cfg["sample_rate"]="bad";cfg["record_length"]="bad";check(prepare(),error);
    cfg["timescale"]="1001s";check(!prepare()&&error.contains("1000"),"Auto range bypassed");
}

}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try { successfulPairs();readbackFailures();cancellationAndDecode();rangePreflight();
        std::cout<<"PASS coupled horizontal settings, actual readback, invalid/failed responses, ESR, Stop, custom values\n";
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
