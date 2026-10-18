#include "oscmeasurementresult.h"
#include "brownoutrecord.h"
#include "measurementresult.h"
#include <cmath>
#include <iostream>
#include <type_traits>

// This target links Data/QtCore only: values must not need a strategy or a TestSequence model.
static_assert(std::is_same_v<decltype(BrownOutRecord{}.channels)::value_type, ChannelMeasurement>);
static_assert(std::is_same_v<decltype(MeasurementResult{}.channels)::value_type, ChannelMeasurement>);

int main()
{
    const OscChannelMeasure regular;
    const ChannelMeasurement waveform;
    const OscMeasureResult strategy;
    const BrownOutRecord record;
    const MeasurementResult result;
    if (regular.channel != 1 || waveform.channel != 0 || !waveform.unit.isEmpty() ||
        !std::isnan(regular.maxVal) || !std::isnan(regular.minVal) ||
        !std::isnan(regular.rms) || !std::isnan(regular.mean) ||
        !std::isnan(waveform.maximum) || !std::isnan(waveform.minimum) ||
        !std::isnan(waveform.rms) || !std::isnan(waveform.mean) ||
        strategy.success || strategy.cleanupFailed || !strategy.channels.isEmpty() ||
        record.sequence != 0 || !record.channels.isEmpty() ||
        result.success || result.cleanupFailed || !result.channels.isEmpty()) {
        std::cerr << "Unavailable measurement defaults changed to valid data\n";
        return 1;
    }
    return 0;
}
