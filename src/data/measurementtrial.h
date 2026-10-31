#pragma once
#include "channelmeasurement.h"
#include <QVector>

enum class MeasurementDirection { None, Rise, Fall };

// Completed steady references used only when their direction has no event.
// Turn off may select a different AUTO/Baseline quartet for each channel.
// It has no event sequence, threshold step or trial identity.
struct MeasurementReference {
    MeasurementDirection direction = MeasurementDirection::None;
    QVector<ChannelMeasurement> channels;
};

// One accepted event acquisition. Sequence is local to a strategy/Load invocation;
// step and trial identify the search threshold and its repetition (all 1-based).
// No Trigger, Baseline, AUTO reference and clipped records are not event trials.
struct MeasurementTrial {
    int sequence = 0;
    MeasurementDirection direction = MeasurementDirection::None;
    int step = 0;
    int trial = 0;
    double triggerLevel = std::numeric_limits<double>::quiet_NaN();
    QVector<ChannelMeasurement> channels;
};

// A failed or skipped scheduled trial, not an accepted event acquisition.
// Keep it outside trials so it cannot affect event counts or representatives.
struct MeasurementTrialIssue {
    MeasurementDirection direction = MeasurementDirection::None;
    int step = 0;
    int trial = 0;
    double triggerLevel = std::numeric_limits<double>::quiet_NaN();
    bool skipped = false;
    QString error;
};

inline QString measurementDirectionName(MeasurementDirection direction)
{
    switch (direction) {
    case MeasurementDirection::Rise: return QStringLiteral("Rise");
    case MeasurementDirection::Fall: return QStringLiteral("Fall");
    default: return QStringLiteral("—");
    }
}
