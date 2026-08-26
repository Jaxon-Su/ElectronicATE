#pragma once
#include <QDialog>
#include <QVariantMap>
#include <QStringList>
#include <QVector>
#include "relaydialog.h"

class QComboBox;
class QSpinBox;

// ══════════════════════════════════════════════════════
//  ShortTestDialog — Short then Turn On / Turn On then Short 共用視窗
//
//  儲存格式 config()：
//    cfg["input_index"]      = int   (-1 = 不選)
//    cfg["input_label"]      = string
//    cfg["load_index"]       = int   (-1 = 不選)
//    cfg["load_label"]       = string
//    cfg["relay_index"]      = int   (-1 = 不選)   (Short RL)
//    cfg["relay_label"]      = string
//    cfg["relay_values"]     = QStringList
//    cfg["discharge_index"]  = int   (-1 = 不選)
//    cfg["discharge_label"]  = string
//    cfg["discharge_values"] = QStringList
//    cfg["delay_ms"]         = int   (預設 5000)
// ══════════════════════════════════════════════════════
class ShortTestDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { ShortThenTurnOn, TurnOnThenShort };

    explicit ShortTestDialog(Mode                        mode,
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
    QVector<RelayOption> m_relayOptions;
    QComboBox*           m_inputCombo     = nullptr;
    QComboBox*           m_loadCombo      = nullptr;
    QComboBox*           m_relayCombo     = nullptr;   // Short
    QComboBox*           m_dischargeCombo = nullptr;   // Discharge
    QSpinBox*            m_spinMs         = nullptr;
    QVariantMap          m_cfg;
};
