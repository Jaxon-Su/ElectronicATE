#pragma once
#include "page5resultrecord.h"
#include <algorithm>
#include <cmath>

inline void accumulateGroupCondition(Page5ResultRecord &group, const Page5ConditionResult &condition)
{
    for (const auto &sample : condition.channels) {
        auto found = std::find_if(group.channels.begin(), group.channels.end(),
                                  [&](const auto &channel) { return channel.channel == sample.channel; });
        if (found == group.channels.end()) {
            Page5ChannelResult channel;
            channel.channel = sample.channel;
            channel.unit = sample.unit;
            group.channels.append(channel);
            found = group.channels.end() - 1;
        }
        auto &channel = *found;
        if (std::isfinite(sample.maximum) &&
            (!std::isfinite(channel.maximum) || sample.maximum > channel.maximum)) {
            channel.maximum = sample.maximum;
            channel.maximumSource = condition.condition;
        }
        if (std::isfinite(sample.minimum) &&
            (!std::isfinite(channel.minimum) || sample.minimum < channel.minimum)) {
            channel.minimum = sample.minimum;
            channel.minimumSource = condition.condition;
        }
        if (std::isfinite(sample.maximum) || std::isfinite(sample.minimum) ||
            std::isfinite(sample.rms) || std::isfinite(sample.mean)) {
            channel.rms = sample.rms;
            channel.mean = sample.mean;
            channel.latestSource = condition.condition;
        }
    }
    std::sort(group.channels.begin(), group.channels.end(),
              [](const auto &left, const auto &right) { return left.channel < right.channel; });
}
