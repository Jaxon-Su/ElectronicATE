#pragma once
#include "testsequenceresultrecord.h"
#include "testsequencetaskkind.h"
#include <cmath>
#include <algorithm>
#include <QMap>

// Shared row contract for the read-only details model and the two Excel sheets.
// Keep numbers numeric for Excel; formatting belongs to the table model.
namespace TestSequenceResultRows {
enum Column { Seq, Task, External, Channel, Max, Min, Rms, Mean, Status, Note,
              Run, Attempt, Load, Direction, Record, Step, Trial, Level, Unit, Count };
inline QStringList headers()
{
    return {"Seq", "Test Item", "Ext. Name", "CH", "Max", "Min", "RMS", "Mean", "Status", "Note",
            "Run", "Attempt", "Load", "Direction", "Record", "Step", "Trial", "Trigger Level", "Unit"};
}
inline QVariant number(double value)
{
    return std::isfinite(value) ? QVariant(value) : QVariant(QStringLiteral("NA"));
}
inline QString loadDescription(const TestSequenceConditionSource &source)
{
    if (source.index < 0) return {};
    const auto row = QString("%1 row %2").arg(source.typeName()).arg(source.index + 1);
    return source.label.isEmpty() ? row : QString("%1 (%2)").arg(row, source.label);
}
inline QVector<QVariantList> rows(const TestSequenceResultRecord &record, bool summary)
{
    QVector<QVariantList> output;
    const auto append = [&](const QString &load, const QString &status, const QString &note,
                            const TestSequenceChannelResult *channel, MeasurementDirection direction,
                            const QString &missing = QString()) {
        const QString rowNote = channel && channel->source == MeasurementSource::SteadyReference
            ? QStringList{QStringLiteral("Steady reference (no transient trigger)"), note.trimmed()}
                  .join("\n").trimmed()
            : note;
        QVariantList row{record.taskIndex + 1, record.taskName, record.externalName,
                         channel ? QString("CH%1").arg(channel->channel) : QString(),
                         channel ? number(channel->maximum) : QVariant(missing),
                         channel ? number(channel->minimum) : QVariant(missing),
                         channel ? number(channel->rms) : QVariant(missing),
                         channel ? number(channel->mean) : QVariant(missing),
                         status, rowNote,
                         record.runId, record.attempt, load,
                         measurementDirectionName(direction),
                         channel && channel->sourceSequence > 0 ? QVariant(channel->sourceSequence) : QVariant(),
                         channel && channel->sourceSequence > 0 ? QVariant(channel->step) : QVariant(),
                         channel && channel->sourceSequence > 0 ? QVariant(channel->trial) : QVariant(),
                         channel && channel->sourceSequence > 0 ? number(channel->triggerLevel) : QVariant(),
                         channel ? channel->unit : QString()};
        output.append(row);
    };
    const QString note = QStringList{record.error, record.summary}.join("\n").trimmed();
    const bool descentLevels = TestSequenceTaskKind::isDescent(TestSequenceTaskKind::fromName(record.taskName));
    const auto appendMeasurements = [&](const QString &load, const QString &status, const QString &details,
                                        const QVector<TestSequenceChannelResult> &channels,
                                        const QVector<MeasurementTrial> &trials,
                                        const QVector<MeasurementTrialIssue> &issues) {
        const auto firstRow = output.size();
        const auto appendIssues = [&] {
            for (const auto &issue : issues) {
                append(load, issue.skipped ? "Skipped" : "Fail", issue.error + "\nTask status: " + status,
                       nullptr, issue.direction, "NA");
                auto &row = output.last();
                row[Step] = issue.step; row[Trial] = issue.trial; row[Level] = number(issue.triggerLevel);
            }
            if (descentLevels && !summary)
                std::stable_sort(output.begin() + firstRow, output.end(), [](const auto &a, const auto &b) {
                    return a[Step].toInt() == b[Step].toInt() ? a[Trial].toInt() < b[Trial].toInt()
                                                           : a[Step].toInt() < b[Step].toInt();
                });
        };
        if (!summary) {
            bool hasReference = false;
            for (const auto &channel : channels)
                if (channel.source == MeasurementSource::SteadyReference) {
                    append(load, status, details, &channel, channel.direction);
                    hasReference = true;
                }
            for (const auto &trial : trials) {
                const QString trialStatus = descentLevels ? "Pass" : status;
                const QString trialDetails = descentLevels ? "Task status: " + status + "\n" + details : details;
                if (descentLevels && trial.channels.isEmpty()) {
                    append(load, trialStatus, trialDetails + "\nTrigger-only record; no enabled measurement channels",
                           nullptr, trial.direction, "NA");
                    auto &row = output.last();
                    row[Record] = trial.sequence; row[Step] = trial.step;
                    row[Trial] = trial.trial; row[Level] = number(trial.triggerLevel);
                }
                for (const auto &value : trial.channels) {
                    TestSequenceChannelResult channel{value.channel, value.unit, value.maximum, value.minimum, value.rms, value.mean};
                    channel.sourceSequence = trial.sequence;
                    channel.step = trial.step;
                    channel.trial = trial.trial;
                    channel.triggerLevel = trial.triggerLevel;
                    append(load, trialStatus, trialDetails, &channel, trial.direction);
                }
            }
            if (trials.isEmpty() && !hasReference && issues.isEmpty())
                append(load, status, details + "\nNo valid event trials", nullptr, MeasurementDirection::None, "NA");
            appendIssues();
            return;
        }
        for (const auto &channel : channels)
            append(load, status, details, &channel, channel.direction);
        const QString target = record.settings.value("measure_target", "BOTH").toString();
        for (const auto direction : {MeasurementDirection::Rise, MeasurementDirection::Fall}) {
            // A secondary channel can have only NA ranking values while the
            // trigger channel is valid. Keep that channel visible without
            // inventing a representative acquisition or borrowing other values.
            QMap<int, QString> missingChannels;
            for (const auto &trial : trials) {
                if (trial.direction != direction) continue;
                for (const auto &value : trial.channels)
                    if (std::none_of(channels.cbegin(), channels.cend(), [&](const auto &ch) {
                            return ch.direction == direction && ch.channel == value.channel;
                        })) missingChannels.insert(value.channel, value.unit);
            }
            for (auto it = missingChannels.cbegin(); it != missingChannels.cend(); ++it) {
                TestSequenceChannelResult missing;
                missing.channel = it.key(); missing.unit = it.value();
                append(load, status, details + "\nNo valid ranking value for this channel", &missing, direction);
            }
            const bool present = std::any_of(channels.cbegin(), channels.cend(), [&](const auto &ch) { return ch.direction == direction; });
            if (present || !missingChannels.isEmpty()) continue;
            bool selected = target == "BOTH" || (direction == MeasurementDirection::Rise ? target == "MAX" : target == "MIN");
            if (TestSequenceTaskKind::isDescent(TestSequenceTaskKind::fromName(record.taskName)))
                selected = direction == MeasurementDirection::Fall;
            if (record.taskName == "Brown Out") {
                const bool rising = record.settings.value("mode").toString() == "TRACK" && record.settings.value("direction").toString() == "RISE";
                selected = (direction == MeasurementDirection::Rise) == rising;
            }
            append(load, status, details + (selected ? "\nNo valid event in this direction" : "\nDirection not selected"),
                   nullptr, direction, selected ? "NA" : "Not run");
        }
        appendIssues();
    };
    const QString status = record.status.isEmpty() ? QStringLiteral("Running") : record.status;
    if (!TestSequenceTaskKind::isMeasurement(record.taskName)) {
        append({}, status, note, nullptr, MeasurementDirection::None);
    } else if (TestSequenceTaskKind::isGroup(TestSequenceTaskKind::fromName(record.taskName))) {
        if (record.conditions.isEmpty()) append({}, status, note, nullptr, MeasurementDirection::None, "NA");
        for (const auto &condition : record.conditions)
            appendMeasurements(loadDescription(condition.condition),
                               condition.status, note + "\n" + condition.error, condition.channels, condition.trials,
                               condition.trialIssues);
    } else {
        appendMeasurements(loadDescription(record.loadCondition), status, note, record.channels, record.trials,
                           record.trialIssues);
    }
    // Capture files belong to the task attempt, not to a Trial ID. List them
    // once so a long acquisition history does not repeat the entire file list.
    if (!output.isEmpty() && !record.files.isEmpty())
        output.front()[Note] = output.front()[Note].toString() +
            "\nTask capture files (not per Trial):\n" + record.files.join("; ");
    return output;
}
} // namespace TestSequenceResultRows
