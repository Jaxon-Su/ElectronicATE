#pragma once
#include "iscopemeasurement.h"
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QDeadlineTimer>
#include <QThread>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace OscilloscopeAcquisition {
// Bounded checked write/readback, shared by automatic expansion and explicit reset.
inline double setScaleChecked(IScopeMeasurement *scope, QAtomicInt &stop,
                            int channel, double target, bool acceptLarger = false,
                            QDeadlineTimer deadline = QDeadlineTimer(QDeadlineTimer::Forever))
{
    double actual = std::numeric_limits<double>::quiet_NaN();
    auto check = [&] {
        if (stop.loadAcquire()) throw std::runtime_error("Stopped by user");
        if (deadline.hasExpired())
            throw std::runtime_error(QString("CH%1 scale adjustment phase time limit reached (timeout): requested=%2 actual=%3 units/div")
                .arg(channel).arg(target,0,'g',16).arg(std::isfinite(actual)?QString::number(actual,'g',16):"unavailable").toStdString());
    };
    check();
    if (!scope || !std::isfinite(target) || target <= 0 || target >= 1e20)
        throw std::runtime_error("Invalid calculated channel scale");
    QString error;
    if (!scope->setVerticalScale(channel, target, error))
        throw std::runtime_error(("Scale adjustment failed: " + error).toStdString());
    QElapsedTimer applied;
    applied.start();
    auto checkReadbackTime = [&] {
        if (applied.elapsed() >= 2000)
            throw std::runtime_error(QString("CH%1 scale readback timeout: requested=%2 actual=%3 units/div")
                .arg(channel).arg(target,0,'g',16).arg(std::isfinite(actual)?QString::number(actual,'g',16):"unavailable").toStdString());
    };
    for (;;) {
        check();
        checkReadbackTime();
        actual = scope->getChannelScale(channel);
        error = scope->lastError();
        check();
        if (!error.isEmpty()) throw std::runtime_error(("Channel scale readback: " + error).toStdString());
        if (!std::isfinite(actual) || actual <= 0 || actual >= 1e20)
            throw std::runtime_error("Invalid channel scale readback");
        checkReadbackTime();
        if (acceptLarger ? actual >= target * (1 - 1e-9) : std::abs(actual - target) <= target * 1e-9)
            return actual;
        QThread::msleep(20);
    }
}

inline double setPositionChecked(IScopeMeasurement *scope, QAtomicInt &stop, int channel, double target,
                                 QDeadlineTimer deadline = QDeadlineTimer(QDeadlineTimer::Forever))
{
    double actual = std::numeric_limits<double>::quiet_NaN();
    auto check = [&] {
        if (stop.loadAcquire()) throw std::runtime_error("Stopped by user");
        if (deadline.hasExpired())
            throw std::runtime_error(QString("CH%1 position adjustment phase time limit reached (timeout): requested=%2 actual=%3 div")
                .arg(channel).arg(target,0,'g',16).arg(std::isfinite(actual)?QString::number(actual,'g',16):"unavailable").toStdString());
    };
    check();
    if (!scope || !std::isfinite(target) || std::abs(target) >= 1e20)
        throw std::runtime_error("Invalid calculated channel position");
    QString error;
    if (!scope->setVerticalPosition(channel,target,error))
        throw std::runtime_error(("Position adjustment failed: " + error).toStdString());
    QElapsedTimer applied; applied.start();
    auto checkReadbackTime = [&] {
        if (applied.elapsed() >= 2000)
            throw std::runtime_error(QString("CH%1 position readback timeout: requested=%2 actual=%3 div")
                .arg(channel).arg(target,0,'g',16).arg(std::isfinite(actual)?QString::number(actual,'g',16):"unavailable").toStdString());
    };
    for (;;) {
        check();
        checkReadbackTime();
        ScopeVerticalRange range;
        if (!scope->readVerticalRange(channel,range,error))
            throw std::runtime_error(("Position readback: " + error).toStdString());
        actual = range.position;
        check();
        if (!std::isfinite(range.position) || std::abs(range.position) >= 1e20)
            throw std::runtime_error("Invalid channel position readback");
        checkReadbackTime();
        // Position is returned in divisions; allow sub-pixel device rounding.
        if (std::abs(range.position-target) <= 0.001) return range.position;
        QThread::msleep(20);
    }
}

}
