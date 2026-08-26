#pragma once
#include <QDialog>
#include <QVariantMap>
#include <QSize>

class Oscilloscope;
class QComboBox;

// ══════════════════════════════════════════════════════
//  OscWriteDialogBase — 抽象基底類別
//
//  負責所有子類共用的 UI 框架：
//    - 標題列 + 型號 badge
//    - 連線狀態副標題 / 未連線黃色 banner
//    - 分隔線
//    - Apply + Close 按鈕列（hasSettings=false 時僅 Close）
//    - 共用樣式表
//
//  子類職責：
//    - 實作 buildContentWidget()  → 返回設定 UI 主體
//    - 實作 saveConfig()          → 將 UI 狀態讀回 m_cfg
//    - 在 constructor 末尾呼叫 buildFrame(minSize, hasSettings)
//
//  ★ C++ 虛函式規則：
//    buildFrame() 會呼叫 buildContentWidget()（虛函式）。
//    必須從「子類」constructor 末尾呼叫 buildFrame()，
//    絕不可在此基底類別 constructor 中呼叫。
//    這樣 virtual dispatch 才能正確解析到子類實作。
//
//  ★ 新增示波器型號：
//    1. 繼承 OscWriteDialogBase
//    2. 實作 buildContentWidget() 與 saveConfig()
//    3. 在 writeoscilloscopedialog.cpp 工廠加一個 else if
//    4. 其餘現有程式碼不需修改
// ══════════════════════════════════════════════════════
class OscWriteDialogBase : public QDialog
{
    Q_OBJECT

public:
    explicit OscWriteDialogBase(Oscilloscope*      scope,
                                const QString&     configuredModel,
                                const QVariantMap& initCfg,
                                int                seqNo,
                                const QString&     extName,
                                QWidget*           parent = nullptr);

    // 取得目前設定（exec() 結束後有效）
    QVariantMap config() const { return m_cfg; }

protected:
    // ── 子類必須實作 ──────────────────────────────────
    // 返回設定 UI 主體（由 buildFrame 嵌入 layout）
    virtual QWidget* buildContentWidget() = 0;

    // 將 UI 狀態讀回 m_cfg；NotConfigured 等不需存設定的子類可不覆寫
    virtual void saveConfig() {}

    // ── 子類在 constructor 末尾呼叫 ──────────────────
    // hasSettings=true  → 顯示 Apply + Close
    // hasSettings=false → 僅顯示 Close（無設定可儲存）
    void buildFrame(const QSize& minSize, bool hasSettings = true);

    // ── 共用輔助 ──────────────────────────────────────
    static QComboBox* makeCombo(const QStringList& items, const QString& current);

    // ── 子類可直接存取的狀態 ──────────────────────────
    Oscilloscope* m_scope          = nullptr;
    QString       m_configuredModel;   // 來自 page1Config，空 = 未配置
    QString       m_model;             // 已連線 = scope->model()；未連線 = configuredModel
    QVariantMap   m_cfg;
};
