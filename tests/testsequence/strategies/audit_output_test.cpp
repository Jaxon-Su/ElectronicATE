#include "instrumentexecutor.h"
#include "communicationfactory.h"
#include "dcloadfactory.h"
#include "acsourcefactory.h"
#include "relayfactory.h"
#include "chroma61505.h"
#include "measurementpreflight.h"
#include "testrunservice.h"
#include "testsequencetaskdefaults.h"
#include "manualcontrolviewmodel.h"
#include "testsequenceviewmodel.h"
#include "testsequencemodel.h"
#include "testrunservicefactory.h"
#include "testrunsession.h"
#include "reportwriter.h"
#include "testsequenceconditionmapping.h"
#include "../../../src/service/message/messageservice.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>
#include <atomic>

static void require(bool ok, const QString &message) { if (!ok) throw std::runtime_error(message.toStdString()); }
static QStringList trace;
static QString badOpen, badOff, badCreate;
// The audit explicitly supplies dependencies; this only satisfies the unused default composition symbol.
std::unique_ptr<TestRunService> makeTestRunService() { return std::make_unique<TestRunService>(TestRunDependencies{}); }
class Comm : public ICommunication {
public:
    QString id; bool opened = false;
    explicit Comm(QString id) : id(std::move(id)) {}
    bool open() override { trace << id + ":OPEN"; return opened = id != badOpen; }
    bool isOpen() const override { return opened; }
    void close() override { opened = false; }
    int write(const QByteArray &data) override { trace << id + ":" + QString::fromLatin1(data).trimmed(); return data.size(); }
    int read(QByteArray &data, int) override { data = "0\n"; return data.size(); }
    QString lastError() const override { return "injected open failure"; }
};
class Load : public DCLoad {
public:
    QString id;
    explicit Load(Comm *comm) : DCLoad(comm), id(comm->id) {}
    QString model() const override { return "63640-80-80"; }
    QString vendor() const override { return "fake"; }
    void setChannel(int ch) override { trace << id + QString(":CH%1").arg(ch); }
    void setLoadOn() override { trace << id + ":ON"; }
    void setLoadOff() override { trace << id + ":OFF"; if (id == badOff) throw std::runtime_error("OFF failed"); }
    void setStaticCurrent(const StaticCurrentParam &) override {}
    void setDynamicCurrent(const DynamicCurrentParam &) override {}
    LoadSyncCapability syncCapability() const override { return LoadSyncCapability::MasterSlaveSyncType; }
    void setSyncRun(bool on) override { trace << id + (on ? ":SYNC ON" : ":SYNC OFF"); }
};
ICommunication *CommunicationFactory::create(const QString &id) {
    if (id == badCreate) throw 42;
    return new Comm(id);
}
DCLoad *DCLoadFactory::createDCLoad(const QString &, ICommunication *comm) { return new Load(static_cast<Comm *>(comm)); }
ACSource *ACSourceFactory::createACSource(const QString &, ICommunication *comm) { return new Chroma61505(comm); }
RelayBase *RelayFactory::createRelay(const QString &, ICommunication *, quint8) { return nullptr; }

