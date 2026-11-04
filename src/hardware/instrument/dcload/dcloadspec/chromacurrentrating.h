#pragma once
#include <QString>
#include <QVector>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "loadcapabilitycatalog.h"

inline QString checkedChromaCurrentRange(const QString &model, int channel, const QString &range, bool dynamic)
{
    const QString mode = range.trimmed().toUpper();
    const auto allowed = dynamic ? LoadCapabilityCatalog::supportedDynamicManualModes(model, channel)
                                 : LoadCapabilityCatalog::supportedManualModes(model, "CC", channel);
    if (!allowed.isEmpty() && (mode.isEmpty() || mode == "AUTO RANGE" || mode == "NO SETTING" || allowed.contains(mode)))
        return mode;
    throw std::invalid_argument(QString("%1: unsupported %2 current range '%3'")
                                    .arg(model, dynamic ? "dynamic" : "static", mode).toStdString());
}

// Catalogs list CC ranges in ascending order. Auto/No Setting use the module
// ceiling for validation; choosing (or retaining) a hardware range is separate.
template<class Range>
void requireChromaCurrentRating(const QVector<Range> &ranges, const QString &model,
                                double amps, double volts, QString mode)
{
    if (mode.startsWith("CCD")) mode.remove(2, 1);
    if (mode.isEmpty() || mode == "AUTO RANGE" || mode == "NO SETTING") mode = "CCH";
    for (const auto &range : ranges) {
        if (range.mode != mode) continue;
        // Decimal inputs at the exact watt limit can round one double above it.
        const double powerLimit = std::nextafter(range.power, std::numeric_limits<double>::infinity());
        if (amps >= 0 && amps <= range.currentSpec.maxCurrent && volts >= 0 &&
            volts <= range.voltageSpec.maxVoltage && amps * volts <= powerLimit) return;
        throw std::invalid_argument(QString("%1 %2: %3 A at %4 V exceeds the %5 A / %6 V / %7 W rating")
            .arg(model, mode).arg(amps).arg(volts).arg(range.currentSpec.maxCurrent)
            .arg(range.voltageSpec.maxVoltage).arg(range.power).toStdString());
    }
    throw std::invalid_argument((model + ": unsupported current range " + mode).toStdString());
}

inline double peakLoadCurrent(const QVector<double> &levels, const QVector<bool> &enabled, int segments)
{
    double peak = 0;
    for (int i = 0; i < segments && i < levels.size(); ++i) {
        if (!enabled.isEmpty() && !enabled.value(i)) continue;
        const double amps = levels[i];
        if (!std::isfinite(amps) || amps < 0) throw std::invalid_argument("Invalid load current");
        peak = qMax(peak, amps);
    }
    return peak;
}
