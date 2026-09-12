#pragma once
#include <QString>
#include <QVariantMap>
#include <QMetaType>
#include "page1config.h"
#include "page2config.h"
class Oscilloscope;

// ══════════════════════════════════════════════════════
//  RunTask — 執行單元
//
//  dutUid：列建立時分配的穩定唯一 ID
//          存在 QTableWidget col 0 的 Qt::UserRole
//          增刪其他列後此值絕對不變
//          Page5 在主執行緒透過 dutUid 取得設定，再傳給 Worker
// ══════════════════════════════════════════════════════
struct RunTask {
    int     dutUid = -1;   // 穩定 UID，非 row index
    QString name;
};

// ══════════════════════════════════════════════════════
//  TaskPayload — 每列任務設定；共用儀器/條件資料由 Page5ExecutionContext 提供
//
//  Worker 不持有 CenterPanel 指標（避免跨執行緒存取 UI）
//  所有設定在 Run 啟動時由 Page5 從主執行緒一次性預取
//
//  settings 依 task.name 而異：
//  ┌──────────────────┬────────────────────────────────┐
//  │ Delay            │ "delay_ms" (int, 毫秒)          │
//  │ Write Oscilloscope│ "timescale","acq_mode","ch1_…" │
//  │ Turn on/off      │ "input_index","load_index"     │
//  │ Relay            │ "relay_index","relay_label"    │
//
struct TaskPayload {
    RunTask     task;
    QVariantMap settings;
    QString     ext;
    int         retry  = 0;
    bool        report = true;
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
    Oscilloscope* scope = nullptr; // Existing externally-owned scope; no new ownership.
};
Q_DECLARE_METATYPE(Page5ExecutionContext)
