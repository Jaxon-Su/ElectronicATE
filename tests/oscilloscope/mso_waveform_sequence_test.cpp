#include "msoseries456.h"
#include "msofilelisting.h"
#include "strategywaveformcapture.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>

void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }

// Stateful fake transport: a query must be fully consumed before the next write.
// LDIR uses the documented grammar and rejects the former file-path argument.
class Transport : public ICommunication {
public:
    bool open() override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    QString lastError() const override { return "injected transport error"; }
    int timeoutMs() const override { return timeout; }
    void setTimeoutMs(int value) override { timeout = value; }
    bool deviceClear() override {
        ++clears;
        if (clearFails) return false;
        reply.clear(); offset = 0; readError = false;
        return true;
    }
    int write(const QByteArray &bytes) override {
        require(offset == reply.size(), "previous response was not fully consumed");
        reply.clear(); offset = 0;
        command = QString::fromUtf8(bytes).trimmed();
        trace << command;
        auto quotedArg = [&] { return command.section('"', 1, 1); };
        if (command == "*ESR?") { reply=QByteArray::number(esr); esr=0; }
        else if (command == "*OPC?") {
            reply="1";
            if (opcReadFails) { opcReadFails=false; readError=true; esr=4; }
        }
        else if (command == "*CLS") esr=0;
        else if (command == "ACQuire:STATE?") reply="0";
        else if (command == "FILESystem:CWD?") reply=(":FILESYSTEM:CWD \""+cwd+"\"\n").toUtf8();
        else if (command.startsWith("FILESystem:CWD ")) {
            if (restoreFails && quotedArg()==originalCwd) { esr=32; restoreFails=false; }
            else cwd=quotedArg();
        }
        else if (command.startsWith("SAVe:WAVEform CH")) {
            filePath=quotedArg();
            require(!usedPaths.contains(filePath), "temporary filename reused");
            usedPaths.insert(filePath);
            const auto payload=filePath.endsWith(".wfm") ? wfm : csv;
            files.insert(filePath,payload);
        }
        else if (command == "FILESystem:LDIR?") {
            ++listings;
            require(cwd==filePath.left(filePath.lastIndexOf('/')), "LDIR used wrong directory");
            if (rejectListing) { esr=32; rejectListing=false; }
            if (listingReadFails) { esr=32; listingReadFails=false; readError=true; }
            reply=":FILESYSTEM:LDIR ";
            if (longListing) {
                for(int i=0;i<1800;++i)
                    reply += QString("\"other,%1;FILE;123;2026-10-15;01:02:03\", ").arg(i).toUtf8();
            }
            const QString name=missingFile ? "not-the-waveform.wfm" : filePath.section('/',-1);
            const QString size=badSize ? "invalid" : QString::number(files[filePath].size());
            reply += ('"'+name+";FILE;"+size+";2026-10-15;01:02:03\"\n").toUtf8();
            if (malformed) reply.chop(2);
        }
        else if (command.startsWith("FILESystem:LDIR?")) { esr=32; readError=true; }
        else if (command.startsWith("FILESystem:READFile ")) {
            ++fileReads;
            require(files.contains(quotedArg()), "read unknown file");
            reply=files[quotedArg()];
            if (shortRead) reply.chop(1);
            reply += fileReplySuffix;
            if (fileReadFails) { fileReadFails=false; readError=true; esr=4; }
        }
        else if (command.startsWith("FILESystem:DELEte ")) {
            if (deleteFails) { esr=32; deleteFails=false; }
            else files.remove(quotedArg());
        }
        return bytes.size();
    }
    int read(QByteArray &bytes, int maximum) override {
        if (readError) { bytes.clear(); return -1; }
        const int length=qMin(maximum,int(reply.size()-offset));
        bytes=reply.mid(offset,length); offset+=length;
        return length;
    }
    const QString originalCwd="C:/User Settings,1";
    QString cwd=originalCwd, command, filePath;
    QByteArray wfm=QByteArray(400128,'\0'), csv="Model,MSO44B\nTIME,CH1\n0,1\n";
    QByteArray fileReplySuffix;
    QMap<QString,QByteArray> files;
    QSet<QString> usedPaths;
    QStringList trace;
    QByteArray reply;
    qsizetype offset=0;
    int timeout=4321,esr=0,listings=0,fileReads=0,clears=0;
    bool rejectListing=false,listingReadFails=false,readError=false,longListing=false;
    bool malformed=false,missingFile=false,badSize=false,shortRead=false,fileReadFails=false;
    bool restoreFails=false,deleteFails=false,clearFails=false,opcReadFails=false;
};

