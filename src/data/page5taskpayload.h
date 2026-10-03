#pragma once
#include <QString>
#include <QVariantMap>
#include <QMetaType>
#include "page1config.h"
#include "loadsyncsettings.h"
#include "page2config.h"
class Oscilloscope;

// Stable runtime task identity; independent of table row position.
struct RunTask {
    int dutUid = -1; // 穩定 UID，非 row index
    QString name;
};

// Immutable draft snapshot produced by the ViewModel. The execution service decodes
// settings into TaskSettings value types; QVariantMap remains the XML/dialog boundary.
struct TaskPayload {
    RunTask task;
    QVariantMap settings;
    QString ext;
    int retry = 0;
    bool report = true;
};

Q_DECLARE_METATYPE(RunTask)
Q_DECLARE_METATYPE(TaskPayload)
Q_DECLARE_METATYPE(QVector<RunTask>)
Q_DECLARE_METATYPE(QVector<TaskPayload>)

// Captured on the GUI thread before dispatch; Worker never reads live models.
struct Page5ExecutionContext {
    Page1Config page1;
    QVector<InputRow> inputs;
    LoadMetaRow loadMeta;
    QVector<LoadDataRow> loads;
    DynamicMetaRow dynamicMeta;
    QVector<DynamicDataRow> dynamics;
    QVector<RelayDataRow> relays;
    Oscilloscope *scope = nullptr; // Existing externally-owned scope; no new ownership.
    LoadSyncSettings loadSync;
    QVector<DcGroup> dcInputs;
    QVector<QString> dcLabels;
    QString reportDirectory;
};
Q_DECLARE_METATYPE(Page5ExecutionContext)
