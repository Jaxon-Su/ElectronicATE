#include "instrumentexecutor.h"
#include "dcinputoperation.h"
#include "communicationfactory.h"
#include "modbusrtuprotocol.h"
#include "manualcontrolviewmodel.h"
#include "testrunservice.h"
#include "controlpagelock.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QThreadPool>
#include <iostream>
#include <stdexcept>

static QString fault, badResource;
static QStringList trace;
static QMap<QString, QMap<int, bool>> coils;
static QSet<QString> enabledSources;
static void check(bool ok, const QString &message) { if (!ok) throw std::runtime_error(message.toStdString()); }
static void reset() { fault.clear(); badResource.clear(); trace.clear(); coils.clear(); enabledSources.clear(); }
static QByteArray frame(QByteArray bytes) {
    const auto crc = Modbus_RTU_Protocol::calcCRC(bytes);
    bytes.append(char(crc)); bytes.append(char(crc >> 8)); return bytes;
}
class Comm : public ICommunication {
public:
    QString resource, last; bool opened = false; QByteArray response;
    explicit Comm(QString value) : resource(std::move(value)) {}
    bool open() override { return opened = !(resource == badResource && fault == "open"); }
    bool isOpen() const override { return opened; }
    void close() override { opened = false; }
    QString lastError() const override { return "injected communication error"; }
    int write(const QByteArray &bytes) override {
        response.clear();
        const bool bad = resource == badResource;
        if (resource.startsWith("MODBUS")) {
            const int fn = quint8(bytes[1]);
            const int ch = quint8(bytes[3]);
            trace << resource + QString(":FC%1:%2").arg(fn).arg(ch);
            if (bad && fault == "throw") throw 42;
            if (fn == 1) {
                if (bad && (fault == "timeout" || (fault == "second_channel" && ch == 1))) return bytes.size();
                QByteArray payload;
                payload.append(bytes[0]); payload.append(char(1)); payload.append(char(1)); payload.append(char(coils[resource][ch]));
                response = frame(payload);
                if (bad && fault == "slave") response[0] = char(2);
                if (bad && fault == "crc") response[5] ^= 1;
            } else if (fn == 5) {
                const bool on = quint8(bytes[4]) == 0xff;
                coils[resource][ch] = on;
                if (!(bad && (fault == "echo" || fault == "timeout"))) response = bytes;
            }
        } else {
            last = QString::fromLatin1(bytes).trimmed(); trace << resource + ':' + last;
            if (bad && fault == "throw") throw 42;
            if (last == "OUTPut?" || last == "LOAD?" || last == "CONF:OUTP?") {
                if (bad && fault == "timeout") return bytes.size();
                response = bad && fault == "invalid" ? "garbage" : enabledSources.contains(resource) ? "ON" : "OFF";
            } else if (last == "OUTPut ON" || last == "LOAD ON" || last == "CONF:OUTP ON") {
                enabledSources.insert(resource);
                if (bad && fault == "enable") return -1;
            } else if (last == "OUTPut OFF" || last == "LOAD OFF" || last == "CONF:OUTP OFF") {
                if (bad && fault == "off") return -1;
                enabledSources.remove(resource);
            } else if (last.contains("SYNC:TYPE?")) response = "0";
        }
        return bytes.size();
    }
    int read(QByteArray &bytes, int count) override {
        bytes = response.left(count); response.remove(0, count); return bytes.isEmpty() ? -1 : bytes.size();
    }
};
ICommunication *CommunicationFactory::create(const QString &resource) { return new Comm(resource); }
static InstrumentConfig instrument(QString type, QString model, int index) {
    InstrumentConfig i; i.type = type; i.modelName = model;
    i.name = type == "InputSource" ? "ACSource" : type == "InputDCSource" ? QString("DC Source%1").arg(index) : type + QString::number(index);
    i.commConfig = CommunicationConfig::fromResourceString(type == "Relay"
        ? QString("MODBUS::COM%1:9600:8:N:1::SLAVE:1").arg(index + 3)
        : QString("GPIB0::%1::INSTR").arg(index));
    i.channels = {{model, index, 0}}; i.channelNumbers = {1}; return i;
}
static InstrumentOperationResult relay(const InstrumentSetupConfig &c, RelayAction action = RelayAction::RelayOn) {
    return InstrumentExecutor::runRelay(c, {{"on", {"ON", "ON", "ON"}}}, 0, action);
}
static void waitFor(const std::function<bool()> &done) {
    QElapsedTimer timer; timer.start();
    while (!done() && timer.elapsed() < 5000) { QCoreApplication::processEvents(); QThread::msleep(1); }
    check(done(), "async operation did not settle");
}
static void relayProbe() {
    InstrumentSetupConfig cfg; cfg.relayOutputs = 3;
    cfg.instruments = {instrument("Relay", "Modbus_RTU_4CH", 1), instrument("Relay", "Modbus_RTU_4CH", 2)};
    cfg.instruments[1].channels.append({"", 3, 0}); cfg.instruments[1].channelNumbers.append(2);
    for (const QString mode : {"open", "timeout", "slave", "crc", "throw", "second_channel"}) {
        reset(); fault = mode; badResource = cfg.instruments[1].getResourceString();
        const auto result = relay(cfg);
        check(!result.success && result.outputUnchanged && !result.confirmedOutput.has_value(), "Relay probe failure lost no-write evidence: " + mode);
        check(!trace.join('|').contains(":FC5:"), "Relay batch partially enabled before all probes passed: " + mode);
        OutputOperationState state;
        state.complete(state.begin({{TableKind::Relay, 0}}, true), result);
        check(!state.hasActiveControl(), "bad COM generated UNKNOWN from OFF: " + mode);
        state.markUnknown({TableKind::Relay, 0});
        state.complete(state.begin({{TableKind::Relay, 0}}, true), result);
        check(state.hasUnknownOutput(), "no-write result cleared old uncertainty");
        trace.clear(); relay(cfg, RelayAction::RelayOff);
        check(trace.join('|').contains(cfg.instruments[0].getResourceString() + ":FC5:0"), "unreachable peer blocked healthy Relay OFF");
    }
    reset(); fault = "timeout"; badResource = cfg.instruments[1].getResourceString();
    coils[cfg.instruments[0].getResourceString()][0] = true;
    const auto observed = relay(cfg);
    check(!observed.success && observed.confirmedOutput == true && !trace.join('|').contains(":FC5:"), "observed ON lost on later failed probe");
    reset(); fault = "echo"; badResource = cfg.instruments[0].getResourceString();
    const auto sent = relay(cfg);
    check(!sent.success && !sent.outputUnchanged && trace.join('|').contains(":FC5:"), "post-write failure falsely classified unchanged");
    reset(); check(relay(cfg).success, "valid relay failed");
    check(relay(cfg, RelayAction::RelayOff).confirmedOutput == false, "valid relay OFF failed");
}
static void sourceProbe() {
    for (const QString type : {"AC", "DC", "DCGroup", "Load", "Dynamic"}) {
        QStringList models = type == "AC" ? QStringList{"61505", "61509", "6530", "DE-A3000AB"} :
            (type == "DC" || type == "DCGroup") ? QStringList{"62050H-40"} :
            QStringList{"63102", "63102A", "63640-80-80", "63202A-150-200", "63804"};
        for (const auto &model : models) {
            InstrumentSetupConfig cfg; cfg.dcInputs = type == "DCGroup" ? 3 : 1;
            const QString instType = type == "AC" ? "InputSource" : type.startsWith("DC") ? "InputDCSource" : "Load";
            const int count = type == "DCGroup" ? 3 : instType == "Load" ? 2 : 1;
            for (int n = 1; n <= count; ++n) cfg.instruments << instrument(instType, model, n);
            auto execute = [&](bool off = false) {
                if (type == "AC") return InstrumentExecutor::runInput(cfg, InputRow{"1phase", "120", "60", "0"}, off ? InputAction::PowerOff : InputAction::PowerOn);
                DcGroup dc = {{{"12", "1", ""}, {"12", "1", ""}, {"12", "1", ""}}};
                if (type == "DC") return runDcInput(cfg, 0, dc[0], off ? InputAction::PowerOff : InputAction::PowerOn, CommunicationFactory::create);
                if (type == "DCGroup") return runDcGroup(cfg, dc, off ? InputAction::PowerOff : InputAction::PowerOn, CommunicationFactory::create);
                if (type == "Load") return InstrumentExecutor::runLoad(cfg, {{"load", {"1", "1"}}}, 0, {}, off ? LoadAction::LoadOff : LoadAction::LoadOn);
                DynamicMetaRow meta; meta.t1t2 = {"1~1", "1~1"};
                return InstrumentExecutor::runDynamicLoad(cfg, {{"load", {"1~2", "1~2"}}}, 0, meta, off ? DynamicLoadAction::LoadOff : DynamicLoadAction::LoadOn);
            };
            for (const QString mode : {"open", "timeout", "invalid", "throw"}) {
                reset(); fault = mode; badResource = cfg.instruments.last().getResourceString();
                const auto r = execute();
                check(!r.success && r.outputUnchanged, type + '/' + model + '/' + mode + " preflight failed classification: " + r.errorMessage);
                for (const auto &cmd : trace) check(cmd.endsWith('?') || cmd.contains(":CHAN "), "setting/enable before all probes passed: " + cmd);
                OutputOperationState state;
                state.complete(state.begin({{TableKind::Input, 0}}, true), r);
                check(!state.hasActiveControl(), "failed probe stranded OFF state");
            }
            if (count > 1) {
                reset(); enabledSources.insert(cfg.instruments[0].getResourceString());
                fault = "timeout"; badResource = cfg.instruments.last().getResourceString();
                const auto observed = execute();
                check(!observed.success && observed.outputUnchanged && observed.confirmedOutput == true,
                      "observed active source/load lost on later probe failure: " + type + '/' + model);
            }
            reset(); const auto on = execute();
            const bool unsupported = type == "Dynamic" && model == "63804";
            if (unsupported)
                check(!on.success && on.errorMessage.contains("Unsupported dynamic current segment count") &&
                      !trace.join('|').contains("LOAD ON"), "63804 unsupported Dynamic changed");
            else check(on.success, type + '/' + model + " valid ON failed: " + on.errorMessage);
            const auto off = execute(true); check(off.success, type + '/' + model + " valid OFF failed: " + off.errorMessage);
            if (unsupported) continue;
            reset(); fault = "enable"; badResource = cfg.instruments[0].getResourceString();
            const auto failure = execute();
            check(!failure.success && !failure.outputUnchanged, "enable write failure lost uncertainty: " + type + '/' + model);
        }
    }
}
static void statePolicy() {
    for (const auto kind : {TableKind::Input, TableKind::Dc, TableKind::Load, TableKind::DyLoad, TableKind::Relay}) {
        OutputOperationState state;
        state.complete(state.begin({{kind, 0}}, false), {false, "OFF connection failed before write", true});
        check(!state.hasActiveControl(), "no-write OFF failure created UNKNOWN");
        state.complete(state.begin({{kind, 0}}, true), {});
        state.complete(state.begin({{kind, 0}}, false), {false, "OFF failed"});
        check(state.hasUnknownOutput(), "failed OFF cleared ON");
        state.complete(state.begin({{kind, 0}}, false), {false, "no communication", true});
        check(state.hasUnknownOutput(), "failed reconnect cleared UNKNOWN");
        state.complete(state.begin({{kind, 0}}, false), {});
        check(!state.hasActiveControl(), "successful OFF remained locked");
        state.complete(state.begin({{kind, 0}}, false), {false, "OFF sent but readback failed"});
        check(state.hasUnknownOutput(), "failed OFF readback was treated as preflight failure");
        state.complete(state.begin({{kind, 0}}, false), {});
        state.complete(state.begin({{kind, 0}}, std::nullopt), {false, "partial Change"});
        check(state.hasUnknownOutput(), "partial Change failed without locking");
    }
}
static void manualControlCorrection() {
    reset();
    InstrumentSetupConfig cfg; cfg.instruments = {instrument("Relay", "Modbus_RTU_4CH", 1)};
    ManualControlOperations ops; ops.connectScopes = [](const auto &) { return OscilloscopeManager::OscMap{}; };
    ops.relay = InstrumentExecutor::runRelay;
    ManualControlModel model; ManualControlViewModel vm(&model, ops);
    QTabWidget tabs; for (int i = 0; i < 5; ++i) tabs.addTab(new QWidget, QString::number(i));
    QObject::connect(&vm, &ManualControlViewModel::controlActiveChanged, [&] { applyControlPageLock(&tabs, {vm.hasActiveControl(), false, false}); });
    int accepted = 0;
    QObject::connect(&vm, &ManualControlViewModel::instrumentSetupConfigChanged, [&] { ++accepted; });
    vm.onInstrumentSetupConfigChanged(cfg); waitFor([&] { return accepted == 1 && !vm.isConfigurationBusy(); });
    vm.onRelayRowsChanged({{"on", {"ON"}}}); vm.onSelected(TableKind::Relay, 0, "on");
    fault = "timeout"; badResource = cfg.instruments[0].getResourceString();
    vm.handleRelay(RelayAction::RelayOn); check(!tabs.isTabEnabled(0), "pending operation unlocked InstrumentSetup");
    waitFor([&] { return !vm.hasPendingControlOperation(); });
    check(tabs.isTabEnabled(0) && !vm.hasUnknownOutput(), "wrong COM stranded InstrumentSetup");
    cfg.instruments[0].commConfig.portName = "COM8";
    vm.onInstrumentSetupConfigChanged(cfg); waitFor([&] { return accepted == 2 && !vm.isConfigurationBusy(); });
    vm.handleRelay(RelayAction::RelayOn); waitFor([&] { return !vm.hasPendingControlOperation(); });
    check(vm.outputState(TableKind::Relay) == OutputState::ConfirmedOn && !tabs.isTabEnabled(0), "corrected COM not used or ON unlocked");
    fault = "echo"; badResource = cfg.instruments[0].getResourceString();
    vm.handleRelay(RelayAction::RelayOff); waitFor([&] { return !vm.hasPendingControlOperation(); });
    check(vm.hasUnknownOutput() && !tabs.isTabEnabled(0), "post-write OFF failure unlocked");
    auto replacement = cfg; replacement.instruments[0].commConfig.portName = "COM9";
    vm.onInstrumentSetupConfigChanged(replacement); fault.clear(); trace.clear();
    vm.handleRelay(RelayAction::RelayOff); waitFor([&] { return !vm.hasPendingControlOperation(); });
    check(tabs.isTabEnabled(0) && !vm.hasActiveControl(), "successful OFF failed recovery");
    check(trace.join('|').contains("MODBUS::COM8:") && !trace.join('|').contains("MODBUS::COM9:"), "cleanup silently retargeted old instrument");
    waitFor([&] { return accepted == 3 && !vm.isConfigurationBusy(); });
}
static void testSequenceProbe() {
    for (const QString mode : {"timeout", "echo"}) {
        reset();
        TestSequenceExecutionContext context; context.instrumentSetup.instruments = {instrument("Relay", "Modbus_RTU_4CH", 1)};
        context.relays = {{"on", {"ON"}}};
        fault = mode; badResource = context.instrumentSetup.instruments[0].getResourceString();
        TestRunDependencies deps; deps.instruments.relay = InstrumentExecutor::runRelay;
        TestRunService service(deps); QVector<TableKind> uncertain; QVector<TestSequenceResultRecord> records;
        QObject::connect(&service, &TestRunService::outputsUncertain, [&](const auto&, const auto &outputs, const auto&) { uncertain = outputs; });
        QObject::connect(&service, &TestRunService::resultRecordsReady, [&](const auto &value) { records = value; });
        TaskPayload task; task.task.name = "Relay"; task.settings = {{"relay_index", 0}};
        service.startTasks({task}, context);
        check(records.size() == 1 && records[0].status == "Fail", "TestSequence communication failure reported success");
        check(uncertain.contains(TableKind::Relay) == (mode == "echo"), "TestSequence no-write/changed output handoff incorrect");
    }
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        const QString scenario = argc > 1 ? argv[1] : "relay";
        if (scenario == "relay") relayProbe();
        else if (scenario == "sources") sourceProbe();
        else if (scenario == "state") statePolicy();
        else if (scenario == "manualControl") manualControlCorrection();
        else if (scenario == "testSequence") testSequenceProbe();
        else throw std::runtime_error("unknown scenario");
        check(QThreadPool::globalInstance()->waitForDone(3000), "worker leak");
        std::cout << "PASS connection recovery " << scenario.toStdString() << '\n';
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
