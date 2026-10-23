#pragma once
#include "iscopemeasurement.h"
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QThread>
#include <cmath>
#include <stdexcept>

namespace OscilloscopeAcquisition {
// Bounded checked write/readback, shared by automatic expansion and explicit reset.
inline void setScaleChecked(IScopeMeasurement *scope, QAtomicInt &stop,
                            int channel, double target, bool acceptLarger = false)
{
    auto check = [&] {
        if (stop.loadAcquire()) throw std::runtime_error("Stopped by user");
    };
    check();
    if (!scope || !std::isfinite(target) || target <= 0 || target >= 1e20)
        throw std::runtime_error("Invalid calculated channel scale");
    QString error;
    if (!scope->setVerticalScale(channel, target, error))
        throw std::runtime_error(("Scale adjustment failed: " + error).toStdString());
    QElapsedTimer applied;
    applied.start();
    for (;;) {
        check();
        const double actual = scope->getChannelScale(channel);
        error = scope->lastError();
        check();
        if (!error.isEmpty()) throw std::runtime_error(("Channel scale readback: " + error).toStdString());
        if (!std::isfinite(actual) || actual <= 0 || actual >= 1e20)
            throw std::runtime_error("Invalid channel scale readback");
        if (acceptLarger ? actual >= target * (1 - 1e-9) : std::abs(actual - target) <= target * 1e-9)
            return;
        if (applied.elapsed() >= 2000)
            throw std::runtime_error("Channel scale readback did not reach requested scale");
        QThread::msleep(20);
    }
}

}
