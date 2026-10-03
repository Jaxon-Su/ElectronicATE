#pragma once
#include "page5resultrecord.h"
#include "page5taskkind.h"
#include <cmath>

namespace TaskResultFormatter
{
inline QString measurement(const Page5ResultRecord &record, bool complete)
{
    const auto number = [](double value) {
        return std::isfinite(value) ? QString::number(value, 'g', 8) : QStringLiteral("NA");
    };
    QStringList rows;
    const auto kind = Page5TaskKind::fromName(record.taskName);
    const bool extremaOnly = Page5TaskKind::isTransient(kind);
    const bool perCondition = Page5TaskKind::isGroup(kind);
    if (!complete)
        rows << "Incomplete / NA";
    const auto channelLine = [&](const Page5ChannelResult &ch) {
        QString line = QString("CH%1   Max %2   Min %3")
                    .arg(ch.channel)
                    .arg(number(ch.maximum), number(ch.minimum));
        if (!extremaOnly)
            line += QString("   RMS %1   Mean %2").arg(number(ch.rms), number(ch.mean));
        return line;
    };
    if (perCondition) {
        for (const auto &condition : record.conditions) {
            rows << QString("%1 row %2 (%3) — %4")
                        .arg(condition.condition.typeName()).arg(condition.condition.index + 1)
                        .arg(condition.condition.label, condition.status);
            if (condition.channels.isEmpty())
                rows << "  NA";
            for (const auto &ch : condition.channels)
                rows << "  " + channelLine(ch);
            if (!condition.error.isEmpty())
                rows << "  " + condition.error;
        }
    } else {
        for (const auto &ch : record.channels)
            rows << channelLine(ch);
    }
    if (rows.isEmpty())
        rows << "NA";
    if (!record.summary.isEmpty())
        rows << record.summary;
    return rows.join('\n');
}
} // namespace TaskResultFormatter
