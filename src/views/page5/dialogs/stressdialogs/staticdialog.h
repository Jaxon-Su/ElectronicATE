#pragma once
#include <QDialog>
#include "../loadconditionselector.h"
#include <QVariantMap>
#include <QStringList>

class StrategyCaptureWidget;
class QComboBox;
class QCheckBox;
class QSpinBox;

class StaticDialog : public QDialog {
    Q_OBJECT

  public:
    using Selection = LoadConditionSelector::Selection;
    explicit StaticDialog(const QStringList& inputOptions, const QStringList& loadOptions,
                          const QVariantMap& initCfg, int seqNo, QWidget* parent = nullptr,
                             Selection selection = Selection::Single);

    QVariantMap config() const { return m_cfg; }

  private:
    void buildUI(const QStringList& inputOptions, const QStringList& loadOptions);
    void restoreFromCfg();
    void saveConfig();

    QComboBox* m_inputCombo = nullptr;
    LoadConditionSelector* m_loadSelection = nullptr;
    StrategyCaptureWidget* m_capture = nullptr;
    QCheckBox* m_autoPeriod = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QSpinBox* m_spinMs = nullptr;
    QVariantMap m_cfg;
    Selection m_selection;
};
