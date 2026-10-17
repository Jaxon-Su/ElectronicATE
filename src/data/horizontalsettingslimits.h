#pragma once
#include <QString>
#include <cmath>
#include <optional>

namespace HorizontalSettingsLimits {
inline constexpr double previewDivisions = 10;
inline constexpr double largestExactCount = 9007199254740991.0;

struct Preview {
    std::optional<double> time, rate, points;
    QString error;
};

// A selected validation profile, not detected instrument capabilities. Unknown
// models keep numeric checks only. Firmware acceptance is confirmed by readback.
// MSO44B: 077154608 pp.19-20; 48W-74016-8 p.32 (real-time rate range).
inline Preview evaluate(bool manual, bool timeAndRate, std::optional<double> time,
                        std::optional<double> rate, std::optional<double> points,
                        const QString &profile)
{
    Preview result{time, manual ? rate : std::nullopt, manual ? points : std::nullopt, {}};
    if ((manual && (!rate || (timeAndRate ? !time : !points))) || (!manual && !time)) {
        result.error = "Enter positive numbers; RL must be a whole number of points.";
        return result;
    }
    if (manual) {
        if (timeAndRate) result.points = *time * *rate * previewDivisions;
        else result.time = *points / *rate / previewDivisions;
    }
    for (auto value : {result.time, result.rate, result.points}) {
        if (value && (!std::isfinite(*value) || *value <= 0)) {
            result.error = "The input combination is outside the numeric range.";
            return result;
        }
    }
    if (result.points && (*result.points < 1 || *result.points > largestExactCount)) {
        result.error = "RL must be at least one point and within the exact integer range.";
        return result;
    }
    if (profile == "MSO44B" || profile == "MSO44B_62M") {
        // Tolerance only absorbs floating-point conversion at exact boundaries.
        auto outside = [](double v, double lo, double hi) {
            return v < lo * (1 - 1e-12) || v > hi * (1 + 1e-12);
        };
        if (result.rate && outside(*result.rate, 1.5625, 6.25e9))
            result.error = "MSO44B real-time Sample Rate: 1.5625 S/s to 6.25 GS/s.";
        else if (outside(*result.time, 200e-12, 1000))
            result.error = "MSO44B Time Scale: 200 ps/div to 1000 s/div; RL also limits the range.";
        else if (result.points && outside(*result.points, 1000, profile == "MSO44B" ? 31.25e6 : 62.5e6))
            result.error = profile == "MSO44B"
                ? "MSO44B RL: 1k to 31.25M points. 62.5M requires the installed option."
                : "MSO44B RL with option: 1k to 62.5M points.";
    }
    if (result.points && result.error.isEmpty()) result.points = std::round(*result.points);
    return result;
}
} // namespace HorizontalSettingsLimits
