#pragma once
#include "tasksettingrules.h"
#include <QComboBox>
#include <QVariantMap>

// Missing legacy settings intentionally preserve the instrument's current source.
class TriggerSourceSelector : public QComboBox {
public:
    explicit TriggerSourceSelector(const QVariantMap& cfg, QWidget* parent = nullptr)
        : QComboBox(parent), m_keepCurrent(!cfg.contains(TaskSettingRules::triggerChannel.key))
    {
        setObjectName("triggerSource");
        setToolTip("Apply this trigger source before the test starts. The selected channel must be enabled.");
        const auto& rule = TaskSettingRules::triggerChannel;
        const auto value = cfg.value(rule.key);
        const auto type = value.metaType().id();
        const bool numeric = type == QMetaType::Int || type == QMetaType::UInt || type == QMetaType::LongLong ||
                             type == QMetaType::ULongLong || type == QMetaType::Double || type == QMetaType::Float;
        const bool valid = numeric && rule.accepts(value.toDouble());
        if (!cfg.contains(rule.key))
            addItem("Keep current source", QVariant{});
        else if (!valid)
            addItem(QString("Invalid source (%1) — select CH").arg(value.toString()), value);
        for (int ch = rule.minimum; ch <= rule.maximum; ++ch)
            addItem(QString("CH%1").arg(ch), ch);
        if (valid) setCurrentIndex(findData(value.toInt()));
    }
    void save(QVariantMap& cfg) const
    {
        if (m_keepCurrent && currentIndex() == 0) cfg.remove(TaskSettingRules::triggerChannel.key);
        else cfg[TaskSettingRules::triggerChannel.key] = currentData();
    }
private:
    bool m_keepCurrent;
};