void parserCases()
{
    const QByteArray reply=":FILESYSTEM:LDIR \"folder;DIR;0;date;time\", \"a,b;FILE;400128;date;time\"\n";
    require(MsoFileListing::fileSize(reply,"a,b")==400128,"comma filename");
    require(MsoFileListing::currentDirectory(":FILESYSTEM:CWD \"C:/User Files\"\n")=="C:/User Files","CWD header");
    for(const auto &bad: {QByteArray("\"wave;FILE;1;date;time"),
                         QByteArray("\"wave\",4,400128"),
                         QByteArray("\"wave;FILE;-1;date;time\""),
                         QByteArray("\"wave;FILE;1;date;time\",\"wave;FILE;2;date;time\""),
                         QByteArray("\"wave;FILE;9999999999999999999999;date;time\""),
                         QByteArray("\"wave;DIR;2;date;time\"")}) {
        bool threw=false;
        try { MsoFileListing::fileSize(bad,"wave"); } catch(...) { threw=true; }
        require(threw,"invalid LDIR accepted");
    }
}

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    try {
        const QString mode=app.arguments().value(1);
        if(mode=="parser") { parserCases(); return 0; }
        Transport t; MSOSeries456 scope(&t); scope.connect();
        scope.setScopeSaveDir("C:/Capture Files/");
        if(mode=="file_terminator" || mode=="invalid_file_suffix") {
            QTemporaryDir directory;
            require(directory.isValid(),"temporary output directory");
            const QList<QByteArray> suffixes = mode=="file_terminator"
                ? QList<QByteArray>{QByteArray(), QByteArray("\n")}
                : QList<QByteArray>{QByteArray(1,'\0'), "\r", " ", "X", "\n\n", "\r\n", "X\n"};
            for(int length:{1,65535,65536,65537,400128}) {
                for(const auto &ending:QList<QByteArray>{QByteArray(1,'\0'),"\n","\r\n"," \n"}) {
                    t.wfm=QByteArray(length,'\0')+ending;
                    t.csv="Model,MSO44B\nTIME,CH1\n0,1\n"+ending;
                    for(const auto &suffix:suffixes) {
                        t.fileReplySuffix=suffix;
                        for(const auto &format:QStringList{"WFM","CSV"}) {
                            const QString output=directory.filePath("wave."+format.toLower());
                            QFile::remove(output);
                            const bool ok=scope.captureWaveformFileToHost(1,output,format);
                            if(mode=="file_terminator") {
                                require(ok,"complete file plus optional LF rejected");
                                QFile file(output); require(file.open(QIODevice::ReadOnly),"host file missing");
                                require(file.readAll()==(format=="WFM"?t.wfm:t.csv),"file contents trimmed or suffix saved");
                            } else {
                                require(!ok && !QFile::exists(output),"unexpected suffix saved as a valid file");
                                require(scope.lastError().contains("size mismatch"),"lost size mismatch");
                                require(scope.lastError().contains("extra bytes (hex)="+QString::fromLatin1(suffix.toHex(' '))),"missing suffix diagnostic");
                            }
                            require(t.esr==0 && t.files.isEmpty(),"status/file leaked after suffix handling");
                            require(t.cwd==t.originalCwd && t.timeout==4321,"directory/timeout leaked");
                        }
                    }
                }
            }
        } else if(mode=="testSequence" || mode=="test_sequence_failure" || mode=="test_sequence_lf") {
            if(mode=="test_sequence_lf") t.fileReplySuffix="\n";
            QTemporaryDir directory; QAtomicInt stop(0);
            CaptureOptions options; options.channel=1; options.formats={"WFM","CSV"};
            StrategyWaveformCapture capture(options,directory.path(),stop,"AC","DC","Static Test",{1,1,QDateTime::currentDateTimeUtc()});
            ScopeCaptureRecord record; record.phase="Measurement"; record.channels.append({1,2,-1,1,0});
            if(mode=="test_sequence_failure") {
                t.rejectListing=true;
                QString error;
                try { capture.record(&scope,record,{}); } catch(const std::exception &e){error=e.what();}
                require(error.contains("LDIR") && error.contains("ESR=32"),"TestSequence lost current error");
                require(capture.files().isEmpty() && t.esr==0,"TestSequence retained failed result/status");
            }
            for(int i=0;i<4;++i) capture.record(&scope,record,{});
            require(capture.files().size()==8,"TestSequence alternating files missing");
            for(const auto &path:capture.files()) {
                QFile file(path); require(file.open(QIODevice::ReadOnly),"missing TestSequence file");
                require(file.readAll()==(path.endsWith(".wfm")?t.wfm:t.csv),"TestSequence bytes changed");
            }
        } else if(mode=="repeat" || mode=="alternate" || mode=="long_directory" || mode=="repeat_lf" || mode=="alternate_lf") {
            t.longListing=mode=="long_directory";
            if(mode.endsWith("_lf")) t.fileReplySuffix="\n";
            for(int i=0;i<12;++i) {
                const QString format=mode.startsWith("alternate") && i%2 ? "CSV" : "WFM";
                const auto data=scope.captureWaveformFile(1,format);
                require(data==(format=="CSV"?t.csv:t.wfm),"sequence bytes missing/changed");
                require(scope.lastError().isEmpty() && t.esr==0,"latent status after success");
                require(t.cwd==t.originalCwd && t.timeout==4321,"directory/timeout not restored");
                require(t.files.isEmpty(),"temporary file not cleaned");
            }
        } else {
            t.rejectListing=mode=="ldir_rejected";
            t.opcReadFails=mode=="opc_read_failure";
            t.listingReadFails=mode=="ldir_read_failure" || mode=="clear_failure";
            t.clearFails=mode=="clear_failure";
            t.badSize=mode=="bad_size";
            t.missingFile=mode=="missing";
            t.malformed=mode=="malformed";
            t.shortRead=mode=="short_file";
            t.fileReadFails=mode=="file_read_failure";
            t.restoreFails=mode=="restore_failure";
            t.deleteFails=mode=="delete_failure";
            require(scope.captureWaveformFile(1,"WFM").isEmpty(),"fault passed as success");
            const QString error=scope.lastError();
            require(!error.isEmpty(),"cleanup erased failure");
            require(t.timeout==4321,"timeout leaked on failure");
            if(mode=="clear_failure") {
                require(t.trace.last()=="FILESystem:LDIR?","used unrecovered transport");
                require(error.contains("recovery failed"),"missing recovery error");
            } else {
                require(t.esr==0,"current operation left latent ESR");
                if(mode!="restore_failure") require(t.cwd==t.originalCwd,"CWD not restored on failure");
                if(mode=="ldir_rejected" || mode=="ldir_read_failure")
                    require(error.contains("LDIR") && error.contains("ESR=32") && t.fileReads==0,"LDIR error ignored/misattributed");
                t.badSize=t.missingFile=t.malformed=t.shortRead=false;
                require(scope.captureWaveformFile(1,"CSV")==t.csv,"next CSV contaminated");
                require(scope.captureWaveformFile(1,"WFM")==t.wfm,"next WFM contaminated");
            }
        }
        std::cout<<"PASS "<<mode.toStdString()<<'\n';
        return 0;
    } catch(const std::exception &e){ std::cerr<<e.what()<<'\n'; return 1; }
}
