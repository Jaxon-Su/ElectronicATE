#define main original_matrix_main
#include "../strategies/strategy_matrix_test.cpp"
#undef main

class CheckedNormal : public NormalScope {
public:
    QString metric, error;
    bool reference = false, communicationFailure = true;
    int faults = 0;
    QString lastError() const override { return error; }
    double measureSignalPeak(int ch, const QString &type) override {
        error.clear();
        if (type == metric && (reference || !autoMode) &&
            ((metric == "RMS" || metric == "MEAN") || ch == 2)) {
            ++faults;
            if (communicationFailure) error = "injected query timeout";
            return NAN;
        }
        return NormalScope::measureSignalPeak(ch, type);
    }
};
class CheckedTransient : public Scope {
public:
    QString error, metric;
    int faults = 0;
    bool capture = false, archived = false;
    QString lastError() const override { return error; }
    bool beginSingleAcquisition() override { archived = false; return Scope::beginSingleAcquisition(); }
    double measureSignalPeak(int ch, const QString &type) override {
        error.clear();
        if (event && ch == 2 && type == metric && (!capture || archived)) {
            ++faults;
            error = "injected query timeout";
            return NAN;
        }
        return Scope::measureSignalPeak(ch, type);
    }
};
class EarlyScope : public Scope {
public:
    bool completed = false;
    QString fault;
    QAtomicInt *stopFlag = nullptr;
    bool beginSingleAcquisition() override {
        const bool ok = Scope::beginSingleAcquisition();
        completed = ok && fault != "running";
        return ok;
    }
    ScopeAcquisitionState acquisitionState() override {
        if (fault == "query") throw std::runtime_error("acquisition query failed");
        if (fault == "invalid") return ScopeAcquisitionState::Invalid;
        return completed ? ScopeAcquisitionState::Completed : ScopeAcquisitionState::Running;
    }
    ScopeTriggerState triggerState() override {
        if (fault == "cancel") stopFlag->storeRelease(1);
        return ScopeTriggerState::Saving;
    }
};

class TurnOnNoiseScope : public Scope {
public:
    TurnOnNoiseScope() { single = true; scale = 1; totalChannels = 1; }
    bool early = false;
    int earlyRecords = 0;
    int completionQueries = 0;
    QString fault, error;
    QAtomicInt *stopFlag = nullptr;
    std::function<void()> beforeFrozenQuery;
    QString lastError() const override { return error; }
    bool beginSingleAcquisition() override {
        early = false;
        completionQueries = 0;
        return Scope::beginSingleAcquisition();
    }
    ScopeTriggerState triggerState() override {
        error.clear();
        if (fault == "auto") return ScopeTriggerState::Automatic;
        if (fault == "arming") return ScopeTriggerState::Arming;
        if (fault == "invalid") { error = "injected invalid trigger reply"; return ScopeTriggerState::Invalid; }
        if (armed && !event && (std::abs(level) < .10 || fault == "limit" ||
                                (fault == "after_event" && events == 1 && arms == 4) ||
                                (fault == "floor_rounding" && events == 1 && arms == 2))) {
            if (!early) ++earlyRecords;
            early = true;
            if (fault == "stop_ready") stopFlag->storeRelease(1);
            if (fault == "state_error") error = "injected readiness query error";
            if (fault == "save" || fault == "save_running") return ScopeTriggerState::Saving;
            return ScopeTriggerState::Triggered;
        }
        return ScopeTriggerState::Ready;
    }
    ScopeAcquisitionState acquisitionState() override {
        error.clear();
        if (!early) return Scope::acquisitionState();
        ++completionQueries;
        if (fault == "pending" && completionQueries >= 4 && beforeFrozenQuery)
            beforeFrozenQuery();
        if (fault == "query_error") { error = "injected acquisition query error"; return ScopeAcquisitionState::Invalid; }
        if (fault == "pending_error" && completionQueries > 1) {
            error = "injected completion query error"; return ScopeAcquisitionState::Invalid;
        }
        if (fault == "stop_completion") stopFlag->storeRelease(1);
        if (fault == "stop_frozen" && completionQueries == 2) stopFlag->storeRelease(1);
        if (fault == "late_completion") QThread::msleep(25);
        if (fault == "save_running" || fault == "incomplete" || fault == "pending_error" ||
            (fault == "pending" && completionQueries < 3)) return ScopeAcquisitionState::Running;
        return ScopeAcquisitionState::Completed;
    }
    double measureSignalPeak(int ch, const QString &metric) override {
        require(!early, "pre-power record used as a measurement");
        return Scope::measureSignalPeak(ch, metric);
    }
};

class SingleWaitScope : public Scope {
public:
    QElapsedTimer sinceSingle;
    QAtomicInt started{0};
    bool early = false, injectEarly = false;
    int normalArms = 0, normalArmAttempts = 0, normalQueries = 0, earlyRecords = 0;
    QVector<double> eventLevels;
    bool beginSingleAcquisition() override {
        if (!autoMode) ++normalArmAttempts;
        const bool ok = Scope::beginSingleAcquisition();
        early = false;
        if (ok && !autoMode) {
            eventLevels << level;
            ++normalArms; sinceSingle.start(); started.storeRelease(1);
            early = injectEarly && normalArms <= 2;
            if (early) ++earlyRecords;
        }
        return ok;
    }
    void checkProcessingTime() {
        if (!autoMode) {
            ++normalQueries;
            require(sinceSingle.isValid() && sinceSingle.elapsed() >= 3000,
                    "READY/Baseline queried before 3000 ms processing wait");
        }
    }
    ScopeTriggerState triggerState() override {
        checkProcessingTime();
        return early ? ScopeTriggerState::Triggered : ScopeTriggerState::Ready;
    }
    ScopeAcquisitionState acquisitionState() override {
        checkProcessingTime();
        return early ? ScopeAcquisitionState::Completed : Scope::acquisitionState();
    }
    double measureSignalPeak(int ch, const QString &metric) override {
        if (!early) return Scope::measureSignalPeak(ch, metric);
        require(!single && (metric == "MAXimum" || metric == "MINimum"), "early record became event quartet");
        return level; // Independent known Baseline floor: .04 then .08.
    }
};

