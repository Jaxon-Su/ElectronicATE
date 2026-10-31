#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

// Zero-based trigger grid in channel units. Measurements stay unquantized.
// Both polarities use outward coordinates (Rise: level, Fall: -level).
class TriggerLevelGrid
{
public:
    explicit TriggerLevelGrid(double step) : m_step(step)
    {
        if (!std::isfinite(step) || step <= 0 || step >= 1e30)
            throw std::invalid_argument("Invalid trigger level step");
    }

    double atOrAbove(double value) const { return valueAt(std::ceil(index(value))); }
    double above(double value) const { return valueAt(std::floor(index(value)) + 1); }
    double next(double value) const { return valueAt(std::ceil(index(value)) + 1); }
    double floorIndex(double value) const { return std::floor(index(value)); }
    double ceilIndex(double value) const { return std::ceil(index(value)); }

    double valueAt(double integralIndex) const
    {
        const double value = integralIndex * m_step;
        if (!std::isfinite(integralIndex) || std::abs(integralIndex) > 0x1p48 ||
            !std::isfinite(value) || std::abs(value) >= 1e30)
            throw std::runtime_error("Trigger level grid cannot advance safely");
        return value == 0 ? 0 : value;
    }

private:
    double index(double value) const
    {
        const double quotient = value / m_step;
        if (!std::isfinite(value) || std::abs(value) >= 1e30 ||
            !std::isfinite(quotient) || std::abs(quotient) > 0x1p48)
            throw std::runtime_error("Trigger level grid cannot advance safely");
        const double nearest = std::round(quotient);
        // Remove binary rounding noise at exact grid boundaries (including negatives).
        const double noise = 4 * std::numeric_limits<double>::epsilon() *
                             std::max(1.0, std::abs(quotient));
        return std::abs(quotient - nearest) <= noise ? nearest : quotient;
    }
    double m_step;
};
