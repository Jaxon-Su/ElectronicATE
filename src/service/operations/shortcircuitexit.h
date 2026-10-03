#pragma once
#include <QString>
#include <QThread>
#include <functional>
#include <exception>

// Runs on the instrument worker. Stop cannot skip the discharge/release sequence.
// A failed step must never fall through to a later relay state.
inline bool finishShortCircuit(const std::function<bool(QString&)>& inputOff,
                               const std::function<bool(bool, bool, QString&)>& relays,
                               bool holdShort, int powerOffMs, int dischargeMs, QString& error)
{
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
        while (ms > 0) {
            const auto chunk = qMin(20, ms);
            QThread::msleep(static_cast<unsigned long>(chunk));
            ms -= chunk;
        }
    };
    if (!step("Input OFF", inputOff)) return false;
    wait(powerOffMs);
    if (!step("Discharge ON", [&](QString& e) { return relays(holdShort, true, e); })) return false;
    wait(dischargeMs);
    if (!step("Discharge OFF", [&](QString& e) { return relays(holdShort, false, e); })) return false;
    if (holdShort && !step("Short OFF", [&](QString& e) { return relays(false, false, e); })) return false;
    error.clear();
    return true;
}