static void singleWait(TransientKind kind)
{
    for (const QString mode : {"recovery", "stop", "phase", "arm", "negative", "exit_off"}) {
        if (mode == "exit_off" && kind != TransientKind::ShortThenTurnOn) continue;
        SingleWaitScope scope; scope.scale = 1; scope.totalChannels = 1; scope.steady = 0; scope.peak = .12;
        scope.single = kind != TransientKind::TurnOnThenShort; scope.injectEarly = mode == "recovery";
        scope.onArm = [&] { scope.failArm = mode == "arm" && !scope.autoMode; };
        auto ctx = context(scope); ctx.target = "MAX"; ctx.trialsPerLevel = 1; ctx.phaseTimeoutMs = 60000;
        ctx.timeoutMs = 10; ctx.singleProcessingMs = TransientContext{}.singleProcessingMs;
        require(ctx.singleProcessingMs == 3000, "runtime processing wait default changed");
        if (mode == "phase") ctx.phaseTimeoutMs = 100;
        if (mode == "negative") ctx.singleProcessingMs = -1;
        QAtomicInt stop{0};
        bool powered = false, shorted = false, discharging = false, exitFailed = false;
        int inputEvents = 0, shortEvents = 0, offFailures = 0, diagnostics = 0;
        QStringList logs;
        ctx.log = [&](const QString &line) { logs << line; };
        ctx.power = [&](bool on, QString &error) {
            scope.trace << (on ? "POWER ON" : "POWER OFF");
            if (!on && mode == "exit_off" && scope.normalArms > 0) {
                ++offFailures; exitFailed = true; error = "injected OFF failure"; return false;
            }
            if (on && scope.single) {
                require(!scope.early && scope.sinceSingle.elapsed() >= 3000, "Input ON preceded processing/READY");
                ++inputEvents; scope.event = true; ++scope.events;
            }
            powered = on; return true;
        };
        ctx.relays = [&](bool sh, bool dis, QString &) {
            require(!exitFailed, "relay released after failed Input OFF");
            scope.trace << QString("RELAY %1 %2").arg(sh).arg(dis);
            if (sh && powered) {
                require(kind == TransientKind::TurnOnThenShort && !dis && !scope.early &&
                        scope.sinceSingle.elapsed() >= 3000, "Short preceded processing/READY");
                ++shortEvents; scope.event = true; ++scope.events;
            }
            shorted = sh; discharging = dis; return true;
        };
        ctx.captureRecord = [&](const ScopeCaptureRecord &record) {
            if (!scope.early) return;
            ++diagnostics;
            require(!powered && !shorted && !discharging, "early Capture preceded safe exit");
            require(record.phase == (scope.single ? "Before READY" : "Baseline"), "diagnostic identity lost");
        };
        std::unique_ptr<QThread> cancel;
        if (mode == "stop" || mode == "exit_off") {
            cancel.reset(QThread::create([&] {
                QElapsedTimer deadline; deadline.start();
                while (!scope.started.loadAcquire() && deadline.elapsed() < 5000) QThread::msleep(1);
                QThread::msleep(100); stop.storeRelease(1);
            }));
            cancel->start();
        }
        QElapsedTimer elapsed; elapsed.start();
        const auto result = runTransientStrategy(kind, ctx, &scope, stop);
        if (cancel) cancel->wait();
        if (mode == "negative") {
            require(!result.success && result.errorMessage.contains("Invalid") && scope.trace.isEmpty(), "negative wait operated hardware");
            continue;
        }
        require(scope.trace.last() == "LOAD OFF" && scope.trace.contains("ACQuire:STATE STOP"), "wait skipped cleanup");
        if (mode == "exit_off") {
            require(!result.success && result.cleanupFailed && shorted && offFailures == 1 && diagnostics == 0,
                    "wait cancellation bypassed retained-short protection");
            continue;
        }
        require(!powered && !shorted && !discharging && !result.cleanupFailed, "wait left outputs active");
        if (mode == "recovery") {
            require(result.success, qPrintable(result.errorMessage));
            if (scope.earlyRecords != 2 || diagnostics != 2 || scope.eventLevels.mid(0, 3) != QVector<double>({.04,.08,.12})) {
                std::cerr << "single wait kind=" << int(kind) << " early=" << scope.earlyRecords << " diagnostics=" << diagnostics << " levels:";
                for (double value : scope.eventLevels) std::cerr << ' ' << value;
                std::cerr << '\n';
            }
            require(scope.earlyRecords == 2 && diagnostics == 2 && scope.eventLevels.mid(0, 3) == QVector<double>({.04,.08,.12}),
                    "wait changed pre-trigger Level recovery");
            require(result.trials.size() == 1 && scope.scale == 1, "wait changed events/Scale");
            require(logs.count("Single processing wait: 3000 ms before Baseline/READY") == scope.normalArms,
                    "wait missing per Single or incorrectly applied to AUTO reference");
            require(scope.single ? inputEvents > 0 : shortEvents > 0, "no real event after recovery");
        } else {
            require(!result.success && result.trials.isEmpty() && inputEvents == 0 && shortEvents == 0 &&
                    scope.normalQueries == 0 && elapsed.elapsed() < 2500, "wait failure queried/applied event or ignored Stop/Phase");
            if (mode == "arm") require(scope.normalArmAttempts == 1 && scope.normalArms == 0 &&
                    logs.filter("Single processing wait:").isEmpty(), "failed event arm waited or was not exercised");
            if (mode == "phase") require(result.errorMessage.contains("phase time limit"), "phase deadline reset");
            if (mode == "stop") require(result.errorMessage.contains("Stopped"), "Stop lost");
        }
        std::cout << "PASS single wait kind=" << int(kind) << " mode=" << mode.toStdString() << '\n';
    }
}

