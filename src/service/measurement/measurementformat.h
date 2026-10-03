#pragma once
#include <QString>
#include <cmath>

// Shared log/capture formatting; independent of strategy execution contracts.
inline QString formatOscMeasurement(double value)
{
    return std::isfinite(value) ? QString::number(value, 'g', 8) : QStringLiteral("NA");
}
