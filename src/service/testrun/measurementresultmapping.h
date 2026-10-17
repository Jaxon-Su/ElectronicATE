#pragma once
#include "measurementresult.h"
#include "testsequenceresultrecord.h"
#include <cmath>
#include <algorithm>

// Select each channel independently, copying the entire winning acquisition.
// Strict comparisons make ties deterministic (first record wins). NA on the
// ranking metric cannot win; NA on another metric is preserved, never filled
// from a different trial.
inline QVector<TestSequenceChannelResult> representativeChannels(const QVector<MeasurementTrial> &trials)
{
    QVector<TestSequenceChannelResult> selected;
    for (const auto direction : {MeasurementDirection::Rise, MeasurementDirection::Fall}) {
        for (const auto &record : trials) {
            if (record.direction != direction) continue;
            for (const auto &value : record.channels) {
                const double rank = direction == MeasurementDirection::Rise ? value.maximum : value.minimum;
                if (!std::isfinite(rank)) continue;
                auto found = std::find_if(selected.begin(), selected.end(), [&](const auto &candidate) {
                    return candidate.channel == value.channel && candidate.direction == direction;
                });
                if (found != selected.end() &&
                    !(direction == MeasurementDirection::Rise ? rank > found->maximum : rank < found->minimum))
                    continue;
                TestSequenceChannelResult result{value.channel, value.unit, value.maximum, value.minimum, value.rms, value.mean};
                result.direction = direction;
                result.sourceSequence = record.sequence;
                result.step = record.step;
                result.trial = record.trial;
                result.triggerLevel = record.triggerLevel;
                if (found == selected.end()) selected.append(result);
                else *found = result;
            }
        }
    }
    return selected;
}

inline QVector<TestSequenceChannelResult> toTestSequenceChannels(const QVector<ChannelMeasurement> &measurements,
                                                  bool includeRmsMean = true)
{
    QVector<TestSequenceChannelResult> channels;
    channels.reserve(measurements.size());
    for (const auto &value : measurements) {
        TestSequenceChannelResult channel{value.channel, value.unit, value.maximum, value.minimum};
        if (includeRmsMean) {
            channel.rms = value.rms;
            channel.mean = value.mean;
        }
        channels.append(channel);
    }
    // Source fields keep the single-condition defaults. Group aggregation assigns provenance.
    return channels;
}
