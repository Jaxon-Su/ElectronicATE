#pragma once
#include "channelmeasurement.h"
#include <QVector>

enum class MeasurementDirection { None, Rise, Fall };

// A completed steady acquisition used only when its direction has no event.
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

inline QString measurementDirectionName(MeasurementDirection direction)
{
    switch (direction) {
    case MeasurementDirection::Rise: return QStringLiteral("Rise");
    case MeasurementDirection::Fall: return QStringLiteral("Fall");
    default: return QStringLiteral("—");
    }
}
