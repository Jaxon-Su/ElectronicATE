#include "../../../support/transient_test_support.h"
#include "steadystatesearchstrategy.h"
#include "steady_legacy_snapshot.h"

// Device/read/Capture traces are stable; polling counts depend on OS scheduling.
class SteadyScope : public Scope {
public:
    QString traceTarget;
    bool miss = false, always = false, invalidNormal = false, secondaryNaN = false;
    bool isClipping(int ch) override { return !autoMode && Scope::isClipping(ch); }
    QString getAcquisitionState() override {
        if (onState) onState();
        if (commError) return "invalid";
        if (autoMode) return "0";
        return !miss && (always || (negative ? level >= -2 : level <= 2)) ? "0" : "1";
    }
    double measureSignalPeak(int ch, const QString &type) override {
        // Single-direction searches now read the opposite extremum as well.
        // Exclude only that added metric; preserve all control/Capture ordering.
        if (autoMode || traceTarget == "BOTH" || type == "RMS" || type == "MEAN" ||
            type == (negative ? "MINimum" : "MAXimum"))
            trace << QString("READ %1 %2").arg(ch).arg(type);
        ++reads;
        if (onRead) onRead();
        if (!autoMode && (invalidNormal || (secondaryNaN && ch == 2)))
            return std::numeric_limits<double>::quiet_NaN();
        // Distinct RMS/Mean, channels and records detect accumulation/order changes.
        if (type == "RMS") return arms + ch * .1;
        if (type == "MEAN") return arms + ch * .2;
        return type == "MINimum" ? -2 : 2;
    }
    void stop() override { trace << "GUARD STOP"; }
};

struct Observation {
    OscMeasureResult result;
    QString exception;
    QStringList trace;
    QVector<ScopeCaptureRecord> captures;
    QVector<double> levels;
};

Observation observe(const QString &scenario, const QString &target, bool legacy)
{
    SteadyScope s;
    s.traceTarget = target;
    QAtomicInt stop(0);
    Observation observation;
    auto capture = [&](const ScopeCaptureRecord &record) {
        s.trace << "CAPTURE " + record.phase;
        observation.captures.append(record);
        if (scenario == "capture_throw") throw std::runtime_error("Capture failed");
        if (scenario == "capture_unknown") throw 42;
        if (scenario == "stop_capture") stop.storeRelease(1);
    };
    if (scenario == "flat") s.miss = true;
    if (scenario == "iteration_limit") s.always = true;
    if (scenario == "nan") s.invalidNormal = true;
    if (scenario == "secondary_na") s.secondaryNaN = true;
    if (scenario == "bad_source") s.source = "MATH1";
    if (scenario == "disabled_source") s.enabled = false;
    if (scenario == "stop_before") stop.storeRelease(1);
    if (scenario == "write_edge") s.failWrite = "TRIGger:A:TYPe EDGE";
    if (scenario == "write_level") s.onWrite = [&](const QString &cmd) {
        if (cmd.startsWith("TRIGger:A:LEVel")) s.failWrite = cmd;
    };
    if (scenario == "arm_reference") s.failArm = true;
    if (scenario == "arm_search") s.onArm = [&] { if (!s.autoMode) s.failArm = true; };
    if (scenario == "comm_reference") s.commError = true;
    if (scenario == "comm_search") s.onState = [&] { if (!s.autoMode) s.commError = true; };
    if (scenario == "stop_wait") s.onState = [&] { if (!s.autoMode) stop.storeRelease(1); };
    if (scenario == "stop_read") s.onRead = [&] { if (!s.autoMode) stop.storeRelease(1); };
    if (scenario == "read_throw") s.onRead = [&] { if (!s.autoMode) throw std::runtime_error("read failed"); };
    if (scenario == "read_unknown") s.onRead = [&] { if (!s.autoMode) throw 42; };
    if (scenario == "stop_level") s.onWrite = [&](const QString &cmd) {
        if (cmd.startsWith("TRIGger:A:LEVel")) stop.storeRelease(1);
    };
    if (scenario == "stop_failure") s.onWrite = [&](const QString &cmd) {
        if (s.arms > 1 && cmd == "ACQuire:STATE STOP") s.failWrite = cmd;
    };
    if (scenario == "rescale" || scenario == "scale_clamp") {
        s.scale = 1;
        s.clipped = true;
        s.clampScale = scenario == "scale_clamp";
    }
    LegacySteadyStateSearchStrategy before(5, 10000, 8, target);
    SteadyStateSearchStrategy after(5, 10000, 8, target);
    IOscilloscopeMeasureStrategy &strategy = legacy
        ? static_cast<IOscilloscopeMeasureStrategy &>(before) : after;
    // NA retries inside Capture use wall time and have nondeterministic read counts.
    // Compare search NA semantics without Capture; test_sequence_gap_legal_na covers Capture NA.
    if (scenario != "capture_disabled" && scenario != "nan" && scenario != "secondary_na")
        strategy.setCaptureObserver(capture);
    try {
        observation.result = strategy.execute(scenario == "null" ? nullptr : &s, stop);
    } catch (const std::exception &e) { observation.exception = e.what(); }
      catch (...) { observation.exception = "unknown"; }
    observation.trace = s.trace;
    observation.levels = s.levels;
    return observation;
}

bool equalNumber(double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); }
void compareChannels(const QVector<OscChannelMeasure> &a, const QVector<OscChannelMeasure> &b)
{
    require(a.size() == b.size(), "channel count changed");
    for (int i = 0; i < a.size(); ++i)
        require(a[i].channel == b[i].channel && equalNumber(a[i].maxVal, b[i].maxVal)
                && equalNumber(a[i].minVal, b[i].minVal) && equalNumber(a[i].rms, b[i].rms)
                && equalNumber(a[i].mean, b[i].mean), "measurement changed");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QString scenario;
    try {
        require(argc == 2, "target required");
        for (const auto *name : {"success", "flat", "iteration_limit", "nan", "secondary_na",
             "bad_source", "disabled_source", "stop_before", "write_edge", "write_level",
             "arm_reference", "arm_search", "comm_reference", "comm_search", "stop_wait",
             "stop_read", "read_throw", "read_unknown", "stop_level", "stop_failure",
             "rescale", "scale_clamp", "capture_throw", "capture_unknown", "stop_capture",
             "capture_disabled", "null"}) {
            scenario = name;
            const auto a = observe(scenario, argv[1], true);
            const auto b = observe(scenario, argv[1], false);
            require(a.trace == b.trace, "command/read/Capture order changed");
            require(a.levels == b.levels, "thresholds changed");
            require(a.exception == b.exception && a.result.success == b.result.success
                    && a.result.cleanupFailed == b.result.cleanupFailed
                    && a.result.errorMessage == b.result.errorMessage, "status/error changed");
            compareChannels(a.result.channels, b.result.channels);
            require(a.captures.size() == b.captures.size(), "Capture count changed");
            for (int i = 0; i < a.captures.size(); ++i) {
                require(a.captures[i].phase == b.captures[i].phase
                        && a.captures[i].clipped == b.captures[i].clipped, "Capture metadata changed");
                compareChannels(a.captures[i].channels, b.captures[i].channels);
            }
        }
        std::cout << "27 scenarios match legacy behavior\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << scenario.toStdString() << ": " << e.what() << "\n";
        return 1;
    }
}
