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
                require(!shortTest || (exitOff == 1 && exitRelays == 3), "Capture preceded short protection exit");
                if (fault == "save") throw std::runtime_error("SAVE failed");
            };
            const auto result = runTransientStrategy(kind, ctx, &scope, stop);
            const bool shouldCapture = fault == "none" || fault == "save";
            require(!result.success && captures == int(shouldCapture), "READY failure capture count/status");
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
        if (mode == "ready") ready();
        else measurements(mode == "legal_na");
    } catch (const std::exception &e) { std::cerr << e.what() << "\n"; return 1; }
}
