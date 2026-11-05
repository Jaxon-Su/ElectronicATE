#include "../../../support/transient_test_support.h"
#include "steadystatesearchstrategy.h"
#include "steady_legacy_snapshot.h"

// Device/read/Capture traces are stable; polling counts depend on OS scheduling.
class SteadyScope : public Scope {
public:
    QString traceTarget;
    bool miss = false, always = false, invalidNormal = false, secondaryNaN = false;
    int directionArms = 0;
    bool faultInjected = false;
    bool writeConfigurationCommand(const QString &command, QString &error) override {
        if(command.startsWith("TRIGger:A:EDGE:SLOpe "))directionArms=0;
        return Scope::writeConfigurationCommand(command,error);
    }
    bool beginSingleAcquisition() override {
        if(!autoMode)++directionArms;
        return Scope::beginSingleAcquisition();
    }
    bool isClipping(int ch) override { return !autoMode && Scope::isClipping(ch); }
    QString getAcquisitionState() override {
        if (onState) onState();
        if (commError) return "invalid";
        if (autoMode) return "0";
        // Compare lifecycle on the same acquired records. Level policy has an
        // independent golden test; it must not bypass read/Stop fault injection here.
        return !miss && (always || directionArms <= (clipped ? 4 : 2)) ? "0" : "1";
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
    if (scenario == "stop_read") s.onRead = [&] { if (!s.autoMode) {s.faultInjected=true;stop.storeRelease(1);} };
    if (scenario == "read_throw") s.onRead = [&] { if (!s.autoMode) {s.faultInjected=true;throw std::runtime_error("read failed");} };
    if (scenario == "read_unknown") s.onRead = [&] { if (!s.autoMode) {s.faultInjected=true;throw 42;} };
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
    if(target!="INVALID" && (scenario=="stop_read"||scenario=="read_throw"||scenario=="read_unknown"))
        require(s.faultInjected,"comparison skipped the injected Normal read fault");
    return observation;
}

QStringList controlTrace(const QStringList &trace, bool checkGrid, double scale)
{
    QStringList result;
    for(const auto &line:trace) {
        if(line.startsWith("CH1:SCAle "))scale=line.section(' ',-1).toDouble();
        if(line.startsWith("TRIGger:A:LEVel")) {
            const double level=line.section(' ',-1).toDouble();
            if(checkGrid)require(std::abs(level/(scale/25)-std::round(level/(scale/25)))<1e-8,"non-grid steady Level");
            result<<line.section(' ',0,0)+" <Level>";
        } else result<<line;
    }
    return result;
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
            const double initialScale=scenario=="rescale"||scenario=="scale_clamp"?1:10;
            require(controlTrace(a.trace,false,initialScale) == controlTrace(b.trace,true,initialScale), "command/read/Capture order changed");
            require(a.levels.size() == b.levels.size(), "acquisition sequence changed");
            if(scenario=="success" && QString(argv[1])!="INVALID") {
                const QVector<double> expected=QString(argv[1])=="MAX"?QVector<double>{.4,2.4,2.8,3.2}:
                    QString(argv[1])=="MIN"?QVector<double>{.4,-2.4,-2.8,-3.2}:QVector<double>{.4,2.4,2.8,3.2,-2.4,-2.8,-3.2};
                require(b.levels.size()==expected.size(),"steady golden Level count");
                for(int i=0;i<expected.size();++i)require(std::abs(b.levels[i]-expected[i])<1e-10,"steady golden Level");
            }
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
        std::cout << "27 scenarios preserve control/read/Capture contracts; Level grid checked separately\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << scenario.toStdString() << ": " << e.what() << "\n";
        return 1;
    }
}
