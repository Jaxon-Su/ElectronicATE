#pragma once
#include <QDialog>
#include "../loadconditionselector.h"
#include <QVariantMap>
#include <QStringList>
#include <QVector>
#include "relaydialog.h"

class TransientSettingsWidget;
class QComboBox;
class QSpinBox;

class TurnOnOffDialog : public QDialog {
    Q_OBJECT

  public:
    using Selection = LoadConditionSelector::Selection;
    enum Mode { TurnOn, TurnOff };

    explicit TurnOnOffDialog(Mode mode, const QStringList& inputOptions, const QStringList& loadOptions,
                             const QVector<RelayOption>& relayOptions, const QVariantMap& initCfg, int seqNo,
                             QWidget* parent = nullptr,
                             Selection selection = Selection::Single);

    QVariantMap config() const { return m_cfg; }

  private:
    void buildUI(const QStringList& inputOptions, const QStringList& loadOptions,
                 const QVector<RelayOption>& relayOptions);
    void restoreFromCfg();
    void saveConfig();

    Mode m_mode;
    int m_seqNo;
    QVector<RelayOption> m_relayOptions;
    QComboBox* m_inputCombo = nullptr;
    LoadConditionSelector* m_loadSelection = nullptr;
    QComboBox* m_dischargeCombo = nullptr; // TurnOn 專用
    QSpinBox* m_spinMs = nullptr;          // TurnOff 專用
    TransientSettingsWidget* m_triggerSettings = nullptr;
    QVariantMap m_cfg;
    Selection m_selection;
};
