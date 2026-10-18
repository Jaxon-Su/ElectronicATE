#include "msoseries456.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
class Transport final : public ICommunication {
public:
    bool open() override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    QString lastError() const override { return "injected capture failure"; }
    void setTimeoutMs(int value) override { timeout = value; }
    int timeoutMs() const override { return timeout; }
    int write(const QByteArray &data) override {
        command=QString::fromUtf8(data).trimmed(); trace<<command;consumed=false;
        if (command.startsWith(failPrefix) && !failPrefix.isEmpty()) return -1;
        if (command.startsWith("SAVe:IMAGe ") || command.startsWith("SAVe:WAVEform CH")) pendingRejection=rejectSave;
        if (command.startsWith("SAVe:WAVEform CH")) fileName=command.section('"',1,1).section('/',-1);
        if (command.startsWith("FILESystem:CWD ")) cwd=command.section('"',1,1);
        if (rejectLdir && command.startsWith("FILESystem:LDIR?")) pendingRejection=true;
        return data.size();
    }
    int read(QByteArray &data,int) override {
        if (throwRead && command.startsWith("FILESystem:READFile")) throw std::runtime_error("read threw");
        if(consumed){data.clear();return 0;}consumed=true;
        if(command=="*ESR?"){data=pendingRejection?"32":"0";pendingRejection=false;}
        else if(command=="*OPC?") data="1";
        else if(command=="FILESystem:CWD?") data=('"'+cwd+'"').toUtf8();
        else if(command.startsWith("FILESystem:LDIR?")) { if(rejectLdir) return -1; data=('"'+fileName+";FILE;3;2026-10-15;10:00:00\"").toUtf8(); }
        else if(command.startsWith("FILESystem:READFile")){++fileReads;data="old";}
        else return -1;
        return data.size();
    }
    QString command,failPrefix,fileName,cwd="C:"; QStringList trace;
    int timeout=4321,fileReads=0;
    bool rejectLdir=false;
    bool consumed=false,rejectSave=false,pendingRejection=false,throwRead=false;
};
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    try {
        const QString scenario=app.arguments().value(1);
        if(scenario=="ldir_alternating_probe") {
            Transport transport; MSOSeries456 scope(&transport); scope.connect();
            transport.rejectLdir=true;
            for(int i=0;i<6;++i) {
                const auto bytes=scope.captureWaveformFile(1,"WFM",{},1,0);
                const bool ok=!bytes.isEmpty();
                std::cout << "WFM " << i+1 << ": " << (ok?"OK":"FAIL") << '\n';
                require(ok==(i%2==0),"alternating hypothesis did not reproduce");
            }
            require(!scope.captureWaveformFile(1,"WFM",{},1,0).isEmpty(),"WFM setup");
            require(scope.captureWaveformFile(1,"CSV",{},1,0).isEmpty(),"expected cross-format pending ESR rejection");
            require(!scope.captureWaveformFile(1,"CSV",{},1,0).isEmpty(),"CSV recovery");
            std::cout << "WFM -> CSV FAIL -> CSV OK reproduced with rejected LDIR argument\n";
            return 0;
        }
        for(const auto &format:QStringList{"PNG","WFM","CSV"}){
            Transport transport; MSOSeries456 scope(&transport);scope.connect();
            if(scenario=="save_write") transport.failPrefix=format=="PNG"?"SAVe:IMAGe ":"SAVe:WAVEform CH";
            if(scenario=="save_rejected") transport.rejectSave=true;
            if(scenario=="prepare_write") transport.failPrefix="*CLS";
            if(scenario=="gating_write" && format!="PNG") transport.failPrefix="SAVe:WAVEform:GATing";
            if(scenario=="opc") transport.failPrefix="*OPC?";
            if(scenario=="read_throw") transport.throwRead=true;
            QByteArray result; bool threw=false;
            try {result=format=="PNG"?scope.captureScreenshot(format,{}):scope.captureWaveformFile(1,format,{},1,0);}
            catch(const std::exception&){threw=true;}
            const bool success=scenario=="success" || (scenario=="gating_write" && format=="PNG");
            require(success?result=="old":result.isEmpty(),"failed capture returned a stale file");
            require(!success || !threw,"successful capture threw");
            require(transport.timeout==4321,"capture did not restore caller timeout");
            if(!success && scenario!="read_throw") require(transport.fileReads==0,"failed SAVE read stale file");
        }
        std::cout<<"PASS capture driver "<<scenario.toStdString()<<'\n';
    } catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
