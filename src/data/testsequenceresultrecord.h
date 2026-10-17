#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <limits>
#include "measurementtrial.h"

enum class LoadConditionKind { Static, Dynamic };

struct TestSequenceConditionSource {
    int index = -1; // TestConditions row, zero-based; -1 means the source is unavailable.
    QString label;
    LoadConditionKind kind = LoadConditionKind::Dynamic;

    QString typeName() const { return kind == LoadConditionKind::Dynamic ? "Dy Load" : "Load"; }
};

struct TestSequenceChannelResult {
    int channel = 0;
    QString unit; // Empty means unavailable; never infer V for an unknown probe.
    double maximum = std::numeric_limits<double>::quiet_NaN();
    double minimum = std::numeric_limits<double>::quiet_NaN();
    double rms = std::numeric_limits<double>::quiet_NaN();
    double mean = std::numeric_limits<double>::quiet_NaN();
    MeasurementDirection direction = MeasurementDirection::None;
    int sourceSequence = 0;
    int step = 0;
    int trial = 0;
    double triggerLevel = std::numeric_limits<double>::quiet_NaN();
};

struct TestSequenceConditionResult {
    TestSequenceConditionSource condition;
    QString status = "Skipped";
    QString error;
    QVector<TestSequenceChannelResult> channels;
    QVector<MeasurementTrial> trials;
};

struct TestSequenceResultRecord {
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
    QVector<TestSequenceChannelResult> channels; // Single results; all Groups keep measurements in conditions instead.
    QVector<TestSequenceConditionResult> conditions; // Independent per-load results, including partial/Skipped members.
    QVector<MeasurementTrial> trials;
    TestSequenceConditionSource loadCondition; // Single-task source copied from the execution snapshot.
};
Q_DECLARE_METATYPE(TestSequenceResultRecord)
Q_DECLARE_METATYPE(QVector<TestSequenceResultRecord>)