static void pretriggerCase(const QString &mode, bool shortTest = false)
{
    TurnOnNoiseScope scope;
    scope.fault = mode;
    auto ctx = context(scope);
    ctx.target = mode == "fall" ? "MIN" : mode == "both" ? "BOTH" : "MAX";
    ctx.timeoutMs = 10; ctx.trialsPerLevel = 1;
    if (mode == "limit") ctx.maxIterations = 5;
    if (mode == "late_completion" || mode == "incomplete") ctx.phaseTimeoutMs = 15;
    if (mode == "no_event") scope.never = true;
    if (mode == "floor_rounding") { scope.scale = 50; scope.peak = 1e16 - 2; }
    if (mode == "arm_failure") scope.failArm = true;
    if (mode == "write_failure") scope.onWrite = [&](const QString &cmd) {
        if (scope.arms == 1 && cmd.startsWith("TRIGger:A:LEVel")) scope.failWrite = cmd;
    };
    QAtomicInt stop(0);
    scope.stopFlag = &stop;
    QStringList logs;
    ctx.log = [&](const QString &line) { logs << line; };
    bool powered = false, shorted = shortTest, discharging = false;
    bool exitFailed = false;
    int offFailures = 0, relayAfterFailure = 0;
    if (shortTest) {
        scope.beforeFrozenQuery = [&] {
            require(!powered && !shorted && !discharging,
                    "pending pre-input record queried again before short exit");
        };
        ctx.relays = [&](bool sh, bool dis, QString &error) {
            if (exitFailed) ++relayAfterFailure;
            require(!exitFailed, "relay command after retained-short failure");
            require(!powered, "pre-input short relay changed under power");
            scope.trace << QString("RELAY %1 %2").arg(sh).arg(dis);
            if (scope.early && ((mode == "exit_discharge_on" && dis) ||
                               (mode == "exit_discharge_off" && sh && !dis && discharging) ||
                               (mode == "exit_short_off" && !sh))) {
                exitFailed = true; error = "injected pre-input exit failure"; return false;
            }
            shorted = sh; discharging = dis;
            if (scope.early && mode == "exit_stop_discharge" && dis) stop.storeRelease(1);
            return true;
        };
        scope.onRead = [&] { require(!powered && !shorted && !discharging,
                                    "short event read before safe exit"); };
    }
    int diagnostics = 0;
    if (mode != "rise" && mode != "fall" && mode != "no_event")
        ctx.captureRecord = [&](const ScopeCaptureRecord &record) {
            require(!shortTest || (!powered && !shorted && !discharging),
                    "short Capture preceded safe exit");
            if (record.phase != "Before READY") return;
            ++diagnostics;
            require(scope.early && scope.acquisitionState() == ScopeAcquisitionState::Completed,
                    "diagnosed an incomplete pre-input record");
            for (const auto &ch : record.channels)
                require(std::isnan(ch.maxVal) && std::isnan(ch.minVal) && std::isnan(ch.rms) && std::isnan(ch.mean),
                        "diagnostic record reused event values");
            if (mode == "capture_failure") throw std::runtime_error("injected Capture failure");
            if (mode == "stop_capture") stop.storeRelease(1);
        };
    const auto power = ctx.power;
    ctx.power = [&](bool on, QString &error) {
        require(!on || !scope.early, "input energized after an early trigger");
        require(!shortTest || !on || (shorted && !discharging), "short-on event prepared incorrectly");
        if (shortTest && !on && scope.early && mode.startsWith("exit_off")) {
            ++offFailures; exitFailed = true;
            if (mode == "exit_off_throw") throw std::runtime_error("injected pre-input OFF exception");
            error = "injected pre-input OFF failure"; return false;
        }
        powered = on;
        return power(on, error);
    };
    const auto result = runTransientStrategy(shortTest ? TransientKind::ShortThenTurnOn : TransientKind::TurnOn,
                                             ctx, &scope, stop);
    if (exitFailed) {
        require(!result.success && result.cleanupFailed && shorted && diagnostics == 0 &&
                scope.arms == 1 && relayAfterFailure == 0 &&
                (!mode.startsWith("exit_off") || offFailures == 1),
                "pre-input short-exit failure bypassed retention or automatically retried");
        require(!scope.trace.contains("POWER ON") && scope.trace.last() == "LOAD OFF" &&
                scope.trace.contains("ACQuire:STATE STOP"), "retained short skipped scope/load cleanup or enabled input");
        return;
    }
    require(!shortTest || (!powered && !shorted && !discharging), "pre-input recovery left short active");
    require(scope.trace.last() == "LOAD OFF" && scope.trace.contains("ACQuire:STATE STOP"),
            "pre-input path missed cleanup");
    const bool success = QStringList{"rise", "fall", "both", "save", "pending", "after_event"}.contains(mode);
    if (result.success != success) std::cerr << result.errorMessage.toStdString() << '\n';
    require(result.success == success, "wrong pre-input result status");
    if (!success) {
        if (mode == "floor_rounding") {
            require(result.trials.size() == 1 && scope.levels.size() == 3 &&
                    result.errorMessage.contains("threshold cannot advance"),
                    "inward rounding returned to an excluded pre-input floor");
            return;
        }
        require(result.trials.isEmpty(), "failed pre-input run fabricated event data");
        if (mode != "no_event") require(!scope.trace.contains("POWER ON"), "failed pre-input path energized input");
        if (mode == "limit") require(scope.arms == 5 && diagnostics == 5 && result.errorMessage.contains("iteration limit"),
                                     "pre-input retries escaped acquisition limit");
        else if (mode == "capture_failure") require(diagnostics == 1 && scope.arms == 1 &&
            result.errorMessage.contains("Scope not ready") && result.errorMessage.contains("injected Capture failure"),
            "Capture failure rearmed or lost original cause");
        else if (mode == "pending_error") require(scope.arms == 1 && diagnostics == 0 &&
            result.errorMessage.contains("injected completion query error"), "completion communication error lost");
        else if (mode == "late_completion") require(scope.arms == 1 && diagnostics == 1 &&
            result.errorMessage.contains("time limit"), "late completion lost diagnostic or restarted phase");
        else if (mode.startsWith("stop_")) require(scope.arms == 1 &&
            diagnostics == int(mode == "stop_capture"), "Stop rearmed or saved unwanted data");
        else if (mode == "exit_stop_discharge") require(scope.arms == 1 && diagnostics == 0,
                                                       "Stop during short discharge rearmed/captured");
        else if (mode == "arming") require(scope.arms == 1 && result.errorMessage.contains("readiness timeout"),
                                            "ARMED timeout was treated as a pre-trigger");
        else if (mode == "auto" || mode == "invalid") require(scope.arms == 1 && result.errorMessage.contains("state="),
                                                               "rejected state missing/retried");
        else if (mode == "state_error" || mode == "query_error") require(scope.arms == 1 &&
            result.errorMessage.contains("injected"), "communication error retried or discarded");
        return;
    }
    require(result.success, "Turn on must escape pre-power triggers and acquire the real event");
    require(scope.earlyRecords == (mode == "both" ? 4 : mode == "after_event" ? 3 : 2) && scope.levels.size() >= 3,
            "wrong number of pre-power records");
    const double sign = mode == "fall" ? -1 : 1;
    require(std::abs(scope.levels[0] - sign * .04) < 1e-12 &&
            std::abs(scope.levels[1] - sign * .08) < 1e-12 &&
            std::abs(scope.levels[2] - sign * .12) < 1e-12, "Level did not advance by one 0.04 V step");
    const auto beforeInput = scope.trace.mid(0, scope.trace.indexOf("POWER ON"));
    require(beforeInput.count(shortTest ? "RELAY 1 1" : "RELAY 0 1") == (shortTest ? 3 : 1) &&
            beforeInput.count("ARM") == 3, "pre-power rearm changed discharge/short-exit sequence");
    require(!result.trials.isEmpty() && result.trials.first().sequence == 1 && scope.scale == 1,
            "early record consumed an event or changed Scale");
    for (int i = 0; i < result.trials.size(); ++i) {
        const auto &trial = result.trials[i];
        require(trial.sequence == i + 1 && std::abs(trial.triggerLevel) > .08,
                "pre-input floor lost or diagnostic became a Trial");
        require(trial.channels[0].maximum == 7 && trial.channels[0].minimum == -3 &&
                trial.channels[0].rms == 2 && trial.channels[0].mean == 1,
                "real event quartet lost");
    }
    require(!logs.filter("CH1 pre-input trigger: Level").isEmpty(), "missing pre-input Level diagnosis");
    if (ctx.captureRecord) require(diagnostics == scope.earlyRecords, "pre-input diagnostic missing");
}

