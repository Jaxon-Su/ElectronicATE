#include "../../support/transient_test_support.h"
#include <QElapsedTimer>
#include <QThread>
#include "../../../src/service/operations/shortcircuitexit.h"

static void verifyDefaultExitTiming()
{
    QElapsedTimer clock; clock.start();
    qint64 shortOn = -1, dischargeOff = -1, shortOff = -1;
    bool discharging = false;
    QString error;
    const bool ok = finishShortCircuit([](QString&) { return true; },
        [&](bool sh, bool dis, QString&) {
            if (sh && !dis && shortOn < 0) shortOn = clock.elapsed();
            if (dis) require(shortOn >= 0 && clock.elapsed() - shortOn >= 1000,
                             "Discharge ON did not wait 1000 ms after Short ON");
            if (sh && !dis && discharging) dischargeOff = clock.elapsed();
            if (!sh) {
                require(dischargeOff >= 0 && clock.elapsed() - dischargeOff >= 1000,
                        "Short OFF did not wait 1000 ms after Discharge OFF");
                shortOff = clock.elapsed();
            }
            discharging = dis;
            return true;
        }, true, 0, 0, error);
    require(ok, qPrintable(error));
    require(shortOff >= 0 && clock.elapsed() - shortOff >= 2000,
            "Short exit returned before 2000 ms after Short OFF");
    // Every operation failure stops the sequence, including the new Short ON confirmation.
    for (int failAt = 0; failAt < 5; ++failAt) for (bool throws : {false, true}) {
        int calls = 0;
        auto action = [&](QString& e) {
            if (calls++ != failAt) return true;
            if (throws) throw std::runtime_error("injected exit failure");
            e = "injected exit failure"; return false;
        };
        require(!finishShortCircuit(action, [&](bool, bool, QString& e) { return action(e); },
                                    true, 0, 0, error, {0, 0, 0}) && calls == failAt + 1,
                "exit continued after failed/throwing operation");
        require(error.contains("retained"), "exit failure lost retained state");
    }
    QStringList states;
    require(finishShortCircuit([](QString&) { return true; }, [&](bool sh, bool dis, QString&) {
        states << QString("%1%2").arg(sh).arg(dis); return true;
    }, false, 0, 0, error), "non-shorted baseline exit failed");
    require(states == QStringList({"01", "00"}), "baseline exit introduced short circuit");
}

static void verifyStoppedExit()
{
    for (auto kind : {TransientKind::ShortThenTurnOn, TransientKind::TurnOnThenShort}) {
        Scope scope; auto cfg = context(scope); QAtomicInt stop{0};
        cfg.shortExitDelays = {5, 7, 11};
        bool loaded = false; int releases = 0; qint64 releasedAt = -1;
        QElapsedTimer clock; clock.start();
        cfg.relays = [&](bool sh, bool dis, QString&) {
            if (!sh && !dis) { ++releases; releasedAt = clock.elapsed(); stop.storeRelease(1); }
            return true;
        };
        cfg.load = [&](bool on, QString&) { if (on) loaded = true; return true; };
        const auto result = runTransientStrategy(kind, cfg, &scope, stop);
        require(!result.success && !result.cleanupFailed && releases == 1 && !loaded,
                "Stop during short release started Load or repeated cleanup");
        require(releasedAt >= 0 && clock.elapsed() - releasedAt >= 11,
                "Stop bypassed post-release protection wait");
    }
}