InstrumentConfig instrument(QString name, QString type, QString model, int address)
{
    InstrumentConfig i; i.name = name; i.type = type; i.modelName = model;
    i.commConfig.protocol = ProtocolType::GPIB; i.commConfig.gpibAddress = address;
    return i;
}
TestSequenceExecutionContext context()
{
    TestSequenceExecutionContext c;
    auto ac = instrument("ACSource", "InputSource", "61505", 1);
    auto dc = instrument("DC Source1", "InputDCSource", "62050H-40", 2);
    auto load = instrument("Load1", "Load", "63600-5", 3);
    load.channels = {{"63640-80-80", 1, 0}}; load.channelNumbers = {1};
    auto relay = instrument("Relay1", "Relay", "Modbus_RTU_4CH", 4);
    relay.channels = {{"relay", 1, 0}, {"relay", 2, 0}}; relay.channelNumbers = {1,2};
    c.instrumentSetup.instruments = {ac, dc, load, relay}; c.instrumentSetup.relayOutputs = 2;
    c.inputs = {{"1phase", "120", "60", "0"}};
    DcGroup dcRows; dcRows[0] = {"24", "1", "DC"}; c.dcInputs = {dcRows};
    c.loads = {{"L1", {"1"}}, {"L2", {"2"}}};
    c.loadMeta.modes = {"CC"}; c.loadMeta.ranges = {"Auto Range"}; c.loadMeta.vo = {"12"}; c.loadMeta.von = {"0"};
    c.dynamics = {{"DY1", {"1~2"}}, {"DY2", {"2~3"}}};
    c.dynamicMeta.ranges = {"Auto Range"}; c.dynamicMeta.vo = {"12"}; c.dynamicMeta.von = {"0"}; c.dynamicMeta.t1t2 = {"100~100", "100~100"};
    c.relays = {{"short", {"ON", "OFF"}}, {"discharge", {"OFF", "ON"}}};
    return c;
}
TaskPayload task(const QString &name, bool ac = true, bool dc = true)
{
    TaskPayload p; p.task.name = name;
    p.settings = testSequenceDefaultSettings(name).first().toMap();
    p.settings["ac_input_index"] = ac ? 0 : -1; p.settings["dc_input_index"] = dc ? 0 : -1;
    const auto kind = TestSequenceTaskKind::fromName(name);
    const auto key = TestSequenceTaskKind::loadKey(kind);
    p.settings[TestSequenceTaskKind::isGroup(kind) ? key + "_indices" : key + "_index"] =
        TestSequenceTaskKind::isGroup(kind) ? QVariant(QVariantList{0,1}) : QVariant(0);
    p.settings["delay_ms"] = 0;
    if (TestSequenceTaskKind::isSteady(kind)) {
        p.settings["power_off_ms"] = 0;
        p.settings["startup_ms"] = 0;
    }
    if (p.settings.contains("discharge_index")) p.settings["discharge_index"] = 1;
    if (p.settings.contains("relay_index")) p.settings["relay_index"] = 0;
    if (name == "Brown Out") p.settings["ramp_source"] = ac ? "AC" : "DC";
    return p;
}
class Scope : public Oscilloscope {
public:
    QString model() const override { return "MSOSeries456"; }
    QString vendor() const override { return "fake"; }
    int getTotalChannel() override { return 4; }
    QByteArray captureScreenshot(const QString &, const QString &) override { return {}; }
    bool prepareSteadyAcquisition(QAtomicInt &, QString &, bool) override { return true; }
    bool stopAcquisition(QString &) override { return true; }
};
class Strategy : public IMeasurementStrategy {
public:
    std::function<MeasurementResult()> run;
    explicit Strategy(std::function<MeasurementResult()> run) : run(std::move(run)) {}
    QString name() const override { return "audit fixture"; }
    MeasurementResult execute(QAtomicInt &) override { return run(); }
};
static void creatorCleanup()
{
    for (bool dynamic : {false,true}) for (const QString fault : {"open", "construct", "off"}) {
        auto c = context(); c.instrumentSetup.instruments.clear(); trace.clear(); badOpen.clear(); badOff.clear(); badCreate.clear();
        for (int n = 1; n <= 3; ++n) {
            auto i = instrument("Load" + QString::number(n), "Load", "63600-5", n);
            i.channels = {{"63640-80-80", n, n == 3 ? 1 : 0}}; i.channelNumbers = {1}; c.instrumentSetup.instruments << i;
        }
        const auto a = c.instrumentSetup.instruments[0].getResourceString(), b = c.instrumentSetup.instruments[1].getResourceString(), z = c.instrumentSetup.instruments[2].getResourceString();
        if (fault == "open") badOpen = b; else if (fault == "construct") badCreate = b; else badOff = b;
        const auto result = dynamic ? InstrumentExecutor::runDynamicLoad(c.instrumentSetup, {}, -1, {}, DynamicLoadAction::LoadOff)
                                    : InstrumentExecutor::runLoad(c.instrumentSetup, {}, -1, {}, LoadAction::LoadOff);
        require(!result.success && !result.confirmedOutput, "partial cleanup reported OFF");
        require(trace.contains(a + ":OFF") && trace.contains(z + ":OFF") && trace.contains(z + ":SYNC OFF"), "healthy load/sync cleanup was skipped");
        trace.clear();
        if (fault != "off") {
            auto built = InstrumentCreator::createDCLoads(c.instrumentSetup, TableKind::Load);
            require(!built.success && built.dcLoads().isEmpty(), "normal creation accepted partial devices");
            require(!trace.contains(z + ":OPEN"), "normal creation continued after failure");
        }
    }
    badOpen.clear(); badOff.clear(); badCreate.clear();
    trace.clear(); auto c = context();
    const auto result = InstrumentExecutor::runInput(c.instrumentSetup, InputRow{}, InputAction::PowerOff);
    require(result.success && result.confirmedOutput == false, "empty AC condition blocked OFF");
    require(trace.join('|').contains("OUTPut OFF"), "AC OFF was not sent");
}