static void pretrigger(bool shortTest = false)
{
    QStringList modes{"rise", "fall", "both", "save", "pending", "after_event", "no_event", "limit",
        "auto", "invalid", "arming", "save_running", "query_error", "pending_error", "state_error",
        "arm_failure", "write_failure", "capture_failure", "late_completion", "incomplete",
        "stop_ready", "stop_completion", "stop_frozen", "stop_capture", "floor_rounding"};
    if (shortTest) modes << "exit_off" << "exit_off_throw" << "exit_discharge_on" << "exit_discharge_off"
                        << "exit_short_off" << "exit_stop_discharge";
    for (const auto &mode : modes) {
        try { pretriggerCase(mode, shortTest); }
        catch (...) { std::cerr << "pretrigger " << mode.toStdString() << ": "; throw; }
    }
    std::cout << "PASS " << (shortTest ? "Short then turn on" : "Turn on") << " pretrigger cases=" << modes.size() << '\n';
}

class TurnOffStartupScope : public Scope {
public:
    bool running = false;
    QString fault;
    QAtomicInt *stopFlag = nullptr;
    int stops = 0, normals = 0;

    bool writeConfigurationCommand(const QString &command, QString &error) override {
        const bool stopping = command == "ACQuire:STATE STOP";
        const bool normal = command == "TRIGger:A:MODe NORMAL";
        if (stopping) ++stops;
        if (normal) ++normals;
        const bool initial = (stopping && stops == 1) || (normal && normals == 1);
        const QString stage = stopping ? "stop" : "normal";
        if (initial && (fault == stage + "_failure" || fault == stage + "_throw" ||
                        (normal && fault == "cleanup_failure"))) {
            trace << command;
            error = "injected startup " + stage + " failure";
            if (fault.endsWith("_throw")) throw std::runtime_error("injected startup exception");
            return false;
        }
        const bool ok = Scope::writeConfigurationCommand(command, error);
        if (ok && stopping) running = false;
        if (initial && fault == "cancel_" + stage) stopFlag->storeRelease(1);
        return ok;
    }
    bool beginSingleAcquisition() override {
        running = true;
        return Scope::beginSingleAcquisition();
    }
    QString getAcquisitionState() override {
        const auto state = Scope::getAcquisitionState();
        if (state == "0") running = false;
        return state;
    }
};

static void turnOffStartup()
{
    int cases = 0;
    for (bool running : {false, true}) for (bool automatic : {false, true})
        for (bool capture : {false, true}) {
            TurnOffStartupScope scope;
            scope.running = running; scope.armed = running; scope.autoMode = automatic;
            QAtomicInt stop(0); scope.stopFlag = &stop;
            auto ctx = context(scope); ctx.trialsPerLevel = 1;
            int captures = 0;
            if (capture) ctx.captureRecord = [&](const ScopeCaptureRecord &) {
                require(!scope.running, "Turn off Capture used a running record"); ++captures;
            };
            for (int attempt = 0; attempt < 2; ++attempt) {
                scope.trace.clear();
                const auto result = runTransientStrategy(TransientKind::TurnOff, ctx, &scope, stop);
                require(result.success && !result.cleanupFailed, qPrintable(result.errorMessage));
                require(scope.trace.mid(0, 3) == QStringList({"ACQuire:STATE STOP", "TRIGger:A:MODe NORMAL", "POWER OFF"}),
                        "Turn off touched input/load before STOP/NORMAL");
                require(!scope.running && !scope.autoMode, "Turn off did not finish STOP/NORMAL");
                require(!result.trials.isEmpty(), "startup lost actual event results");
                ++cases;
            }
            require(capture ? captures > 0 : captures == 0, "startup changed Capture selection");
        }
    for (const QString fault : {"stop_failure", "normal_failure", "stop_throw", "normal_throw",
                               "cancel_before", "cancel_stop", "cancel_normal", "cleanup_failure"}) {
        TurnOffStartupScope scope; scope.running = scope.armed = scope.autoMode = true;
        scope.fault = fault;
        QAtomicInt stop(fault == "cancel_before" ? 1 : 0); scope.stopFlag = &stop;
        auto ctx = context(scope);
        QStringList preparationLogs;
        ctx.log = [&](const QString &message) {
            if (message.startsWith("Scope preparation:")) preparationLogs << message;
        };
        const auto power = ctx.power;
        ctx.power = [&](bool on, QString &error) {
            const bool ok = power(on, error);
            if (fault == "cleanup_failure" && !on) { error = "injected cleanup input failure"; return false; }
            return ok;
        };
        int captures = 0;
        ctx.captureRecord = [&](const ScopeCaptureRecord &) { ++captures; };
        const auto result = runTransientStrategy(TransientKind::TurnOff, ctx, &scope, stop);
        require(!result.success && captures == 0 && scope.arms == 0 && scope.reads == 0,
                "failed startup continued acquisition/read/Capture");
        require(!scope.trace.contains("LOAD ON") && !scope.trace.contains("POWER ON") &&
                    !scope.trace.contains("TRIGger:A:MODe AUTO"), "failed startup enabled hardware");
        if (fault == "cancel_before") require(scope.trace.isEmpty(), "pre-cancelled startup touched hardware");
        else require(scope.trace.contains("ACQuire:STATE STOP") && scope.trace.contains("POWER OFF") &&
                         scope.trace.contains("LOAD OFF"), "startup failure skipped cleanup");
        if (fault.startsWith("cancel")) require(result.errorMessage.contains("Stopped by user"), "startup lost Stop");
        else require(result.errorMessage.contains("injected startup"), "startup lost original error");
        if (fault.endsWith("_throw"))
            require(preparationLogs.value(preparationLogs.size() - 1) ==
                        (fault == "stop_throw" ? "Scope preparation: STOP" : "Scope preparation: NORMAL"),
                    "startup exception lost failing preparation stage");
        if (fault.startsWith("stop_") || fault == "cancel_stop")
            require(scope.normals == 0, "failed/cancelled STOP still configured NORMAL");
        if (fault == "cleanup_failure") require(result.cleanupFailed && result.errorMessage.contains("injected cleanup input failure"),
                                                 "startup lost cleanup failure");
        ++cases;
    }
    std::cout << "PASS Turn off startup cases=" << cases << '\n';
}

