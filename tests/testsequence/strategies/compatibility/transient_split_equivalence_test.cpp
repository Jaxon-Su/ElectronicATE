#include "../../../support/transient_test_support.h"
#include "osccaptureobserver.h"

OscMeasureResult runLegacyTransientStrategy(TransientKind, const TransientContext &,
                                           IScopeMeasurement *, QAtomicInt &);

// Compare observable device commands, measurement reads, Capture and logs. Poll
// counts are intentionally excluded: they depend on OS scheduling, not the flow.
class TraceScope : public Scope {
public:
    bool forceBaseline = false;
    QString getAcquisitionState() override
    {
        if (forceBaseline && !single && !event && !autoMode && !negative && level <= 2)
            return "0";
        return Scope::getAcquisitionState();
    }
    double measureSignalPeak(int ch, const QString &type) override
    {
        // New contracts add event and steady-reference RMS/Mean reads. Keep every pre-existing
        // read/command/Capture in this historical flow comparison.
        if (type != "RMS" && type != "MEAN") trace << QString("READ %1 %2").arg(ch).arg(type);
        if (forceBaseline && !event && !autoMode)
            return type == "MAXimum" ? 2 : -.5;
        return Scope::measureSignalPeak(ch, type);
    }
};

struct Observation {
    OscMeasureResult result;
    QStringList trace;
    QStringList logs;
    QVector<ScopeCaptureRecord> captures;
    QVector<double> levels;
    int arms;
    int events;
};

Observation observe(TransientKind kind, const QString &scenario, bool legacy)
{
    TraceScope scope;
    scope.single = kind == TransientKind::TurnOn || kind == TransientKind::ShortThenTurnOn;
    scope.forbidRmsMean = true;
    scope.allowReferenceQuartet = kind == TransientKind::TurnOff || kind == TransientKind::TurnOnThenShort;
    QAtomicInt stop(0);
    auto ctx = context(scope);
    ctx.timeoutMs = 5;
    QStringList logs;
    QVector<ScopeCaptureRecord> captures;
    ctx.log = [&](const QString &message) { logs.append(message); };
    ctx.captureRecord = [&](const ScopeCaptureRecord &record) {
        scope.trace << "CAPTURE " + record.phase;
        captures.append(record);
        if (scenario == "capture_throw") throw std::runtime_error("capture failed");
        if (scenario == "stop_capture") stop.storeRelease(1);
    };
    if (scenario == "max") ctx.target = "MAX";
    if (scenario == "min") ctx.target = "MIN";
    if (scenario == "no_event") scope.never = true;
    if (scenario == "intermittent") scope.intermittent = true;
    if (scenario == "baseline") scope.forceBaseline = true;
    if (scenario == "rescale") {
        scope.clipped = true;
        scope.clipEventOnly = true;
        scope.scale = 1;
        scope.requiredScale = 4;
    }
    if (scenario == "arm_failure") scope.failArm = true;
    if (scenario == "read_throw")
        scope.onRead = [&] { if (scope.event) throw std::runtime_error("read failed"); };
    if (scenario == "stop_event")
        scope.onState = [&] { if (scope.event) stop.storeRelease(1); };
    if (scenario == "stop_before") stop.storeRelease(1);
    if (scenario == "iteration_limit") ctx.maxIterations = 2;
    if (scenario == "power_on_throw") {
        auto power = ctx.power;
        ctx.power = [&, power](bool on, QString &e) {
            power(on, e);
            if (on) throw std::runtime_error("partial ON");
            return true;
        };
    }
    if (scenario == "off_failure") {
        auto power = ctx.power;
        ctx.power = [&, power](bool on, QString &e) {
            const bool afterEvent = scope.events > 0;
            power(on, e);
            if (!on && afterEvent) { e = "OFF rejected"; return false; }
            return true;
        };
    }
    if (scenario == "discharge_failure") {
        auto relays = ctx.relays;
        ctx.relays = [&, relays](bool shorted, bool discharge, QString &e) {
            relays(shorted, discharge, e);
            if (discharge && scope.events > 0) { e = "discharge rejected"; return false; }
            return true;
        };
    }
    if (scenario == "initial_off_failure") {
        auto power = ctx.power;
        ctx.power = [&, power](bool on, QString &e) {
            power(on, e);
            if (!on) { e = "initial OFF rejected"; return false; }
            return true;
        };
    }
    auto result = legacy ? runLegacyTransientStrategy(kind, ctx, &scope, stop)
                         : runTransientStrategy(kind, ctx, &scope, stop);
    return {result, scope.trace, logs, captures, scope.levels, scope.arms, scope.events};
}

