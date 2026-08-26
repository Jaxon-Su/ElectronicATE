#pragma once
#include <QDialog>
#include <QVariantMap>
#include <QStringList>

class QComboBox;
class QSpinBox;

// ══════════════════════════════════════════════════════
//  StaticDialog — Static stress 設定視窗
//
//  與 TurnOnOffDialog 風格一致：
//    Input 選項、Load 選項、持續時間
//
//  儲存格式 config()：
//    cfg["input_index"] = int   (-1 = 不選)
//    cfg["input_label"] = string
//    cfg["load_index"]  = int   (-1 = 不選)
//    cfg["load_label"]  = string
//    cfg["delay_ms"]    = int   (預設 5000)
// ══════════════════════════════════════════════════════
class StaticDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StaticDialog(const QStringList& inputOptions,
                          const QStringList& loadOptions,
                          const QVariantMap& initCfg,
                          int                seqNo,
                          QWidget*           parent = nullptr);

    QVariantMap config() const { return m_cfg; }

private:
    void buildUI(const QStringList& inputOptions,
                 const QStringList& loadOptions);
    void restoreFromCfg();
    void saveConfig();

    QComboBox*  m_inputCombo = nullptr;
    QComboBox*  m_loadCombo  = nullptr;
    QSpinBox*   m_spinMs     = nullptr;
    QVariantMap m_cfg;
};