static void measurements(bool legalNa)
{
    QAtomicInt stop(0);
    int cases = 0;
    for (bool capture : {false, true}) {
        for (bool reference : {false, true}) for (const QString metric : {"MAXimum", "MINimum", "RMS", "MEAN"}) {
            if (legalNa && (reference || capture)) continue; // Legal secondary NA in normal search, plus RMS/Mean.
            CheckedNormal scope; scope.metric = metric; scope.reference = reference;
            scope.communicationFailure = !legalNa;
            int archives = 0;
            SteadyStateSearchStrategy strategy(2, 1000, 20);
            if (capture) strategy.setCaptureObserver([&](const auto &) { ++archives; });
            const auto result = strategy.execute(&scope, stop);
            require(scope.faults > 0 && result.success == legalNa, "steady failure/NA classification");
            if (!legalNa) require(result.errorMessage.contains("injected query timeout"), "steady original error lost");
            if (capture) require(archives > 0, "measurement failure lost completed diagnostic record");
            require(scope.trace.contains("ACQuire:STATE STOP"), "steady failure skipped STOP");
            ++cases;
        }
        if (legalNa) continue;
        for (auto kind : {TransientKind::TurnOn, TransientKind::TurnOff,
                          TransientKind::TurnOnThenShort, TransientKind::ShortThenTurnOn}) {
            for (const QString metric : {"MAXimum", "MINimum"}) {
                CheckedTransient scope; scope.metric = metric; scope.capture = capture; scope.forbidRmsMean = true;
                scope.allowReferenceQuartet = kind == TransientKind::TurnOff || kind == TransientKind::TurnOnThenShort;
                scope.single = kind == TransientKind::TurnOn || kind == TransientKind::ShortThenTurnOn;
                auto ctx = context(scope);
                if (capture) ctx.captureRecord = [&](const auto &) { scope.archived = true; };
                const auto result = runTransientStrategy(kind, ctx, &scope, stop);
                require(scope.faults > 0 && !result.success && result.errorMessage.contains("injected query timeout"),
                        "transient communication error passed/lost");
                require(scope.trace.contains("LOAD OFF") && scope.trace.contains("ACQuire:STATE STOP"),
                        "transient failure skipped cleanup");
                ++cases;
            }
        }
    }
    std::cout << "PASS measurement/NA cases=" << cases << "\n";
}

class OnShortNoiseScope : public Scope {
public:
    QString mode, error;
    bool checkingShort = false, early = false;
    int earlyRecords = 0, completionQueries = 0;
    QAtomicInt *stopFlag = nullptr;
    QString lastError() const override { return error; }
    bool beginSingleAcquisition() override {
        early = checkingShort = false; completionQueries = 0;
        const bool ok = Scope::beginSingleAcquisition();
        if (!autoMode && mode == "baseline" && earlyRecords == 0) { early = true; ++earlyRecords; }
        return ok;
    }
    ScopeTriggerState triggerState() override {
        error.clear();
        if (checkingShort && !event && (earlyRecords == 0 || mode == "limit") &&
            (mode != "after_event" || events > 0)) {
            early = true; ++earlyRecords;
            if (mode == "auto") return ScopeTriggerState::Automatic;
            if (mode == "invalid") { error = "injected trigger error"; return ScopeTriggerState::Invalid; }
            if (mode == "stop_ready") stopFlag->storeRelease(1);
            if (mode == "after_ready") return ScopeTriggerState::Ready;
            return mode == "save" || mode == "save_running" ? ScopeTriggerState::Saving : ScopeTriggerState::Triggered;
        }
        return ScopeTriggerState::Ready;
    }
    ScopeAcquisitionState acquisitionState() override {
        error.clear();
        if (!early) return Scope::acquisitionState();
        ++completionQueries;
        if (mode == "query_error") { error = "injected acquisition error"; return ScopeAcquisitionState::Invalid; }
        if (mode == "stop_completion") stopFlag->storeRelease(1);
        if (mode == "late") QThread::msleep(60);
        if (mode == "save_running" || mode == "incomplete" ||
            (mode == "pending" && completionQueries < 3)) return ScopeAcquisitionState::Running;
        return ScopeAcquisitionState::Completed;
    }
    double measureSignalPeak(int ch, const QString &metric) override {
        if (!early) return Scope::measureSignalPeak(ch, metric);
        require(metric != "RMS" && metric != "MEAN", "pre-short record read event quartet");
        if (mode == "measurement_error") { error = "injected baseline measurement error"; return NAN; }
        if (mode == "na") return NAN;
        return metric == "MAXimum" ? 2 : -2;
    }
};

