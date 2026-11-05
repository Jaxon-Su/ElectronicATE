#pragma once
#include <QString>
#include <QVector>
#include <limits>
#include "measurementtrial.h"

// Values reported by the regular scope measurement strategies.
struct OscChannelMeasure {
    int channel = 1;
    double maxVal = std::numeric_limits<double>::quiet_NaN();
    double minVal = std::numeric_limits<double>::quiet_NaN();
    double rms = std::numeric_limits<double>::quiet_NaN();
    double mean = std::numeric_limits<double>::quiet_NaN();
};

struct OscMeasureResult {
    bool success = false;
    bool cleanupFailed = false;
    QVector<OscChannelMeasure> channels;
    QString errorMessage;
    QVector<MeasurementTrial> trials;
    QVector<MeasurementReference> steadyReferences;
    QVector<MeasurementTrialIssue> trialIssues;
};
