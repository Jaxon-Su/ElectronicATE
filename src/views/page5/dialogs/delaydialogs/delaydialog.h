#pragma once
#include <QDialog>
#include <QVariantMap>

class QSpinBox;

// ══════════════════════════════════════════════════════
//  DelayDialog — 延遲時間設定視窗
//
//  UI：
//    - 時間 SpinBox（單位 ms，步進 1000ms，預設 5000ms）
//    - 支援滾輪、手動輸入、SpinBox 箭頭
//    - Apply 儲存並關閉 / Close 儲存並關閉
//
//  使用方式：
//    DelayDialog dlg(initCfg, seqNo, parent);
//    if (dlg.exec() == QDialog::Accepted)
//        QVariantMap cfg = dlg.config();   // cfg["delay_ms"] = int
// ══════════════════════════════════════════════════════
class DelayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DelayDialog(const QVariantMap& initCfg,
                         int                seqNo,
                         QWidget*           parent = nullptr);

    QVariantMap config() const { return m_cfg; }

private:
    void buildUI();
    void saveConfig();

    QSpinBox*   m_spinMs = nullptr;
    QVariantMap m_cfg;
};