static void preflight()
{
    for (const QString fault : {"valid", "dc", "ac", "mode", "range", "current", "dynamic", "timing", "duplicate"}) {
        auto c = context(); Scope scope; c.scope = &scope; int operations = 0;
        TaskPayload prior; prior.task.name = "Relay"; prior.settings["relay_index"] = 0;
        auto p = task(fault == "dynamic" || fault == "timing" ? "Dynamic Group Test" : "Static Test");
        if (fault == "dc") c.dcInputs[0][0].vin = "100";
        if (fault == "ac") c.inputs[0].frequency = "nan";
        if (fault == "mode") c.loadMeta.modes[0] = "CV";
        if (fault == "range") c.loadMeta.ranges[0] = "CCDL";
        if (fault == "current") c.loads[0].values[0] = "9999";
        if (fault == "dynamic") c.dynamics[1].values[0] = "bad";
        if (fault == "timing") c.dynamicMeta.t1t2[1] = "0";
        if (fault == "duplicate") c.instrumentSetup.instruments[1].commConfig = c.instrumentSetup.instruments[0].commConfig;
        if (fault == "valid") {
            validateMeasurementConditions(c, InputSelection(p.settings), 0, false);
            validateMeasurementConditions(c, InputSelection(p.settings), 1, true);
            continue;
        }
        TestRunDependencies d; d.validateMeasurement = validateMeasurementConditions;
        d.instruments.relay = [&](const auto &, const auto &, int, auto) { ++operations; return InstrumentOperationResult{}; };
        TestRunService service(d); QVector<TestSequenceResultRecord> results;
        QObject::connect(&service, &TestRunService::resultRecordsReady, [&](auto r) { results = r; });
        service.startTasks({prior,p}, c);
        require(operations == 0 && results.size() == 2 && results[0].status == "Skipped" && results[1].attempt == 0 && !results[1].error.isEmpty(), fault + ": invalid later measurement touched hardware");
    }
}

static void handoff(const QString &name, bool selectAc, bool failOff)
{
    auto c = context(); Scope scope; c.scope = &scope;
    bool acOn = false, dcOn = false, next = false; int nextMeasurements = 0;
    TestRunDependencies d; d.validateMeasurement = validateMeasurementConditions;
    auto check = [&] { if (next) require(selectAc ? !dcOn : !acOn, "None source remained ON during next task"); };
    d.instruments.input = [&](const auto &, const InputRow &, InputAction action) {
        if (action == InputAction::PowerOn) check();
        if (next && !selectAc && failOff && action == InputAction::PowerOff) return InstrumentOperationResult{false,"AC off failed"};
        acOn = action == InputAction::PowerOn; return InstrumentOperationResult{};
    };
    d.instruments.dcGroup = [&](const auto &, const DcGroup &, InputAction action) {
        if (action == InputAction::PowerOn) check();
        if (next && selectAc && failOff && action == InputAction::PowerOff) return InstrumentOperationResult{false,"DC off failed"};
        dcOn = action == InputAction::PowerOn; return InstrumentOperationResult{};
    };
    d.instruments.load = [&](const auto &, const auto &, int, const auto &, LoadAction action, bool) {
        if (action == LoadAction::LoadOn) check(); return InstrumentOperationResult{};
    };
    d.instruments.dynamic = [&](const auto &, const auto &, int, const auto &, DynamicLoadAction action, bool, bool) {
        if (action == DynamicLoadAction::LoadOn) check(); return InstrumentOperationResult{};
    };
    d.instruments.relay = [&](const auto &, const auto &, int, RelayAction action) { if (action != RelayAction::RelayOff) check(); return InstrumentOperationResult{}; };
    d.prepareBrownOut = [](const auto &, const auto &) { return []() -> std::unique_ptr<IBrownOutIo> { return {}; }; };
    d.strategy = [&](const MeasurementStrategyRequest &request) {
        return std::visit([&](const auto &r) -> std::unique_ptr<IMeasurementStrategy> {
            using R = std::decay_t<decltype(r)>;
            return std::make_unique<Strategy>([&, r] {
                check(); if (next) ++nextMeasurements;
                if constexpr (std::is_same_v<R, ScopeMeasurementRequest>) {
                    if (r.context.power) {
                        QString error; require(r.context.power(true,error), error); require(r.context.power(false,error),error);
                        // This handoff fixture must fulfill the transient cleanup contract.
                        // The real ordered discharge/retention sequence has separate strategy tests.
                        if (r.context.relays) require(r.context.relays(false,false,error), error);
                    }
                }
                MeasurementResult result; result.success = true; return result;
            });
        }, request);
    };
    TestRunService service(d); QVector<TestSequenceResultRecord> results; QVector<TableKind> uncertain;
    QObject::connect(&service, &TestRunService::taskStatusChanged, [&](int index, TaskStatus status) {
        if (index == 1 && status == TaskStatus::Running) { require(!acOn && !dcOn, "prior steady left sources ON"); next = true; }
    });
    QObject::connect(&service, &TestRunService::outputsUncertain, [&](const auto &, auto outputs) { uncertain = outputs; });
    QObject::connect(&service, &TestRunService::resultRecordsReady, [&](auto r) { results = r; });
    auto second = task(name, selectAc, !selectAc); second.retry = 1;
    service.startTasks({task("Static Test"), second}, c);
    require(results.size() == 2, "handoff retried despite cleanup failure");
    if (failOff) {
        require(nextMeasurements == 0 && results[1].status == "Fail" && uncertain.contains(selectAc ? TableKind::Dc : TableKind::Input), "None OFF failure did not block and persist UNKNOWN");
    } else {
        require(nextMeasurements > 0 && results[1].status == "Pass" && uncertain.isEmpty() && !acOn && !dcOn, results[1].error);
    }
}

