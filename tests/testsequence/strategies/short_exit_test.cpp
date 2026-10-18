#include "../../support/transient_test_support.h"
#include <QElapsedTimer>
#include <QThread>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        const auto mode = app.arguments().value(1);
        for (auto kind : {TransientKind::ShortThenTurnOn, TransientKind::TurnOnThenShort}) {
            Scope s; s.single = kind == TransientKind::ShortThenTurnOn;
            auto ctx = context(s); ctx.target = "MAX"; ctx.timeoutMs = 3;
            ctx.dischargeMs = 3; ctx.powerOffMs = 2; ctx.trialsPerLevel = 1;
            bool powered = false, shorted = true, discharging = false, loaded = false;
            bool eventApplied = false, faulted = false, captureSeen = false;
            int offFailures = 0, eventArchives = 0;
            qint64 offAt = -1, dischargeAt = -1;
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
                if (!on) offAt = timer.elapsed();
                if (on && s.single) eventApplied = true;
                return originalPower(on, error);
            };
            ctx.relays = [&](bool sh, bool dis, QString& error) {
                require(!faulted, "relay command issued after failed cleanup step");
                if (!sh) {
                    require(!powered, "short released while input was ON");
                    require(dischargeAt >= 0 && timer.elapsed() - dischargeAt >= ctx.dischargeMs,
                            "short released without completing discharge");
                }
                if (dis) {
                    require(!powered && offAt >= 0 && timer.elapsed() - offAt >= ctx.powerOffMs,
                            "discharge started before input OFF wait");
                    dischargeAt = timer.elapsed();
                }
                if (eventApplied && ((mode == "discharge_on" && dis) ||
                    (mode == "discharge_off" && sh && !dis && discharging) ||
                    (mode == "short_off" && !sh))) {
                    faulted = true; error = "relay failed"; return false;
                }
                shorted = sh; discharging = dis;
                if (powered && sh) eventApplied = true;
                if (eventApplied && mode == "stop_discharge" && dis) stop.storeRelease(1);
                return originalRelay(sh, dis, error);
            };
            ctx.load = [&](bool on, QString&) { loaded = on; return true; };
            s.onRead = [&] {
                if (s.event && !s.autoMode && eventApplied) {
                    require(!powered && !shorted && !discharging, "event measurement read before safe exit");
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
