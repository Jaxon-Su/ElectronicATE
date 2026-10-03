#pragma once
#include "page5resultrecord.h"
#include <cmath>

namespace TaskResultFormatter
{
inline QString measurement(const Page5ResultRecord &record, bool complete)
{
    const auto number = [](double value) {
        return std::isfinite(value) ? QString::number(value, 'g', 8) : QStringLiteral("NA");
    };
    QStringList rows;
    if (!complete)
        rows << "Incomplete / NA";
    for (const auto &ch : record.channels) {
        rows << QString("CH%1   Max %2   Min %3   RMS %4   Mean %5")
                    .arg(ch.channel)
                    .arg(number(ch.maximum), number(ch.minimum), number(ch.rms), number(ch.mean));
        auto source = [&](const QString &metric, const Page5ConditionSource &condition) {
            if (condition.index >= 0)
                rows << QString("  %1: %2 row %3 (%4)").arg(metric, condition.typeName()).arg(condition.index + 1).arg(condition.label);
        };
        source("Max", ch.maximumSource);
        source("Min", ch.minimumSource);
        source("RMS/Mean (last record)", ch.latestSource);
    }
    if (rows.isEmpty())
        rows << "NA";
    if (!record.summary.isEmpty())
        rows << record.summary;
    return rows.join('\n');
}
} // namespace TaskResultFormatter
