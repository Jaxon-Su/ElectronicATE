#define main previous_audit_main
#include "audit_output_test.cpp"
#undef main

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        const QString name = app.arguments().value(1), fault = app.arguments().value(2);
        auto c = context(); auto scope = std::make_shared<Scope>(); c.scope = scope.get();
        if (fault == "dc2") { c.instrumentSetup.dcInputs = 2; c.instrumentSetup.instruments[1].name = "DC Source2"; }
        bool ac = false, dc = false, sh = false, dis = false, failOff = false;
        int attempts = 0, relayCalls = 0, dcOffCalls = 0;
        TestRunDependencies deps;
        deps.validateMeasurement = [](const auto&, auto, int, bool) {};
        auto source = [&](bool isAc, InputAction action) -> InstrumentOperationResult {
            if (!isAc && action == InputAction::PowerOff) ++dcOffCalls;
            if (action == InputAction::PowerOff && failOff && ((fault == "dc" || fault == "dc2") != isAc)) {
                if (fault == "throw") throw 42;
                return {false, "injected OFF failure"};
            }
            (isAc ? ac : dc) = action == InputAction::PowerOn;
            return {};
        };
        deps.instruments.input = [&](const auto&, const auto&, auto action) { return source(true, action); };
        deps.instruments.dcGroup = [&](const auto&, const auto&, auto action) { return source(false, action); };
        deps.instruments.load = [](const auto&, const auto&, int, const auto&, auto, bool) { return InstrumentOperationResult{}; };
        deps.instruments.relay = [&](const auto&, const auto& rows, int index, RelayAction action) {
            ++relayCalls;
            const bool nextShort = action != RelayAction::RelayOff && rows[index].values[0] == "ON";
            const bool nextDischarge = action != RelayAction::RelayOff && rows[index].values[1] == "ON";
            require(nextShort || (!ac && !dc), "deferred/manual Relay OFF bypassed failed Input OFF");
            require(!nextDischarge || (!ac && !dc), "discharge under power");
            sh = nextShort; dis = nextDischarge;
            return InstrumentOperationResult{};
        };
        deps.strategy = [&](const MeasurementStrategyRequest& request) -> std::unique_ptr<IMeasurementStrategy> {
            const auto r = std::get<ScopeMeasurementRequest>(request);
            return std::make_unique<Strategy>([&, r] {
                ++attempts; QString error;
                require(r.context.relays(true, false, error), error);
                require(r.context.power(true, error), error);
                failOff = true;
                require(!r.context.power(false, error), "injection failed");
                MeasurementResult result; result.success = false; result.cleanupFailed = true;
                result.error = error; return result;
            });
        };
        ManualControlModel model; ManualControlOperations manual;
        manual.input = deps.instruments.input; manual.dcGroup = deps.instruments.dcGroup;
        manual.relay = deps.instruments.relay;
        ManualControlViewModel manualControl(&model, manual);
        TestRunSession session(std::make_unique<TestRunService>(deps, nullptr), std::make_unique<ReportWriter>());
        bool done = false; int delivered = 0; QVector<TestSequenceResultRecord> records;
        QObject::connect(&session, &TestRunSession::outputsUncertain, &manualControl,
            [&](const InstrumentSetupConfig& config, const QVector<TableKind>& outputs, const ShortCircuitProtection& protection) {
                require(QThread::currentThread() == app.thread(), "wrong delivery thread");
                require(protection.active() && protection.shortMayBeOn && protection.dischargeMs == 1,
                        "retained short metadata lost across queued handoff");
                require(outputs.contains(TableKind::Relay), "held relay not UNKNOWN");
                ++delivered; manualControl.adoptUncertainOutputs(config, outputs, protection);
            });
        QObject::connect(&session, &TestRunSession::runningChanged, [&](bool running) {
            if (!running) {
                require(delivered == 1 && manualControl.hasUnknownOutput(), "Run unlocked before short protection handoff");
                done = true;
            }
        });
        QObject::connect(&session, &TestRunSession::resultRecordsReady, [&](const auto& value) { records = value; });
        TaskPayload relay; relay.task.name = "Relay"; relay.settings["relay_index"] = 0;
        auto shortTask = task(name); shortTask.retry = 2; shortTask.settings["discharge_ms"] = 1;
        require(session.start({relay, shortTask, relay}, c, scope, false, {}, {}), "session start failed");
        auto wait = [&](auto predicate) {
            QElapsedTimer t; t.start();
            while (!predicate() && t.elapsed() < 6000) { app.processEvents(); QThread::msleep(1); }
            require(predicate(), "worker timeout");
        };
        wait([&] { return done; });
        require(delivered == 1 && attempts == 1 && sh && !dis && dcOffCalls > 0,
                "retry/member/deferred cleanup bypassed short protection");
        require(records.size() == 3 && records[1].status == "Fail" && records[2].status == "Skipped",
                "remaining task ran after retained short");
        auto idle = [&] { return !manualControl.hasPendingControlOperation(); };
        const auto before = relayCalls;
        manualControl.handleRelay(RelayAction::RelayOff); manualControl.handleRelay(RelayAction::Change);
        manualControl.handleInput(InputAction::PowerOn); manualControl.onDcGroupToggled(true);
        wait(idle);
        require(relayCalls == before && sh && manualControl.hasUnknownOutput(), "ManualControl bypassed retained short guard");
        auto confirmOff = [&] {
            manualControl.handleInput(InputAction::PowerOff); wait(idle);
            manualControl.onDcGroupToggled(false); wait(idle);
        };
        failOff = false; confirmOff();
        failOff = true; // Recheck fails after the earlier manual OFF succeeded.
        manualControl.handleRelay(RelayAction::RelayOff); wait(idle);
        require(relayCalls == before && sh && manualControl.hasUnknownOutput(), "failed OFF recheck released short");
        failOff = false; confirmOff();
        manualControl.handleRelay(RelayAction::RelayOff); wait(idle);
        require(!ac && !dc && !sh && !dis && !manualControl.hasActiveControl() && !manualControl.hasUnknownOutput(),
                "manual OFF recovery did not discharge/release and unlock");
        std::cout << "PASS short handoff " << name.toStdString() << " " << fault.toStdString() << '\n';
    } catch (const std::exception& ex) { std::cerr << ex.what(); return 1; }
}
