#pragma once
#include "../verticalmargincheckbox.h"
#include <QDialog>
#include "../inputselectors.h"
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
    explicit StaticDialog(const InputConditionOptions& inputOptions, const QStringList& loadOptions,
                          const QVariantMap& initCfg, int seqNo, QWidget* parent = nullptr,
                             Selection selection = Selection::Single);

    QVariantMap config() const { return m_cfg; }

  private:
    void buildUI(const InputConditionOptions& inputOptions, const QStringList& loadOptions);
    void restoreFromCfg();
    void saveConfig();

    InputSelectors* m_inputs = nullptr;
    LoadConditionSelector* m_loadSelection = nullptr;
    StrategyCaptureWidget* m_capture = nullptr;
    QCheckBox* m_autoPeriod = nullptr;
    VerticalMarginCheckBox* m_verticalMargin = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QSpinBox* m_powerOffMs = nullptr;
    QSpinBox* m_startupMs = nullptr;
    QSpinBox* m_spinMs = nullptr;
    QVariantMap m_cfg;
    Selection m_selection;
};