static void recovery()
{
    auto c = context(); ManualControlModel model; ManualControlOperations ops; bool fail = true; int offCalls = 0;
    auto result = [&] { ++offCalls; return fail ? InstrumentOperationResult{false,"offline",true} : InstrumentOperationResult{}; };
    ops.input = [&](const auto &config, const InputRow &, InputAction action) { require(config.instruments[0].name == "ACSource" && action == InputAction::PowerOff, "recovery changed snapshot/action"); return result(); };
    ops.dcGroup = [&](const auto &, const auto &, auto action) { require(action == InputAction::PowerOff,"DC recovery enabled output"); return result(); };
    ops.load = [&](const auto &, const auto &, int, const auto &, auto action, bool) { require(action == LoadAction::LoadOff,"load recovery enabled output"); return result(); };
    ops.dynamic = [&](const auto &, const auto &, int, const auto &, auto action, bool, bool) { require(action == DynamicLoadAction::LoadOff,"dynamic recovery enabled output"); return result(); };
    ops.relay = [&](const auto &, const auto &, int, auto action) { require(action == RelayAction::RelayOff,"relay recovery enabled output"); return result(); };
    ManualControlViewModel vm(&model, ops);
    vm.adoptUncertainOutputs(c.instrumentSetup, {TableKind::Input,TableKind::Dc,TableKind::Load,TableKind::DyLoad,TableKind::Relay});
    require(vm.hasUnknownOutput() && vm.hasActiveControl(), "TestSequence UNKNOWN did not hold interlock/close state");
    auto wait = [&] { QElapsedTimer t; t.start(); while (vm.hasPendingControlOperation() && t.elapsed() < 4000) { QCoreApplication::processEvents(); QThread::msleep(1); } require(!vm.hasPendingControlOperation(),"recovery timeout"); };
    vm.handleInput(InputAction::PowerOff); wait();
    require(vm.outputState(TableKind::Input) == OutputState::Unknown, "failed OFF cleared uncertainty");
    fail = false;
    vm.handleInput(InputAction::PowerOff); wait();
    vm.onDcGroupToggled(false); wait();
    vm.handleLoad(LoadAction::LoadOff); wait();
    vm.handleDyLoad(DynamicLoadAction::LoadOff); wait();
    vm.handleRelay(RelayAction::RelayOff); wait();
    require(offCalls == 6 && !vm.hasUnknownOutput() && !vm.hasActiveControl(), "OFF without selected conditions did not release interlock");
}
static void sessionRecovery(bool stoppedRun = false)
{
    QObject::connect(&MessageService::instance(), &MessageService::warningRequested,
                     QCoreApplication::instance(), [](const QString &title, const QString &message) {
                         std::cerr << title.toStdString() << ": " << message.toStdString() << '\n';
                     });
    auto c = context(); bool failOff = true;
    std::atomic_bool relayOn{false};
    TestRunDependencies d;
    d.instruments.relay = [&](const auto &, const auto &, int, RelayAction action) {
        if (action != RelayAction::RelayOff) relayOn = true;
        return action == RelayAction::RelayOff && failOff ? InstrumentOperationResult{false,"OFF unavailable"} : InstrumentOperationResult{};
    };
    ManualControlModel manualModel; ManualControlOperations manual;
    manual.relay = d.instruments.relay;
    ManualControlViewModel manualControl(&manualModel, manual);
    TestSequenceModel model; model.instrumentSetupConfig = c.instrumentSetup; model.relayRows = c.relays;
    auto worker = std::make_unique<TestRunService>(d, nullptr);
    auto session = std::make_unique<TestRunSession>(std::move(worker), std::make_unique<ReportWriter>());
    TestSequenceViewModel testSequence(&model, std::move(session));
    QObject::connect(&testSequence, &TestSequenceViewModel::runningChanged, &manualControl,
                     [&](bool running) { manualControl.setControlAllowed(!running); });
    int deliveries = 0;
    QObject::connect(&testSequence, &TestSequenceViewModel::outputsUncertain, &manualControl, [&](const InstrumentSetupConfig &config, const auto &outputs) {
        require(QThread::currentThread() == QCoreApplication::instance()->thread(), "uncertainty delivered outside UI thread");
        require(testSequence.isRunning(), "uncertainty arrived after Run unlock");
        ++deliveries; manualControl.adoptUncertainOutputs(config, outputs);
    });
    testSequence.setOutputRecoveryCheck([&] { return !manualControl.hasActiveControl(); });
    TaskPayload relay; relay.report = false; relay.task.name = "Relay"; relay.settings["relay_index"] = 0;
    TestSequenceConditions::capture(relay.settings, c);
    auto wait = [&](auto done) { QElapsedTimer time; time.start(); while (!done() && time.elapsed() < 4000) { QCoreApplication::processEvents(); QThread::msleep(1); } require(done(), "session recovery timeout"); };
    QVector<TaskPayload> queue{relay};
    if (stoppedRun) {
        TaskPayload waitTask; waitTask.report = false; waitTask.task.name = "Delay";
        waitTask.settings["delay_ms"] = 10000; queue.append(waitTask);
    }
    require(testSequence.startExecution(queue), "could not start relay fixture");
    if (stoppedRun) {
        wait([&] { return relayOn.load(); });
        testSequence.stopExecution(); // MainWindow's pending close must stop, not destroy the session.
    }
    wait([&] { return !testSequence.isRunning(); });
    require(deliveries == 1 && manualControl.hasUnknownOutput(), "Worker/Session/VM lost uncertainty");
    require(!testSequence.isShutdownComplete(), "stop destroyed the recoverable session");
    TaskPayload delay; delay.report = false; delay.task.name = "Delay"; delay.settings["delay_ms"] = 0;
    testSequence.setControlAllowed(true); // Even a UI enable event must not erase the recovery latch.
    require(!testSequence.startExecution({delay}), "new Run bypassed UNKNOWN latch");
    manualControl.handleRelay(RelayAction::RelayOff); wait([&] { return !manualControl.hasPendingControlOperation(); });
    require(!testSequence.startExecution({delay}), "failed recovery unlocked Run");
    failOff = false; manualControl.handleRelay(RelayAction::RelayOff); wait([&] { return !manualControl.hasPendingControlOperation(); });
    require(!manualControl.hasUnknownOutput() && testSequence.startExecution({delay}), "output recovery did not unlock Run");
    wait([&] { return !testSequence.isRunning(); });
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        const auto mode = app.arguments().value(1);
        if (mode == "creator") creatorCleanup();
        else if (mode == "preflight") preflight();
        else if (mode == "recovery") recovery();
        else if (mode == "session") sessionRecovery();
        else if (mode == "close_recovery") sessionRecovery(true);
        else handoff(app.arguments().value(2), app.arguments().value(3) == "AC", mode == "off_failure");
        std::cout << "PASS " << app.arguments().join(' ').toStdString();
    } catch (const std::exception &e) { std::cerr << e.what(); return 1; }
}
