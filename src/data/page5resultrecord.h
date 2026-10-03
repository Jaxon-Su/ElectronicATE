#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <limits>

enum class LoadConditionKind { Static, Dynamic };

struct Page5ConditionSource {
    int index = -1; // Page2 row, zero-based; -1 means a single-condition task.
    QString label;
    LoadConditionKind kind = LoadConditionKind::Dynamic;

    QString typeName() const { return kind == LoadConditionKind::Dynamic ? "Dy Load" : "Load"; }
};

struct Page5ChannelResult {
    int channel = 0;
    QString unit; // Empty means unavailable; never infer V for an unknown probe.
    double maximum = std::numeric_limits<double>::quiet_NaN();
    double minimum = std::numeric_limits<double>::quiet_NaN();
    double rms = std::numeric_limits<double>::quiet_NaN();
    double mean = std::numeric_limits<double>::quiet_NaN();
    Page5ConditionSource maximumSource;
    Page5ConditionSource minimumSource;
    Page5ConditionSource latestSource;
};

struct Page5ConditionResult {
    Page5ConditionSource condition;
    QString status = "Skipped";
    QString error;
    QVector<Page5ChannelResult> channels;
};

struct Page5ResultRecord {
    QString runId;
    int taskIndex = -1;
    int dutUid = -1;
    int attempt = 0; // 1-based attempts; 0 means preflight failure or not executed.
    QString taskName;
    QString externalName;
    QVariantMap settings;
    bool report = true;
    QDateTime startedAt;
    QDateTime finishedAt;
    QString status; // Pass / Fail / Cancelled / Skipped
    QString error;
    QString summary;
    QStringList files;
    QVector<Page5ChannelResult> channels;
    QVector<Page5ConditionResult> conditions;
};
Q_DECLARE_METATYPE(Page5ResultRecord)
Q_DECLARE_METATYPE(QVector<Page5ResultRecord>)