static void verifyReleaseCrossesPhase()
{
    Scope scope; scope.single = true;
    auto cfg = context(scope); cfg.target = "MAX"; cfg.trialsPerLevel = 1;
    cfg.phaseTimeoutMs = 200; cfg.shortExitDelays = {0, 0, 300};
    QAtomicInt stop{0}; bool powered = false, shorted = false, loaded = false;
    int inputs = 0, captures = 0;
    const auto power = cfg.power; const auto relays = cfg.relays;
    cfg.power = [&](bool on, QString& e) { powered = on; if (on) ++inputs; return power(on,e); };
    cfg.relays = [&](bool sh, bool dis, QString& e) { shorted = sh; return relays(sh,dis,e); };
    cfg.load = [&](bool on, QString&) { loaded = on; return true; };
    cfg.captureRecord = [&](const ScopeCaptureRecord&) {
        require(!powered && !shorted, "late diagnostic before safe exit"); ++captures;
    };
    const auto result = runTransientStrategy(TransientKind::ShortThenTurnOn, cfg, &scope, stop);
    require(!result.success && !result.cleanupFailed &&
            (result.errorMessage.contains("timeout") || result.errorMessage.contains("time limit")),
            "release wait bypassed Phase deadline");
    require(inputs == 1 && scope.arms == 1 && captures == 1 && result.trials.isEmpty(),
            "late release lost diagnostic, counted an event or started next Trial");
    require(!powered && !shorted && !loaded, "Phase expiry skipped release cleanup");
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        const auto mode = app.arguments().value(1);
        if (mode == "success") { verifyDefaultExitTiming(); verifyStoppedExit(); verifyReleaseCrossesPhase(); }
        for (auto kind : {TransientKind::ShortThenTurnOn, TransientKind::TurnOnThenShort}) {
            Scope s; s.single = kind == TransientKind::ShortThenTurnOn;
            auto ctx = context(s); ctx.target = "MAX"; ctx.timeoutMs = 3;
            ctx.dischargeMs = 3; ctx.powerOffMs = 2; ctx.trialsPerLevel = 1;
            ctx.shortExitDelays = {5, 7, 11};
            bool powered = false, shorted = true, discharging = false, loaded = false;
            bool eventApplied = false, faulted = false, captureSeen = false;
            int offFailures = 0, eventArchives = 0;
            qint64 offAt = -1, dischargeAt = -1;
            qint64 confirmAt = -1, dischargeOffAt = -1, releasedAt = -1;
            QElapsedTimer timer; timer.start(); QAtomicInt stop{0};
            const auto originalPower = ctx.power;
            const auto originalRelay = ctx.relays;
            ctx.power = [&](bool on, QString& error) {
                if (!on && eventApplied && mode.startsWith("off_")) {
                    ++offFailures; faulted = true;
                    if (mode == "off_throw") throw std::runtime_error("OFF exception");
                    if (mode == "off_unknown") throw 42;
                    error = "OFF rejected"; return false;
                }
                powered = on;
                if (on && releasedAt >= 0) require(timer.elapsed() - releasedAt >= 11, "Input ON before release wait");
                if (!on) offAt = timer.elapsed();
                if (on && s.single) eventApplied = true;
                return originalPower(on, error);
            };
            ctx.relays = [&](bool sh, bool dis, QString& error) {
                require(!faulted, "relay command issued after failed cleanup step");
                if (sh && !dis && !discharging && !powered) confirmAt = timer.elapsed();
                if (sh && !dis && discharging) dischargeOffAt = timer.elapsed();
                if (!sh) {
                    require(!powered, "short released while input was ON");
                    require(dischargeAt >= 0 && timer.elapsed() - dischargeAt >= ctx.dischargeMs,
                            "short released without completing discharge");
                    if (shorted) require(dischargeOffAt >= 0 && timer.elapsed() - dischargeOffAt >= 7,
                                         "short released before discharge-off separation");
                }
                if (dis) {
                    require(!powered && offAt >= 0 && timer.elapsed() - offAt >= ctx.powerOffMs,
                            "discharge started before input OFF wait");
                    dischargeAt = timer.elapsed();
                    if (sh) require(confirmAt >= 0 && timer.elapsed() - confirmAt >= 5,
                                    "discharge before short-on separation");
                }
                if (eventApplied && ((mode == "discharge_on" && dis) ||
                    (mode == "discharge_off" && sh && !dis && discharging) ||
                    (mode == "short_off" && !sh))) {
                    faulted = true; error = "relay failed"; return false;
                }
                if (!sh && shorted) releasedAt = timer.elapsed();
                if (sh && powered && releasedAt >= 0) require(timer.elapsed() - releasedAt >= 11, "next Short ON before release wait");
                shorted = sh; discharging = dis;
                if (powered && sh) eventApplied = true;
                if (eventApplied && mode == "stop_discharge" && dis) stop.storeRelease(1);
                return originalRelay(sh, dis, error);
            };
            ctx.load = [&](bool on, QString&) { loaded = on; return true; };
            s.onRead = [&] {
                if (s.event && !s.autoMode && eventApplied) {
                    require(!powered && !shorted && !discharging, "event measurement read before safe exit");
                    if (releasedAt >= 0) require(timer.elapsed() - releasedAt >= 11, "read before release wait");
                    if (mode == "read_throw") throw std::runtime_error("measurement failure");
                }
            };
            ctx.captureRecord = [&](const ScopeCaptureRecord&) {
                if (s.autoMode || !s.event || !eventApplied) return;
                require(!powered && !shorted && !discharging, "Capture ran with live short circuit");
                captureSeen = true; ++eventArchives;
                const auto starts = s.arms;
                const auto value = s.measureSignalPeak(1, "MAXimum");
                QThread::msleep(5);
                require(!powered && !shorted && starts == s.arms && s.measureSignalPeak(1, "MAXimum") == value,
                        "Capture changed frozen record or re-energized DUT");
                if (mode == "capture_throw") throw std::runtime_error("capture failure");
                if (mode == "stop_capture") stop.storeRelease(1);
            };
            if (mode == "no_trigger") s.never = true;
            if (mode == "arm_failure") s.failArm = true;
            if (mode == "stop_wait") s.onState = [&] { if (eventApplied) stop.storeRelease(1); };
            const auto result = runTransientStrategy(kind, ctx, &s, stop);
            require(!loaded, "Load OFF skipped");
            if (mode.startsWith("off_") || mode.startsWith("discharge_") || mode == "short_off") {
                require(faulted && result.cleanupFailed && !result.success && shorted,
                        "failed cleanup released short or reported success");
                require(!captureSeen, "file transfer attempted after failed shutdown");
                if (mode.startsWith("off_")) require(powered && offFailures == 1, "automatic OFF retry bypassed retained short");
            } else {
                require(!powered && !shorted && !discharging, "outputs left active after cleanup");
                require(!result.cleanupFailed, qPrintable(result.errorMessage));
                if (mode == "success") require(result.success && eventArchives > 0, qPrintable(result.errorMessage));
                else if (mode != "no_trigger") require(!result.success, "fault/Stop was reported as success");
            }
        }
        std::cout << "PASS safe short exit " << mode.toStdString() << '\n';
    } catch (const std::exception& ex) { std::cerr << ex.what(); return 1; }
}
