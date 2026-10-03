#pragma once
#include <cmath>

// Shared field constraints. Draft completeness and execution readiness are separate policies.
namespace TaskSettingRules
{
struct Integer
{
    const char *key;
    int initial, minimum, maximum;
    bool accepts(double value) const
    {
        return std::isfinite(value) && std::floor(value) == value && value >= minimum && value <= maximum;
    }
};
inline constexpr Integer settle{"delay_ms", 5000, 0, 3600000};
inline constexpr Integer triggerTimeout{"trig_timeout_ms", 10000, 1, 60000};
inline constexpr Integer phaseTimeout{"phase_timeout_ms", 900000, 1000, 3600000};
inline constexpr Integer discharge{"discharge_ms", 10000, 1, 3600000};
inline constexpr Integer trials{"trials_per_level", 3, 1, 10};
inline constexpr Integer retry{"retry", 0, 0, 100};
inline constexpr Integer captureChannel{"channel", 0, 0, 8};
} // namespace TaskSettingRules
