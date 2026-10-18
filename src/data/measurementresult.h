#pragma once
#include "channelmeasurement.h"
#include "measurementtrial.h"
#include <QVector>

// Result shared by all measurement strategies, independent of TestSequence and reporting.
struct MeasurementResult {
    bool success = false, cleanupFailed = false;
    QString error, summary;
    QVector<ChannelMeasurement> channels;
    bool outputCleanupFailed = false; // Output uncertainty is distinct from scope restoration.
    QVector<MeasurementTrial> trials;
    QVector<MeasurementReference> steadyReferences;
};