static void onShortPretriggerCase(const QString &mode, bool capture)
{
    OnShortNoiseScope scope; scope.mode = mode; scope.totalChannels = 1;
    QAtomicInt stop(0); scope.stopFlag = &stop;
    auto ctx = context(scope); ctx.target = mode == "fall" ? "MIN" : "MAX";
    ctx.timeoutMs = 3; ctx.trialsPerLevel = 1;
    if (mode == "limit") ctx.maxIterations = 5;
    if (mode == "late" || mode == "incomplete") ctx.phaseTimeoutMs = 50;
    QStringList logs;
    ctx.log = [&](const QString &line) {
        logs << line;
        if (line == "Apply short; wait for event" || line == "Check scope before short") scope.checkingShort = true;
    };
    bool powered = false, shorted = true, discharging = false, exitFailed = false;
    int shortEvents = 0, diagnostics = 0, offFailures = 0;
    ctx.power = [&](bool on, QString &error) {
        scope.trace << (on ? "POWER ON" : "POWER OFF");
        if (!on && scope.early && mode == "exit_off") {
            ++offFailures; exitFailed = true; error = "injected input OFF failure"; return false;
        }
        powered = on; return true;
    };
    ctx.relays = [&](bool sh, bool dis, QString &error) {
        require(!exitFailed, "relay changed after failed short exit");
        scope.trace << QString("RELAY %1 %2").arg(sh).arg(dis);
        if (scope.early && mode == "exit_discharge" && dis) {
            exitFailed = true; error = "injected discharge failure"; return false;
        }
        if (sh && powered) {
            require(!scope.early, "short applied after a pre-short trigger");
            require(!dis, "discharge applied under power");
            ++shortEvents; scope.event = true; ++scope.events;
        }
        shorted = sh; discharging = dis; return true;
    };
    if (capture) ctx.captureRecord = [&](const ScopeCaptureRecord &record) {
        if (!scope.early) return;
        require(!powered && !shorted && !discharging, "pre-short Capture preceded safe exit");
        ++diagnostics;
        if (mode == "capture_failure") throw std::runtime_error("injected Capture failure");
        if (mode == "stop_capture") stop.storeRelease(1);
        require(record.phase == "Baseline", "pre-short record lost baseline identity");
    };
    const auto result = runTransientStrategy(TransientKind::TurnOnThenShort, ctx, &scope, stop);
    const bool success = QStringList{"rise", "fall", "baseline", "save", "pending", "after_ready", "after_event"}.contains(mode);
    if (result.success != success) std::cerr << mode.toStdString() << ": " << result.errorMessage.toStdString() << '\n';
    require(result.success == success, "wrong pre-short result status");
    require(scope.trace.last() == "LOAD OFF" && scope.trace.contains("ACQuire:STATE STOP"), "pre-short missed cleanup");
    if (exitFailed) {
        require(result.cleanupFailed && diagnostics == 0 && scope.arms == 2 &&
                    (mode != "exit_off" || offFailures == 1), "pre-short cleanup failure retried/bypassed");
        return;
    }
    require(!powered && !shorted && !discharging, "pre-short left outputs active");
    if (!success) {
        require(result.trials.isEmpty() && result.steadyReferences.isEmpty() && shortEvents == 0,
                "failed pre-short fabricated event or successful fallback");
        if (mode == "limit") require(scope.arms == 5 && result.errorMessage.contains("iteration limit"), "unbounded pre-short retry");
        else require(scope.arms == 2, "failed pre-short rearmed");
        if (mode == "late") require(diagnostics == int(capture) && result.errorMessage.contains("time limit"), "late record lost diagnosis/deadline");
        if (mode == "measurement_error" || mode == "query_error" || mode == "invalid" || mode == "capture_failure")
            require(result.errorMessage.contains("injected"), "original pre-short failure lost");
        return;
    }
    require(scope.earlyRecords == 1 && !result.trials.isEmpty() && shortEvents > 0, "pre-short not recovered");
    require(diagnostics == int(capture), "missing or duplicate baseline diagnostic");
    require(!logs.filter("Baseline trigger excluded").isEmpty(), "missing baseline exclusion log");
    for (int i = 0; i < result.trials.size(); ++i) {
        const auto &trial = result.trials[i];
        require(trial.sequence == i + 1 && trial.channels[0].maximum == 7 && trial.channels[0].minimum == -3 &&
                    trial.channels[0].rms == 2 && trial.channels[0].mean == 1, "baseline contaminated event quartet");
        if (mode != "after_event") require(mode == "fall" ? trial.triggerLevel < -2 : trial.triggerLevel > 2,
                                           "search revisited excluded baseline");
    }
    if (mode == "after_event") require(result.trials.first().triggerLevel == 1.4, "previous real event lost");
}

static void onShortPretrigger()
{
    int cases = 0;
    for (bool capture : {false, true})
        for (const QString mode : {"rise", "fall", "baseline", "save", "pending", "after_ready", "after_event",
                                   "auto", "invalid", "save_running", "query_error", "measurement_error", "na",
                                   "stop_ready", "stop_completion", "late", "incomplete", "limit", "exit_off",
                                   "exit_discharge", "capture_failure", "stop_capture"}) {
            if (!capture && (mode == "capture_failure" || mode == "stop_capture")) continue;
            onShortPretriggerCase(mode, capture); ++cases;
        }
    std::cout << "PASS pre-short cases=" << cases << '\n';
}

