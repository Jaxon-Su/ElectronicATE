#include "instrumentexecutor.h"
#include "dcinputoperation.h"
#include "communicationfactory.h"
#include "msoseries456.h"
#include "dcloadfactory.h"
#include "dcload.h"
#include <memory>
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

static QString mode;
static QStringList trace;
static QString failCommand;
class AuditComm : public ICommunication {
public:
    QString id; QByteArray reply = "OFF"; bool opened = false;
    explicit AuditComm(QString id) : id(std::move(id)) {}
    bool open() override { return opened = true; }
    void close() override { opened = false; }
    bool isOpen() const override { return opened; }
    QString lastError() const override { return "injected failure"; }
    int write(const QByteArray &bytes) override {
        const auto cmd = QString::fromUtf8(bytes).trimmed(); trace << id + ':' + cmd;
        if (mode == "dc_on_unknown" && id.contains("::2::") && cmd == "CONF:OUTP ON") throw 42;
        if (mode == "dc_rollback_unknown") {
            if (id.contains("::2::") && cmd == "CONF:OUTP ON") return -1;
            if (id.contains("::1::") && cmd == "CONF:OUTP OFF") throw 42;
        }
        if (mode == "dc_off_unknown" && id.contains("::2::") && cmd == "CONF:OUTP OFF") throw 42;
        if (!failCommand.isEmpty() && cmd.startsWith(failCommand)) return -1;
        return bytes.size();
    }
    int read(QByteArray &data, int) override { data = reply; return data.size(); }
};
ICommunication *CommunicationFactory::create(const QString &id) { return new AuditComm(id); }
class BrownOutComm : public AuditComm {
public:
    BrownOutComm() : AuditComm("brownout") {}
    QString last; bool failCleanup = false;
    QMap<QString, QByteArray> values{{"*ESR?","0"}, {"DISplay:GLObal:CH1:STATE?","ON"},
        {"DISplay:GLObal:CH2:STATE?","OFF"},{"DISplay:GLObal:CH3:STATE?","OFF"},{"DISplay:GLObal:CH4:STATE?","OFF"},
        {"HORizontal:FASTframe:STATE?","0"},{"HORizontal:RECOrdlength?","4"},{"CH1:SCAle?","1"},
        {"TRIGger:A:TYPe?","EDGE"},{"TRIGger:A:EDGE:SOUrce?","CH2"},{"TRIGger:A:EDGE:SLOpe?","RISE"},
        {"TRIGger:A:MODe?","AUTO"},{"TRIGger:A:LEVel:CH1?","3"},{"HORizontal:SCAle?",".01"},
        {"ACQuire:MODe?","HIRES"},{"ACQuire:STOPAfter?","RUNSTOP"},{"ACQuire:SEQuence:MODe?","NUMACQS"},
        {"ACQuire:SEQuence:NUMSEQuence?","3"},{"DATa:SOUrce?","CH2"},{"DATa:ENCdg?","ASCII"},
        {"DATa:WIDth?","1"},{"DATa:STARt?","2"},{"DATa:STOP?","3"}};
    int write(const QByteArray &b) override {
        last = QString::fromUtf8(b).trimmed();
        if (failCleanup && last == "ACQuire:STATE STOP") { trace << "brownout:STOP throws"; throw 42; }
        return AuditComm::write(b);
    }
    int read(QByteArray &b, int) override { b = values.value(last); return b.isEmpty() ? -1 : b.size(); }
};
static void check(bool ok, const char *text) { if (!ok) throw std::runtime_error(text); }
static InstrumentConfig instrument(QString name, QString type, QString model, int address) {
    InstrumentConfig i; i.enabled = true; i.name = name; i.type = type; i.modelName = model;
    i.commConfig.protocol = ProtocolType::GPIB; i.commConfig.gpibAddress = address; return i;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        InstrumentSetupConfig dc; dc.dcInputs = 3;
        for (int i = 1; i <= 3; ++i) dc.instruments << instrument(QString("DC Source%1").arg(i), "InputDCSource", "62050H-40", i);
        DcGroup rows = {{{"12", "1", ""}, {"12", "1", ""}, {"12", "1", ""}}};
        for (const QString fault : {"dc_on_unknown", "dc_rollback_unknown"}) {
            mode = fault; trace.clear(); bool escaped = false;
            InstrumentOperationResult result;
            try { result = runDcGroup(dc, rows, InputAction::PowerOn, CommunicationFactory::create); } catch (...) { escaped = true; }
            check(!escaped && !result.success && trace.contains("GPIB0::1::INSTR:CONF:OUTP OFF") && trace.contains("GPIB0::2::INSTR:CONF:OUTP OFF") && trace.contains("GPIB0::3::INSTR:CONF:OUTP OFF"), "DC rollback must continue");
            check(fault == "dc_on_unknown" ? result.confirmedOutput == false : !result.confirmedOutput.has_value(), "DC rollback confirmation incorrect");
            std::cout << fault.toStdString() << " escaped=" << escaped << " trace=" << trace.join('|').toStdString() << '\n';
        }
        mode = "dc_off_unknown"; trace.clear();
        const auto off = runDcGroup(dc, {}, InputAction::PowerOff, CommunicationFactory::create);
        check(!off.success && trace.contains("GPIB0::1::INSTR:CONF:OUTP OFF") && trace.contains("GPIB0::3::INSTR:CONF:OUTP OFF"), "Explicit DC OFF regression");
        std::cout << "Explicit DC OFF continues after unknown exception: verified\n";
        mode.clear();
        for (const QString model : {"63102", "63102A", "63640-80-80", "63804"}) {
            InstrumentSetupConfig c; auto load = instrument("Load1", "Load", "mainframe", 4);
            load.channels = {{model, 1, 0}}; load.channelNumbers = {1}; c.instruments << load;
            LoadMetaRow meta; meta.modes = {"CV"}; meta.ranges = {"Auto Range"};
            trace.clear(); const auto r = InstrumentExecutor::runLoad(c, {{"cv", {"12V/1A"}}}, 0, meta, LoadAction::LoadOn);
            check(!r.success && !r.errorMessage.isEmpty() && !trace.contains("GPIB0::4::INSTR:LOAD ON"), "Unsupported CV accepted");
            std::cout << "CV " << model.toStdString() << " success=" << r.success << " trace=" << trace.join('|').toStdString() << '\n';
        }
        for (bool mixed : {false, true}) {
            InstrumentSetupConfig c; auto load = instrument("Load1", "Load", "mainframe", 4);
            load.channels = {{"63202A-150-200", 1, 0}}; load.channelNumbers = {1}; c.instruments << load;
            LoadMetaRow meta; meta.modes = {"CV"}; meta.ranges = {"Auto Range"};
            QStringList values{"12V/1A"};
            if (mixed) {
                auto other = instrument("Load2", "Load", "mainframe", 6);
                other.channels = {{"63102", 2, 0}}; other.channelNumbers = {1}; c.instruments << other;
                meta.modes << "CV"; meta.ranges << "Auto Range"; values << "12V/1A";
            }
            trace.clear(); const auto r = InstrumentExecutor::runLoad(c, {{"cv", values}}, 0, meta, LoadAction::LoadOn);
            check(r.success == !mixed, "CV capability decision incorrect");
            check(trace.join('|').contains("LOAD ON") == !mixed, "Mixed CV partially enabled");
        }
        for (const QString model : {"61505", "61509", "6530", "DE-A3000AB"}) {
            InstrumentSetupConfig c; c.instruments << instrument("ACSource", "InputSource", model, 5);
            trace.clear(); failCommand = model == "6530" ? "RANGe" : "SOURce:VOLTage";
            const auto result = InstrumentExecutor::runInput(c, InputRow{}, InputAction::PowerOff);
            check(!result.success && result.confirmedOutput == false && trace.contains("GPIB0::5::INSTR:OUTPut OFF"), "AC zero-setting failure blocked OFF");
            std::cout << "AC " << model.toStdString() << " zero-setting failure still OFF: verified\n";
        }
        failCommand.clear();
        for (const QString model : {"63102", "63102A", "63640-80-80", "63202A-150-200", "63804"}) {
            AuditComm comm("load"); std::unique_ptr<DCLoad> load(DCLoadFactory::createDCLoad(model, &comm)); load->connect();
            for (const QString fault : {"write", "readback", "valid"}) {
                failCommand = fault == "write" ? "LOAD OFF" : QString{};
                comm.reply = fault == "readback" ? "garbage" : "OFF";
                bool rejected = false; try { load->setLoadOff(); } catch (...) { rejected = true; }
                check(rejected == (fault != "valid"), "Load OFF falsely accepted failed write/readback");
            }
            std::cout << "Load OFF " << model.toStdString() << " write/readback failures rejected: verified\n";
        }
        failCommand.clear(); AuditComm transport("scope"); MSOSeries456 scope(&transport); scope.connect();
        transport.reply = "garbage";
        const bool running = scope.isRunning();
        check(!running && !scope.lastError().isEmpty(), "Scope malformed state accepted");
        std::cout << "Scope malformed state: running=" << running << " error='" << scope.lastError().toStdString() << "'\n";
        for (const auto &reply : {"0", "STOP", "OFF", "1", "RUN", "ON", ":ACQUIRE:STATE 1", "ACQ:STATE 0"}) {
            transport.reply = reply; const bool state = scope.isRunning();
            check(scope.lastError().isEmpty(), "Valid STATE rejected");
            check(state == (transport.reply == "1" || transport.reply == "RUN" || transport.reply == "ON" || transport.reply.endsWith(" 1")), "Wrong STATE");
        }
        for (const auto &reply : {"0.5", "UNKNOWN", "RUNNING", "NONE", ":OTHER:STATE 1", "1;0"}) {
            transport.reply = reply; check(!scope.isRunning() && !scope.lastError().isEmpty(), "Invalid STATE accepted");
        }
        transport.reply = "RUNSTOP"; failCommand = "ACQuire:STOPAfter RUNSTop"; trace.clear(); scope.run();
        check(!scope.lastError().isEmpty() && !trace.contains("scope:ACQuire:STATE RUN"), "Scope RUN erased prior failure");
        std::cout << "Scope RUN aborted after failed STOPAfter: error='" << scope.lastError().toStdString() << "'\n";
        for (const auto &reply : {"RUNSTOP", "SEQUENCE", ":ACQUIRE:STOPAFTER SEQUENCE"}) {
            failCommand.clear(); transport.reply = reply; trace.clear(); scope.run();
            check(scope.lastError().isEmpty() && trace.contains("scope:ACQuire:STATE RUN"), "Valid RUN failed");
            if (transport.reply.contains("SEQUENCE")) check(!trace.contains("scope:ACQuire:STOPAfter RUNSTop"), "SEQUENCE overwritten");
        }
        for (const auto &fault : {"query", "invalid", "run"}) {
            transport.reply = QString(fault) == "invalid" ? "garbage" : "RUNSTOP";
            failCommand = QString(fault) == "query" ? "ACQuire:STOPAfter?" : (QString(fault) == "run" ? "ACQuire:STATE RUN" : "");
            trace.clear(); scope.run(); check(!scope.lastError().isEmpty(), "RUN failure not reported");
            if (QString(fault) != "run") check(!trace.contains("scope:ACQuire:STATE RUN"), "RUN continued after precondition failure");
        }
        failCommand.clear(); BrownOutComm brownComm; MSOSeries456 brownScope(&brownComm); brownScope.connect();
        auto settings = BrownOut::defaults(); settings["trigger_channel"] = 1;
        QAtomicInt stop{0}; QString error; auto session = brownScope.beginBrownOut(BrownOut::Settings(settings), stop, error);
        check(bool(session), "Brown Out fixture initialization failed"); trace.clear(); brownComm.failCleanup = true;
        bool escaped = false, finished = true; try { finished = session->finish(error); } catch (...) { escaped = true; }
        check(!escaped && !finished && !error.isEmpty() && trace.contains("brownout:ACQuire:MODe HIRES") && trace.contains("brownout:DATa:STOP 3"), "Brown Out cleanup must continue");
        const auto firstError = error; const auto firstTrace = trace;
        const bool retry = session->finish(error);
        check(!retry && error == firstError && trace == firstTrace, "Brown Out repeated finish lost failure");
        std::cout << "Brown Out scope cleanup continues after unknown exception; repeated finish success=" << retry << '\n';
        session.reset();
        brownComm.failCleanup = false; failCommand.clear();
        session = brownScope.beginBrownOut(BrownOut::Settings(settings), stop, error);
        check(bool(session), "Brown Out success fixture failed"); trace.clear();
        check(session->finish(error) && error.isEmpty(), "Normal cleanup failed");
        const auto cleanTrace = trace;
        check(session->finish(error) && error.isEmpty() && trace == cleanTrace, "Normal cleanup repeated IO");
        session.reset();
        session = brownScope.beginBrownOut(BrownOut::Settings(settings), stop, error);
        check(bool(session), "Brown Out standard failure fixture failed"); trace.clear(); failCommand = "ACQuire:STATE STOP";
        check(!session->finish(error) && !error.isEmpty() && trace.contains("brownout:DATa:STOP 3"), "Standard cleanup failure skipped remaining restore");
        const auto standardError = error;
        check(!session->finish(error) && error == standardError, "Standard cleanup failure not retained");
        return 0; // Regression: failures rejected and remaining cleanup attempted.
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
