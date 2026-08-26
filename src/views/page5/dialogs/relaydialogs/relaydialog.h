#pragma once
#include <QDialog>
#include <QVariantMap>
#include <QVector>
#include <QString>
#include <QStringList>

class QComboBox;

// ══════════════════════════════════════════════════════
//  RelayOption — 傳入 Dialog 的每一列 Relay 資料
// ══════════════════════════════════════════════════════
struct RelayOption {
    QString          label;   // e.g. "RL1"
    QVector<QString> values;  // e.g. ["on","off","off","on"]
};

// ══════════════════════════════════════════════════════
//  RelayDialog — Relay 設定視窗
//
//  ┌─────────────────────────────────────────────────┐
//  │  資料傳入（由 CenterPanel 從 ViewModel 取得）      │
//  │    relayOptions → [{label:"RL1", values:[...]},  │
//  │                    {label:"RL2", values:[...]}]  │
//  │                                                  │
//  │  儲存格式 config()：                              │
//  │    cfg["relay_index"]  = int   (-1 = 不選)       │
//  │    cfg["relay_label"]  = string  e.g. "RL1"     │
//  │    cfg["relay_values"] = QStringList             │
//  └─────────────────────────────────────────────────┘
// ══════════════════════════════════════════════════════
class RelayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RelayDialog(const QVector<RelayOption>& relayOptions,
                         const QVariantMap&           initCfg,
                         int                          seqNo,
                         QWidget*                     parent = nullptr);

    QVariantMap config() const { return m_cfg; }

private:
    void buildUI();
    void updatePreviewTable(int relayIdx);
    void restoreFromCfg();
    void saveConfig();

    int                    m_seqNo;
    QVector<RelayOption>   m_relayOptions;
    QComboBox*             m_relayCombo = nullptr;
    QVariantMap            m_cfg;
};
