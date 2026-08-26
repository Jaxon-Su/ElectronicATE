#pragma once
#include "oscwritedialogbase.h"

class Oscilloscope;
class QWidget;

// ══════════════════════════════════════════════════════
//  createWriteOscilloscopeDialog — 簡單工廠函式
//
//  唯一職責：根據 configuredModel 選擇並建立正確的 Dialog。
//
//  ★ 新增示波器型號時，唯一需要修改的地方：
//    1. 新建 XxxWriteDialog.h/.cpp（繼承 OscWriteDialogBase）
//    2. 在 createwriteoscilloscopedialog.cpp 的函式本體加一個 if
//    ─────────────────────────────────────────────────────
//    此 .h 不需修改
//    page5centerpanel.cpp 不需修改
//    現有 Dialog 類別不需修改（開放/封閉原則）
//
//  傳入狀態 → 建立的 Dialog：
//  ┌──────────────────┬──────────────────┬──────────────────────────┐
//  │ configuredModel  │ scope            │ 建立的 Dialog            │
//  ├──────────────────┼──────────────────┼──────────────────────────┤
//  │ ""（空）          │ nullptr          │ NotConfiguredDialog      │
//  │ "DPO7000"        │ nullptr          │ DPO7000WriteDialog       │
//  │ "DPO7000"        │ 有效指標         │ DPO7000WriteDialog（已連線）│
//  │ "Unknown"        │ any              │ GenericOscWriteDialog    │
//  └──────────────────┴──────────────────┴──────────────────────────┘
// ══════════════════════════════════════════════════════
OscWriteDialogBase* createWriteOscilloscopeDialog(
    Oscilloscope*      scope,
    const QString&     configuredModel,
    const QVariantMap& initCfg,
    int                seqNo,
    const QString&     extName,
    QWidget*           parent = nullptr);
