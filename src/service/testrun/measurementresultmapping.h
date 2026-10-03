#pragma once
#include "measurementresult.h"
#include "page5resultrecord.h"

inline QVector<Page5ChannelResult> toPage5Channels(const QVector<ChannelMeasurement> &measurements,
                                                  bool includeRmsMean = true)
{
    QVector<Page5ChannelResult> channels;
    channels.reserve(measurements.size());
    for (const auto &value : measurements) {
        Page5ChannelResult channel{value.channel, value.unit, value.maximum, value.minimum};
        if (includeRmsMean) {
            channel.rms = value.rms;
            channel.mean = value.mean;
        }
        channels.append(channel);
    }
    // Source fields keep the single-condition defaults. Group aggregation assigns provenance.
    return channels;
}