class OnShortRescaleScope : public OnShortNoiseScope {
public:
    int adjustedChannel = 2, changes = 0;
    int repeats = 1;
    QString fault;
    bool margin = false, changedRecord = false;
    double scales[3]{0, 1, 1};
    QVector<double> earlyLevels;
    bool beginSingleAcquisition() override {
        changedRecord = false;
        return OnShortNoiseScope::beginSingleAcquisition();
    }
    ScopeTriggerState triggerState() override {
        if (earlyRecords >= repeats) return ScopeTriggerState::Ready;
        const int before = earlyRecords;
        const auto state = OnShortNoiseScope::triggerState();
        if (earlyRecords != before) earlyLevels << level;
        return state;
    }
    bool isClipping(int ch) override { return early && !margin && ch == adjustedChannel; }
    double getChannelScale(int ch) override { return scales[ch]; }
    bool setVerticalScale(int ch, double value, QString &e) override {
        require(early && ch == adjustedChannel, "unexpected scale change outside pre-short record");
        if (fault == "scale_error") { e = "injected scale error"; return false; }
        scales[ch] = fault == "roundup" ? value * 1.25 : value;
        changedRecord = true; ++changes;
        if (fault == "stop_scale") stopFlag->storeRelease(1);
        return true;
    }
    bool readVerticalRange(int ch, ScopeVerticalRange &range, QString&) override {
        // Normal event values fit; only the selected pre-short record requests margin.
        range = {scales[ch], 0, early ? 10.0 : 1000.0}; return true;
    }
    double measureSignalPeak(int ch, const QString &metric) override {
        if (!early) return Scope::measureSignalPeak(ch, metric);
        require(!changedRecord, "baseline extremum read after changing its scale");
        require(metric != "RMS" && metric != "MEAN", "rescale baseline became an event quartet");
        if (ch == 1) {
            if (fault == "stop_read") stopFlag->storeRelease(1);
            if (fault == "read_error") { error = "injected extremum error"; return NAN; }
            if (fault == "na") return NAN;
            if (fault == "deadline") QThread::msleep(60);
        }
        const double peak = ch == adjustedChannel ? (margin ? 4.8 : 1000.0) : earlyRecords > 1 ? 1.5 : 2.0;
        return metric == "MAXimum" ? peak : -peak;
    }
};

static void onShortRescaleCase(bool falling, bool capture, int adjustedChannel, bool margin,
                               bool priorEvent = false, const QString &fault = {})
{
    OnShortRescaleScope scope; scope.totalChannels = 2; scope.adjustedChannel = adjustedChannel;
    scope.margin = margin; scope.mode = priorEvent ? "after_event" : "rise";
    scope.fault = fault;
    if (fault == "repeat" || fault == "limit") { scope.mode = "limit"; scope.repeats = fault == "repeat" ? 2 : 100; }
    QAtomicInt stop(0); scope.stopFlag = &stop;
    auto ctx = context(scope); ctx.target = falling ? "MIN" : "MAX";
    ctx.timeoutMs = 3; ctx.trialsPerLevel = 1; ctx.verticalScale.margin = margin;
    if (fault == "na" || fault == "deadline") ctx.phaseTimeoutMs = 50;
    if (fault == "limit") ctx.maxIterations = 4;
    bool powered = false, shorted = false, discharging = false;
    int diagnostics = 0;
    ctx.log = [&](const QString &line) { if (line == "Check scope before short") scope.checkingShort = true; };
    ctx.power = [&](bool on, QString&) { powered = on; return true; };
    ctx.relays = [&](bool sh, bool dis, QString&) {
        if (sh && powered) {
            require(!scope.early, "rescale applied short to a pre-short record");
            scope.event = true; ++scope.events;
        }
        shorted = sh; discharging = dis; return true;
    };
    if (capture) ctx.captureRecord = [&](const ScopeCaptureRecord &record) {
        if (!scope.early) return;
        require(!powered && !shorted && !discharging && !scope.changedRecord,
                "rescale Capture preceded safe exit or followed scale mutation");
        require(record.phase == "Baseline", "rescale lost diagnostic identity");
        ++diagnostics;
        if (fault == "capture_error") throw std::runtime_error("injected Capture error");
        if (fault == "stop_capture") stop.storeRelease(1);
    };
    const auto result = runTransientStrategy(TransientKind::TurnOnThenShort, ctx, &scope, stop);
    require(!powered && !shorted && !discharging && scope.trace.last() == "LOAD OFF", "rescale missed cleanup");
    const bool success = fault.isEmpty() || fault == "repeat" || fault == "roundup";
    if (result.success != success) std::cerr << fault.toStdString() << ": " << result.errorMessage.toStdString() << '\n';
    require(result.success == success, "rescale recovery status incorrect");
    const int expectedRecords = fault == "limit" ? 3 : success ? scope.repeats : 1;
    require(scope.earlyLevels.size() == expectedRecords, "rescale lost/repeated pre-short acquisition");
    require(diagnostics == (fault == "stop_read" ? 0 : int(capture) * expectedRecords),
            "rescale diagnostic missing/duplicated");
    if (!success) {
        require(result.trials.isEmpty() && result.steadyReferences.isEmpty() && scope.events == 0,
                "failed rescale fabricated an event/reference");
        require(scope.arms == (fault == "limit" ? 4 : 2), "failed rescale rearmed or reset iteration budget");
        if (fault.endsWith("error")) require(result.errorMessage.contains("injected"), "rescale lost original error");
        if (fault == "limit") require(result.errorMessage.contains("iteration limit"), "rescale bypassed iteration limit");
        if (fault == "deadline" || (fault == "na" && capture))
            require(result.errorMessage.contains("time limit"), "rescale bypassed phase deadline");
        if (fault == "na" && !capture) require(result.errorMessage.contains("measurement: NA"), "invalid extremum accepted");
        if (fault.startsWith("stop")) require(result.errorMessage.contains("Stopped"), "rescale lost Stop");
        return;
    }
    require(scope.changes == scope.repeats, "rescale adjustment missing/duplicated");
    const double sign = falling ? -1.0 : 1.0;
    const double oldLevel = sign * scope.earlyLevels.first();
    const double floor = std::max(oldLevel, adjustedChannel == 1 && !margin ? oldLevel : margin && adjustedChannel == 1 ? 4.8 : 2.0);
    const int firstRetry = priorEvent ? 3 : 2; // AUTO, optional real event, pre-short, retry.
    require(scope.levels.size() > firstRetry, "rescale did not acquire again");
    require(sign * scope.levels[firstRetry] > floor, "rescale revisited excluded pre-short level/extremum");
    const double expectedStep = adjustedChannel == 1 ? (margin ? .048 : .08) * (fault == "roundup" ? 1.25 : 1) : .04;
    require(std::abs(sign * scope.levels[firstRetry] - floor - expectedStep) < 1e-9,
            "rescale did not use updated trigger scale step");
    for (int i = firstRetry; i < scope.levels.size(); ++i)
        require(sign * scope.levels[i] > floor, "bracket searched inward across excluded floor");
    if (fault == "repeat") {
        require(sign * scope.earlyLevels[1] > floor, "second pre-short record lost first floor");
        for (int i = firstRetry + 1; i < scope.levels.size(); ++i)
            require(sign * scope.levels[i] > sign * scope.earlyLevels[1], "repeated rescale lost monotonic floor");
    }
    for (const auto &trial : result.trials) {
        require(trial.channels[0].maximum == 7 && trial.channels[0].minimum == -3 &&
                trial.channels[0].rms == 2 && trial.channels[0].mean == 1, "rescale contaminated event quartet");
    }
    if (priorEvent) require(!result.trials.isEmpty() && result.trials.first().triggerLevel == scope.levels[1],
                            "rescale discarded previous real event");
}

