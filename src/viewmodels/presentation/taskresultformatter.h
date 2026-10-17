#pragma once
#include "testsequenceresultrecord.h"
#include "testsequencetaskkind.h"
#include <QSet>

namespace TaskResultFormatter {
inline QString measurement(const TestSequenceResultRecord &record, bool complete)
{
    if (!TestSequenceTaskKind::isMeasurement(record.taskName))
        return record.summary.simplified().left(160);
    QSet<int> channels, directions;
    int count = 0;
    const auto include = [&](const QVector<MeasurementTrial> &trials) {
        count += trials.size();
        for (const auto &trial : trials) {
            directions.insert(int(trial.direction));
            for (const auto &channel : trial.channels) channels.insert(channel.channel);
        }
    };
    include(record.trials);
    for (const auto &condition : record.conditions) include(condition.trials);
    QString text = QString("%1 directions · %2 CH · %3 trials").arg(directions.size()).arg(channels.size()).arg(count);
    if (!record.conditions.isEmpty()) text += QString(" · %1 loads").arg(record.conditions.size());
    if (!complete) text += " · Incomplete";
    return text + " · View details";
}
} // namespace TaskResultFormatter
