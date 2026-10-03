#pragma once
#include "../strategycapturewidget.h"
#include "tasksettingrules.h"
#include <QCheckBox>
#include <QWidget>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QVariantMap>
#include <QLabel>

// Shared controls for all four transient test dialogs; QVariantMap is persisted by Page5.
class TransientSettingsWidget : public QWidget {
  public:
    TransientSettingsWidget(const QVariantMap& cfg, bool discharge, bool single, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* form = new QFormLayout(this);
        auto addTimeHelp = [form](QWidget* field, const QString& text) {
            field->setToolTip(text);
            auto* help = new QLabel(text);
            help->setWordWrap(true);
            help->setStyleSheet("color: #526580;");
            form->addRow("", help);
        };
        target = new QComboBox;
        target->addItem("BOTH", "BOTH");
        target->addItem("Rise", "MAX");
        target->addItem("Fall", "MIN");
        target->setCurrentIndex(qMax(0, target->findData(cfg.value("measure_target", "BOTH").toString().trimmed().toUpper())));
        target->setObjectName("measureTarget");
        target->setToolTip("BOTH: Rise then Fall. Rise searches Max; Fall searches Min. Unselected extremum is NA.");
        form->addRow("Search direction:", target);
        edge = new QComboBox;
        edge->addItem(QString::fromUtf8("BOTH(Rise、Fall)"), "AUTO");
        edge->setObjectName("triggerEdge");
        edge->setEnabled(false);
        edge->setToolTip("Separate phases: rising for Max, falling for Min.");
        auto updateEdge = [this] {
            edge->setItemText(0, target->currentData().toString() == "MAX" ? "Rise" :
                                target->currentData().toString() == "MIN" ? "Fall" : "BOTH(Rise, Fall)");
        };
        connect(target, &QComboBox::currentTextChanged, this, updateEdge);
        updateEdge();
        form->addRow("Edge:", edge);
        capture = new StrategyCaptureWidget(cfg, this);
        form->addRow("", capture);
        timeout = new QSpinBox;
        timeout->setRange(TaskSettingRules::triggerTimeout.minimum, TaskSettingRules::triggerTimeout.maximum);
        timeout->setSuffix(" ms");
        timeout->setValue(cfg.value(TaskSettingRules::triggerTimeout.key, TaskSettingRules::triggerTimeout.initial).toInt());
        form->addRow("Trigger timeout:", timeout);
        addTimeHelp(timeout, "每次等待觸發／確認待命的上限。已觸發後可繼續等待擷取完成，受階段總時限限制。");
        Q_UNUSED(single);
        form->addRow("Initial search step:", new QLabel("Auto: trigger CH Scale × 0.04"));
        auto* tolerance = new QLabel("Auto: trigger CH Scale / 25");
        tolerance->setObjectName("searchTolerance");
        form->addRow("Search tolerance:", tolerance);
        trials = new QSpinBox;
        trials->setRange(TaskSettingRules::trials.minimum, TaskSettingRules::trials.maximum);
        trials->setObjectName("trialsPerLevel");
        trials->setValue(cfg.value(TaskSettingRules::trials.key, TaskSettingRules::trials.initial).toInt());
        form->addRow("Trials per level:", trials);
        phase = new QSpinBox;
        phase->setRange(TaskSettingRules::phaseTimeout.minimum, TaskSettingRules::phaseTimeout.maximum);
        phase->setSuffix(" ms");
        phase->setValue(cfg.value(TaskSettingRules::phaseTimeout.key, TaskSettingRules::phaseTimeout.initial).toInt());
        form->addRow("Phase time limit:", phase);
        addTimeHelp(phase, "每個 Rise／Fall 階段各自的總時限，包含穩定、放電、重試與擷取；超時回報失敗。");
        if (discharge) {
            dischargeTime = new QSpinBox;
            dischargeTime->setRange(TaskSettingRules::discharge.minimum, TaskSettingRules::discharge.maximum);
            dischargeTime->setSuffix(" ms");
            dischargeTime->setValue(cfg.value(TaskSettingRules::discharge.key, TaskSettingRules::discharge.initial).toInt());
            form->addRow("Discharge time:", dischargeTime);
            addTimeHelp(dischargeTime, "放電 Relay 保持啟用的時間；時間到後解除放電，再繼續測試。");
        }
    }
    void save(QVariantMap& cfg) const
    {
        capture->save(cfg);
        cfg["measure_target"] = target->currentData().toString();
        cfg["trigger_edge"] = edge->currentData().toString();
        cfg["trig_timeout_ms"] = timeout->value();
        cfg["phase_timeout_ms"] = phase->value();
        cfg["trials_per_level"] = trials->value();
        if (dischargeTime)
            cfg["discharge_ms"] = dischargeTime->value();
    }

  private:
    StrategyCaptureWidget* capture = nullptr;
    QComboBox *target, *edge;
    QSpinBox *timeout, *phase, *trials, *dischargeTime = nullptr;
};
