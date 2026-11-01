#pragma once
#include <QString>
#include <QThread>
#include <QElapsedTimer>
#include <functional>
#include <exception>
#include "../../data/shortcircuitprotection.h"

// Runs on the instrument worker. Stop cannot skip the discharge/release sequence.
// A failed step must never fall through to a later relay state.
inline bool finishShortCircuit(const std::function<bool(QString&)>& inputOff,
                               const std::function<bool(bool, bool, QString&)>& relays,
                               bool holdShort, int powerOffMs, int dischargeMs, QString& error,
                               const ShortCircuitExitDelays& delays = {})
{
    if (!delays.valid()) {
        error = "Invalid short-circuit exit delays; short-circuit relay state retained";
        return false;
    }
    auto step = [&](const char* name, auto action) {
        QString detail;
        try {
            if (action(detail)) return true;
        } catch (const std::exception& ex) { detail = QString::fromUtf8(ex.what()); }
          catch (...) { detail = "unexpected exception"; }
        error = QString::fromLatin1(name) + ": " + (detail.isEmpty() ? "operation failed" : detail)
                + "; short-circuit relay state retained";
        return false;
    };
    auto wait = [](int ms) {
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < ms) {
            const auto chunk = qMin<qint64>(20, ms - timer.elapsed());
            if (chunk <= 0) break;
            QThread::msleep(static_cast<unsigned long>(chunk));
        }
    };
    if (!step("Input OFF", inputOff)) return false;
    wait(powerOffMs);
    if (holdShort) {
        if (!step("Short ON", [&](QString& e) { return relays(true, false, e); })) return false;
        wait(delays.shortToDischargeMs);
    }
    if (!step("Discharge ON", [&](QString& e) { return relays(holdShort, true, e); })) return false;
    wait(dischargeMs);
    if (!step("Discharge OFF", [&](QString& e) { return relays(holdShort, false, e); })) return false;
    if (holdShort) {
        wait(delays.dischargeToReleaseMs);
        if (!step("Short OFF", [&](QString& e) { return relays(false, false, e); })) return false;
        wait(delays.afterReleaseMs);
    }
    error.clear();
    return true;
}
