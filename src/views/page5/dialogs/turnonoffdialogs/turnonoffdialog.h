#pragma once
#include <QDialog>
#include <QVariantMap>
#include <QStringList>
#include <QVector>
#include "relaydialog.h"

class QComboBox;
class QSpinBox;

// ══════════════════════════════════════════════════════
//  TurnOnOffDialog — Turn On / Turn Off 共用設定視窗
//
//  模式由 Mode enum 決定標題與 badge 顏色
//
//  ┌─────────────────────────────────────────────────┐
//  │  資料傳入（由 CenterPanel 從 ViewModel 取得）      │
//  │    inputOptions   → ["0/30/30", "90/47/0", ...]  │
//  │    loadOptions    → ["Test1", "Test2", ...]       │
//  │    relayOptions   → [{label,values}, ...]  (TurnOn)│
//  │                                                  │
//  │  儲存格式 config()：                              │
//  │    cfg["input_index"]      = int   (-1 = 不選)   │
//  │    cfg["input_label"]      = string              │
//  │    cfg["load_index"]       = int   (-1 = 不選)   │
//  │    cfg["load_label"]       = string              │
//  │    cfg["discharge_index"]  = int   (-1 = 不選)   │  TurnOn
//  │    cfg["discharge_label"]  = string              │  TurnOn
//  │    cfg["discharge_values"] = QStringList         │  TurnOn
//  └─────────────────────────────────────────────────┘
// ══════════════════════════════════════════════════════
class TurnOnOffDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { TurnOn, TurnOff };

    explicit TurnOnOffDialog(Mode                        mode,
                             const QStringList&          inputOptions,
                             const QStringList&          loadOptions,
                             const QVector<RelayOption>& relayOptions,
                             const QVariantMap&          initCfg,
                             int                         seqNo,
                             QWidget*                    parent = nullptr);

    QVariantMap config() const { return m_cfg; }

private:
    void buildUI(const QStringList&          inputOptions,
                 const QStringList&          loadOptions,
                 const QVector<RelayOption>& relayOptions);
    void restoreFromCfg();
    void saveConfig();

    Mode                 m_mode;
    int                  m_seqNo;
    QVector<RelayOption> m_relayOptions;
    QComboBox*           m_inputCombo     = nullptr;
    QComboBox*           m_loadCombo      = nullptr;
    QComboBox*           m_dischargeCombo = nullptr;  // TurnOn 專用
    QSpinBox*            m_spinMs         = nullptr;  // TurnOff 專用
    QVariantMap          m_cfg;
};
