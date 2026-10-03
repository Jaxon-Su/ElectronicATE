#pragma once
#include <QString>
#include <limits>

// Page-neutral waveform values. No task/report provenance or inferred probe unit.
struct ChannelMeasurement {
    int channel = 0;
    QString unit;
    double maximum = std::numeric_limits<double>::quiet_NaN();
    double minimum = std::numeric_limits<double>::quiet_NaN();
    double rms = std::numeric_limits<double>::quiet_NaN();
    double mean = std::numeric_limits<double>::quiet_NaN();
};