static void onShortRescale()
{
    int cases = 0;
    for (bool falling : {false, true})
        for (bool capture : {false, true})
            for (int channel : {1, 2})
                for (bool margin : {false, true})
                    for (bool prior : {false, true}) {
                        onShortRescaleCase(falling, capture, channel, margin, prior); ++cases;
                    }
    for (bool falling : {false, true})
        for (bool capture : {false, true}) {
            for (const QString fault : {"repeat", "limit", "scale_error", "stop_scale", "read_error", "stop_read",
                                        "na", "deadline", "capture_error", "stop_capture"}) {
                if (!capture && (fault == "capture_error" || fault == "stop_capture")) continue;
                onShortRescaleCase(falling, capture, 2, false, false, fault); ++cases;
            }
            onShortRescaleCase(falling, capture, 1, false, false, "roundup"); ++cases;
        }
    std::cout << "PASS pre-short rescale cases=" << cases << '\n';
}

static void ready()
{
    int cases = 0;
    for (auto kind : {TransientKind::TurnOn, TransientKind::ShortThenTurnOn}) {
        const bool shortTest = kind == TransientKind::ShortThenTurnOn;
        for (const QString fault : {"none", "save", "running", "invalid", "query", "cancel", "off", "discharge"}) {
            if (!shortTest && (fault == "off" || fault == "discharge")) continue;
            EarlyScope scope; scope.single = true; scope.fault = fault; scope.forbidRmsMean = true;
            QAtomicInt stop(0); scope.stopFlag = &stop;
            auto ctx = context(scope);
            // Power-on events may recover a naturally completed SAVE record; persistent
            // pre-input records still fail at the existing acquisition limit.
            ctx.maxIterations = 3;
            int captures = 0, exitOff = 0, exitRelays = 0;
            auto power = ctx.power; auto relays = ctx.relays;
            ctx.power = [&](bool on, QString &e) {
                if (scope.completed && !on) {
                    ++exitOff;
                    if (fault == "off") { e = "input OFF rejected"; return false; }
                }
                return power(on, e);
            };
            ctx.relays = [&](bool sh, bool dis, QString &e) {
                if (scope.completed) {
                    ++exitRelays;
                    if (fault == "off") throw std::runtime_error("relay after failed Input OFF");
                    if (fault == "discharge" && dis) { e = "discharge rejected"; return false; }
                }
                return relays(sh, dis, e);
            };
            ctx.captureRecord = [&](const ScopeCaptureRecord &record) {
                ++captures;
                require(record.phase == "Before READY" && record.channels.size() == 2, "diagnostic identity lost");
                for (const auto &ch : record.channels)
                    require(std::isnan(ch.maxVal) && std::isnan(ch.rms), "diagnostic reused previous values");
                require(!shortTest || (exitOff == captures && exitRelays == 4 * captures - 1),
                        "Capture preceded short protection exit");
                if (fault == "save") throw std::runtime_error("SAVE failed");
            };
            const auto result = runTransientStrategy(kind, ctx, &scope, stop);
            const bool shouldCapture = fault == "none" || fault == "save";
            const int expectedCaptures = fault == "none" ? 3 : int(shouldCapture);
            require(!result.success && captures == expectedCaptures, "READY failure capture count/status");
            require(!scope.trace.contains("POWER ON") && scope.trace.contains("LOAD OFF"), "READY failure enabled input/skipped cleanup");
            if (fault == "save") require(result.errorMessage.contains("Scope not ready") && result.errorMessage.contains("SAVE failed"), "dual error lost");
            if (fault == "off" || fault == "discharge") require(result.cleanupFailed && captures == 0, "short protection bypassed");
            if (fault == "off") require(exitOff == 1 && exitRelays == 0, "failed short exit automatically retried");
            ++cases;
        }
    }
    std::cout << "PASS READY/short safety cases=" << cases << "\n";
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        const auto mode = app.arguments().value(1);
        if (mode == "single_wait") {
            for (auto kind : {TransientKind::TurnOn, TransientKind::ShortThenTurnOn, TransientKind::TurnOnThenShort}) singleWait(kind);
        }
        else if (mode == "single_wait_short") singleWait(TransientKind::ShortThenTurnOn);
        else if (mode == "single_wait_onshort") singleWait(TransientKind::TurnOnThenShort);
        else if (mode == "onshort_rescale") onShortRescale();
        else if (mode == "onshort_rescale_red") onShortRescaleCase(false, false, 2, false);
        else if (mode.startsWith("onshort_pretrigger:")) onShortPretriggerCase(mode.mid(19), true);
        else if (mode == "onshort_pretrigger") onShortPretrigger();
        else if (mode.startsWith("short_pretrigger:")) pretriggerCase(mode.mid(17), true);
        else if (mode == "short_pretrigger") pretrigger(true);
        else if (mode.startsWith("pretrigger:")) pretriggerCase(mode.mid(11));
        else if (mode == "pretrigger") pretrigger();
        else if (mode == "turnoff_startup") turnOffStartup();
        else if (mode == "ready") { ready(); pretrigger(); pretrigger(true); turnOffStartup(); onShortPretrigger(); onShortRescale(); }
        else measurements(mode == "legal_na");
    } catch (const std::exception &e) { std::cerr << e.what() << "\n"; return 1; }
}
