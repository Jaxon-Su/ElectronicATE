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

class ShortTestDialog : public QDialog {
    Q_OBJECT

  public:
    using Selection = LoadConditionSelector::Selection;
    enum Mode { ShortThenTurnOn, TurnOnThenShort };

    explicit ShortTestDialog(Mode mode, const QStringList& inputOptions, const QStringList& loadOptions,
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
    QVector<RelayOption> m_relayOptions;
    QComboBox* m_inputCombo = nullptr;
    LoadConditionSelector* m_loadSelection = nullptr;
    QComboBox* m_relayCombo = nullptr;     // Short
    QComboBox* m_dischargeCombo = nullptr; // Discharge
    QSpinBox* m_spinMs = nullptr;
    TransientSettingsWidget* m_triggerSettings = nullptr;
    QVariantMap m_cfg;
    Selection m_selection;
};
