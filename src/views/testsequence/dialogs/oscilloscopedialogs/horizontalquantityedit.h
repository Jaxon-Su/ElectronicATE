#pragma once
#include "oscilloscopesettingrules.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QWidget>
#include <functional>

// Numeric presets are shortcuts; a typed value is never replaced on focus loss.
// The separate unit selector changes the unit of the entered number.
class HorizontalQuantityEdit : public QWidget {
  public:
    HorizontalQuantityEdit(const QString &kind, const QStringList &presets,
                           const QString &saved, const QString &name)
        : m_kind(kind), m_presets(presets)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_number = new QComboBox;
        m_number->setObjectName(name);
        m_number->setEditable(true);
        m_number->setInsertPolicy(QComboBox::NoInsert);
        m_number->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_number->setMinimumContentsLength(8);
        m_unit = new QComboBox;
        m_unit->setObjectName(name + "Unit");
        if (kind == "time") addUnits({{"ps/div",1e-12},{"ns/div",1e-9},{"us/div",1e-6},{"ms/div",1e-3},{"s/div",1}});
        else if (kind == "rate") addUnits({{"S/s",1},{"kS/s",1e3},{"MS/s",1e6},{"GS/s",1e9}});
        else addUnits({{"points",1},{"k points",1e3},{"M points",1e6}});
        layout->addWidget(m_number, 1);
        layout->addWidget(m_unit);
        m_unit->setMinimumWidth(92);
        m_number->setToolTip("Choose a shortcut or type a number, then select its unit.");
        if (const auto value = OscilloscopeSettingRules::quantity(saved, kind)) setValue(*value);
        else { refreshPresets(); m_number->setEditText(saved); }
        connect(m_number, &QComboBox::currentTextChanged, this, [this] { notify(); });
        connect(m_unit, &QComboBox::currentIndexChanged, this, [this] { refreshPresets(); notify(); });
    }

    QString text() const
    {
        QString unit = m_unit->currentText();
        if (m_kind == "time") unit.remove("/div");
        if (m_kind == "count") unit = m_unit->currentIndex() == 0 ? "" : m_unit->currentIndex() == 1 ? "k" : "M";
        return m_number->currentText().trimmed() + unit;
    }
    std::optional<double> value() const { return OscilloscopeSettingRules::quantity(text(), m_kind); }
    void setValue(double value)
    {
        QSignalBlocker numberBlock(m_number), unitBlock(m_unit);
        int chosen = 0;
        for (int i = 0; i < m_unit->count(); ++i)
            if (value >= m_unit->itemData(i).toDouble() * (1 - 1e-12)) chosen = i;
        m_unit->setCurrentIndex(chosen);
        refreshPresets();
        m_number->setEditText(QString::number(value / m_unit->currentData().toDouble(), 'g', 15));
    }
    std::function<void()> changed;

  private:
    void addUnits(std::initializer_list<std::pair<const char *, double>> units)
    {
        for (const auto &unit : units) m_unit->addItem(unit.first, unit.second);
    }
    void refreshPresets()
    {
        QSignalBlocker blocker(m_number);
        const QString typed = m_number->currentText();
        m_number->clear();
        for (const auto &preset : m_presets) {
            const auto value = OscilloscopeSettingRules::quantity(preset, m_kind);
            if (!value) continue;
            const double scaled = *value / m_unit->currentData().toDouble();
            if (scaled >= 1 && scaled <= 1000) m_number->addItem(QString::number(scaled, 'g', 15));
        }
        m_number->setEditText(typed);
    }
    void notify() { if (changed) changed(); }
    QString m_kind;
    QStringList m_presets;
    QComboBox *m_number, *m_unit;
};