bool sameNumber(double a, double b)
{
    return a == b || (std::isnan(a) && std::isnan(b));
}

void compareChannels(const QVector<OscChannelMeasure> &a, const QVector<OscChannelMeasure> &b)
{
    require(a.size() == b.size(), "channel count changed");
    for (int i = 0; i < a.size(); ++i) {
        require(a[i].channel == b[i].channel, "channel order changed");
        require(sameNumber(a[i].maxVal, b[i].maxVal) && sameNumber(a[i].minVal, b[i].minVal)
                && sameNumber(a[i].rms, b[i].rms) && sameNumber(a[i].mean, b[i].mean),
                "measurement values changed");
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QString scenario;
    try {
        const auto kind = static_cast<TransientKind>(QString::fromLocal8Bit(argv[1]).toInt());
        for (const auto &name : {"success", "max", "min", "no_event", "intermittent", "baseline",
                                "rescale", "arm_failure", "read_throw", "capture_throw",
                                "stop_capture", "stop_event", "stop_before", "iteration_limit",
                                "power_on_throw", "off_failure", "discharge_failure",
                                "initial_off_failure"}) {
            scenario = name;
            const auto before = observe(kind, scenario, true);
            const auto after = observe(kind, scenario, false);
            auto comparableTrace = after.trace;
            auto comparableLogs = after.logs;
            if (kind == TransientKind::TurnOnThenShort) {
                comparableLogs.clear();
                for (int i = 0; i < after.logs.size(); ++i) {
                    const auto &line = after.logs[i];
                    if (line == "Check scope before short") {
                        require(after.logs.value(i + 1) == "Relay: short=1 discharge=0" &&
                                after.logs.value(i + 2) == "Short applied; wait for event",
                                "OnShort readiness/action logs misplaced");
                        comparableLogs << "Apply short; wait for event";
                    } else if (line == "Short applied; wait for event") {
                        require(after.logs.value(i - 1) == "Relay: short=1 discharge=0",
                                "OnShort reported applied before relay");
                    } else if (line.contains(" pre-short trigger: Level ")) {
                        require(after.logs.value(i - 1) == "Baseline trigger excluded from event results",
                                "OnShort logged threshold recovery without excluded Baseline");
                    } else comparableLogs << line;
                }
            }
            if (kind == TransientKind::TurnOff && scenario != "stop_before") {
                // The authorized startup delta is checked once at its exact position;
                // every subsequent command/read/Capture/log still matches the legacy flow.
                require(after.trace.mid(0, 2) == QStringList({"ACQuire:STATE STOP", "TRIGger:A:MODe NORMAL"}),
                        "Turn off startup STOP/NORMAL missing or out of order");
                comparableTrace.remove(0, 2);
                const int inputOff = before.logs.indexOf("Input OFF");
                require(inputOff >= 0 && after.logs.mid(inputOff, 2) ==
                            QStringList({"Scope preparation: STOP", "Scope preparation: NORMAL"}),
                        "Turn off preparation logs not before initial Input OFF");
                comparableLogs.remove(inputOff, 2);
            }
            if (before.trace != comparableTrace) {
                for (int i = 0; i < qMin(before.trace.size(), comparableTrace.size()); ++i)
                    if (before.trace[i] != comparableTrace[i]) {
                        std::cerr << "trace[" << i << "] " << before.trace[i].toStdString()
                                  << " != " << comparableTrace[i].toStdString() << "\n";
                        break;
                    }
                throw std::runtime_error("device/read/Capture sequence changed");
            }
            require(before.logs == comparableLogs, "logs changed");
            require(before.arms == after.arms && before.events == after.events
                    && before.levels == after.levels, "event count/search thresholds changed");
            require(before.result.success == after.result.success
                    && before.result.cleanupFailed == after.result.cleanupFailed
                    && before.result.errorMessage == after.result.errorMessage, "result status changed");
            compareChannels(before.result.channels, after.result.channels);
            require(before.captures.size() == after.captures.size(), "capture count changed");
            for (int i = 0; i < before.captures.size(); ++i) {
                require(before.captures[i].phase == after.captures[i].phase
                        && before.captures[i].clipped == after.captures[i].clipped, "capture metadata changed");
                compareChannels(before.captures[i].channels, after.captures[i].channels);
            }
        }
        std::cout << "18 scenarios match legacy behavior\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << scenario.toStdString() << ": " << e.what() << "\n";
        return 1;
    }
}
