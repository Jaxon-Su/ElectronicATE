#pragma once
#include "oscmeasurementresult.h"

// Describes one completed acquisition, including diagnostic records excluded from results.
struct ScopeCaptureRecord {
    QVector<OscChannelMeasure> channels;
    QString phase = "Measurement";
    bool clipped = false;
};
