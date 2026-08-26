#include "createwriteoscilloscopedialog.h"
#include "oscshareddata.h"

// ── 已知型號的 Dialog 類別 ──────────────────────────
#include "mso44bwritedialog.h"    // MSO 4/5/6 系列（MSO44B / MSO56B…）
#include "dpo7000writedialog.h"   // DPO7000 / DPO5000 系列
#include "dpo4000writedialog.h"   // DPO4000 / MSO4000 舊款系列
#include "genericoscwritedialog.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QVariant>

// ★ 新增示波器型號時：
//   1. 在此 include 新的 Dialog 標頭
//   2. 在 createWriteOscilloscopeDialog() 加對應的 if（注意判斷順序）
//   3. 在 OscSharedData 新增 is___Family() 函式

// ══════════════════════════════════════════════════════
//  NotConfiguredDialog — 內嵌於此檔
//
//  此 Dialog 僅顯示「請先到 Instruments 配置示波器」提示，
//  沒有任何設定 UI，不需獨立成檔。
// ══════════════════════════════════════════════════════
class NotConfiguredDialog : public OscWriteDialogBase
{
public:
    explicit NotConfiguredDialog(QWidget* parent)
        : OscWriteDialogBase(nullptr, {}, {}, 0, {}, parent)
    {
        setWindowTitle("Write Oscilloscope — Not Configured");
        buildFrame({420, 240}, false);   // 無設定 → 只有 Close
    }

protected:
    QWidget* buildContentWidget() override
    {
        auto* card = new QFrame;
        card->setObjectName("card");
        auto* vlay = new QVBoxLayout(card);
        vlay->setAlignment(Qt::AlignCenter);
        vlay->setSpacing(14);

        auto* icon = new QLabel("🔭");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("font-size:38px; background:transparent;");

        auto* title = new QLabel("No Oscilloscope Configured");
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(
            "font-size:13px; font-weight:bold; color:#2a3a5a; background:transparent;");

        auto* msg = new QLabel(
            "Please go to the  Instruments  page,\n"
            "select and configure an oscilloscope (e.g. MSO44B / DPO7000)\n"
            "before using Write Oscilloscope.");
        msg->setAlignment(Qt::AlignCenter);
        msg->setStyleSheet(
            "font-size:11px; color:#6b7a99; font-style:italic; background:transparent;");

        vlay->addWidget(icon);
        vlay->addWidget(title);
        vlay->addWidget(msg);
        return card;
    }

    // saveConfig() 不覆寫：使用基底類別的空實作（無設定可存）
};

// ══════════════════════════════════════════════════════
//  createWriteOscilloscopeDialog — 簡單工廠函式
//
//  ★★★ 新增示波器型號時，只需修改此函式 ★★★
//
//  判斷順序（由上到下，第一個符合的優先）：
//    1. 未配置                      → NotConfiguredDialog
//    2. MSO 4/5/6 系列              → MSO44BWriteDialog
//    3. DPO7000 / DPO5000 系列      → DPO7000WriteDialog
//    4. DPO4000 / MSO4000 舊款系列  → DPO4000WriteDialog
//    5. 其他未知型號                → GenericOscWriteDialog
//
//  ⚠ 順序注意：isMSO456Family 必須在 isDPO7000Family 之前，
//    因為 DPO7000 舊版曾以 "MSO" 字串作為判斷條件之一。
// ══════════════════════════════════════════════════════
OscWriteDialogBase* createWriteOscilloscopeDialog(
    Oscilloscope*      scope,
    const QString&     configuredModel,
    const QVariantMap& initCfg,
    int                seqNo,
    const QString&     extName,
    QWidget*           parent)
{
    // ── 狀態 A：未在 Instruments 頁配置任何示波器 ──
    if (configuredModel.isEmpty())
        return new NotConfiguredDialog(parent);

    // ── 狀態 B / C：根據型號選擇具體 Dialog ──────
    // （不管有沒有連線，Dialog 類別都相同；
    //   Banner 由基底類別根據 scope 是否為 nullptr 決定）

    // MSO 4/5/6 系列（MSO44B / MSO46B / MSO54B / MSO56B / MSO58B…）
    // ⚠ 必須先於 isDPO7000Family 判斷
    if (OscSharedData::isMSO456Family(configuredModel))
        return new MSO44BWriteDialog(scope, configuredModel, initCfg, seqNo, extName, parent);

    // DPO7000 / DPO5000 系列
    if (OscSharedData::isDPO7000Family(configuredModel))
        return new DPO7000WriteDialog(scope, configuredModel, initCfg, seqNo, extName, parent);

    // DPO4000 / MSO4000 舊款系列
    if (OscSharedData::isDPO4000Family(configuredModel))
        return new DPO4000WriteDialog(scope, configuredModel, initCfg, seqNo, extName, parent);

    // ★ 未來新增示波器範例（取消下方註解）：
    // if (OscSharedData::isRigolFamily(configuredModel))
    //     return new RigolWriteDialog(scope, configuredModel, initCfg, seqNo, extName, parent);

    // ── 後備：已配置但型號無專屬 UI ──
    return new GenericOscWriteDialog(scope, configuredModel, initCfg, seqNo, extName, parent);
}
