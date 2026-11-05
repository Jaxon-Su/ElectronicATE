#pragma once
#include <QString>
#include <stdexcept>

namespace Chroma63107Range {
inline bool applies(const QString &module) { return module == "63107" || module == "63107A"; }
inline bool left(int channel) { return channel > 0 && channel % 2 == 1; }

// The left output has one 5 A / 30 W range. The right output has two ranges.
inline QString select(int channel, double current, double voltage, const QString &range)
{
    if (channel < 1 || channel > 8) throw std::invalid_argument("63107 requires its physical CH1..8");
    const bool lowOnly = left(channel);
    const bool low = range == "CCL" || range == "CCDL";
    const bool high = range == "CCH" || range == "CCDH";
    const double maxCurrent = lowOnly ? 5.0 : low ? 4.0 : 40.0;
    const double maxPower = lowOnly || low ? 30.0 : 250.0;
    if ((lowOnly && high) || !(current >= 0 && current <= maxCurrent && voltage >= 0 &&
        voltage <= 80 && current * voltage <= maxPower))
        throw std::invalid_argument("63107 current, voltage, power or range exceeds the selected channel rating");
    return lowOnly || low || (!high && current <= 4.0 * 0.95 && current * voltage <= 30.0 * 0.95)
        ? QStringLiteral("CCL") : QStringLiteral("CCH");
}
} // namespace Chroma63107Range
