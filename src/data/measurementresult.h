#pragma once
#include "channelmeasurement.h"
#include <QVector>

// Result shared by all measurement strategies, independent of Page5 and reporting.
struct MeasurementResult {
    bool success = false, cleanupFailed = false;
    QString error, summary;
    QVector<ChannelMeasurement> channels;
    bool outputCleanupFailed = false; // Output uncertainty is distinct from scope restoration.
};
