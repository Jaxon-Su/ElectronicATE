#include "chromaload6310spec.h"
#include <limits>
#include <stdexcept>

void validate6310Rating(const QString &model, double current, double voltage, const QString &label)
{
    const auto range = findPowerRange(model, std::numeric_limits<double>::max());
    if (!range || current > range->currentSpec.maxCurrent || voltage > range->voltageSpec.maxVoltage ||
        current * voltage > range->power)
        throw std::invalid_argument((label + ": load current/voltage/power exceeds the model rating").toStdString());
}
