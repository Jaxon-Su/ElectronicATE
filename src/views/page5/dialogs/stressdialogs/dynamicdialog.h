#pragma once
#include <QDialog>
#include <QVariantMap>
#include <QStringList>

class QComboBox;
class QSpinBox;

// ══════════════════════════════════════════════════════
//  DynamicDialog — Dynamic stress 設定視窗
//
//  與 StaticDialog 風格一致，Load 改為 Dynamic Load 選項
//
//  儲存格式 config()：
//    cfg["input_index"]   = int   (-1 = 不選)
//    cfg["input_label"]   = string
//    cfg["dyload_index"]  = int   (-1 = 不選)
//    cfg["dyload_label"]  = string
//    cfg["delay_ms"]      = int   (預設 5000)
// ══════════════════════════════════════════════════════
class DynamicDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DynamicDialog(const QStringList& inputOptions,
                           const QStringList& dyloadOptions,
                           const QVariantMap& initCfg,
                           int                seqNo,
                           QWidget*           parent = nullptr);

    QVariantMap config() const { return m_cfg; }

private:
    void buildUI(const QStringList& inputOptions,
                 const QStringList& dyloadOptions);
    void restoreFromCfg();
    void saveConfig();

    QComboBox*  m_inputCombo  = nullptr;
    QComboBox*  m_dyloadCombo = nullptr;
    QSpinBox*   m_spinMs      = nullptr;
    QVariantMap m_cfg;
};
