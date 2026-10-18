#pragma once
#include "../inputselectors.h"
#include "../loadconditionselector.h"
#include "../strategycapturewidget.h"
#include "../dialogstyle.h"
#include "../relaydialogs/relaydialog.h"
#include "descentsettings.h"
#include "tasksettingrules.h"
#include <QDialog>
#include <QGroupBox>
#include <QHeaderView>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

class DescentDialog final : public QDialog {
public:
    DescentDialog(bool shortTest, const InputConditionOptions &inputs, const QStringList &loads,
                  const QVector<RelayOption> &relays, const QVariantMap &initial,
                  int sequence, QWidget *parent = nullptr)
        : QDialog(parent), m_shortTest(shortTest), m_relays(relays), m_cfg(initial)
    {
        const QString name = shortTest ? "Turn On then Short (descent)" : "Turn Off (descent)";
        setWindowTitle(QString("Row %1 — %2").arg(sequence).arg(name));
        setStyleSheet(TestSequenceDialogStyle::withTypography(R"(
            QDialog, QScrollArea, QWidget#descentBody { background: #f4f7fb; }
            QLabel { color: #2c3e50; background: transparent; }
            QLabel#titleLbl { color: #1a2a4a; }
            QLabel#badgeTurnOff, QLabel#badgeTurnOnShort { color: white; font-weight: bold;
                border-radius: 8px; padding: 2px 8px; }
            QLabel#badgeTurnOff { background: #e74c3c; }
            QLabel#badgeTurnOnShort { background: #2471a3; }
            QFrame#sep { color: #dce6f1; }
            QGroupBox { font-weight: bold; color: #2c3e6a; background: transparent;
                border: 1px solid #dce6f1; border-radius: 5px; margin-top: 8px; padding-top: 6px; }
            QGroupBox::title { subcontrol-origin: margin; left: 8px; top: 0px; padding: 0 4px; }
            QComboBox, QSpinBox, QTableWidget, QTableWidget QLineEdit { background: white; color: #1a2a4a;
                border: 1px solid #c8d8ee; border-radius: 3px; }
            QComboBox { min-height: 26px; min-width: 180px; padding: 3px 8px; }
            QComboBox:focus, QSpinBox:focus, QTableWidget QLineEdit:focus { border-color: #3a7bd5; }
            QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right;
                width: 22px; border-left: 1px solid #c8d8ee; }
            QComboBox QAbstractItemView { background: white; border: 1px solid #c8d8ee;
                selection-background-color: #e8f0fe; selection-color: #1a56db; }
            QSpinBox { min-height: 26px; padding: 2px 6px; padding-right: 20px; }
            QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 18px; height: 14px; }
            QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 18px; height: 14px; }
            QLabel#previewLbl { color: #8899bb; padding: 2px 0; }
            QLabel#hintLbl, QLabel#descentSummary { color: #8899bb; }
            QTableWidget { gridline-color: #dce6f1; selection-background-color: #e8f0fe; selection-color: #1a56db; }
            QHeaderView::section { background: #e8f0fe; color: #2c3e6a; padding: 6px; border: none; }
            QPushButton { background: white; color: #1a2a4a; border: 1px solid #c8d8ee;
                border-radius: 3px; padding: 5px 10px; }
            QPushButton:hover { background: #e8f0fe; }
            QPushButton#applyBtn, QPushButton#closeBtn { color: white; font-weight: bold;
                border: none; border-radius: 4px; padding: 6px 24px; }
            QPushButton#applyBtn { background: #5a8cdd; }
            QPushButton#applyBtn:hover { background: #4a7ccd; }
            QPushButton#applyBtn:pressed { background: #3a6cbb; }
            QPushButton#closeBtn { background: #3a7bd5; }
            QPushButton#closeBtn:hover { background: #4a8be5; }
            QPushButton#closeBtn:pressed { background: #2a6bc5; }
            QPushButton:disabled { color: #8899aa; }
            QPushButton#applyBtn:disabled { background: #dce6f1; color: #8993a7; }
            QLabel#descentError { color: #a52626; }
        )"));
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 16, 20, 16); root->setSpacing(10);
        auto *header = new QHBoxLayout;
        auto *title = new QLabel(name); title->setObjectName("titleLbl");
        auto *badge = new QLabel(shortTest ? "ON→SHORT" : "TURN OFF");
        badge->setObjectName(shortTest ? "badgeTurnOnShort" : "badgeTurnOff");
        header->addWidget(title); header->addStretch(); header->addWidget(badge); root->addLayout(header);
        auto *separator = new QFrame; separator->setObjectName("sep"); separator->setFrameShape(QFrame::HLine);
        root->addWidget(separator);
        auto *scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true);
        auto *body = new QWidget; body->setObjectName("descentBody");
        auto *layout = new QVBoxLayout(body);
        layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
        auto form = [&](const QString &label) {
            auto *group = new QGroupBox(label);
            auto *f = new QFormLayout(group);
            f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            f->setLabelAlignment(Qt::AlignRight); f->setHorizontalSpacing(16);
            f->setVerticalSpacing(12); layout->addWidget(group); return f;
        };
        auto *conditions = form("Settings");
        m_inputs = new InputSelectors(conditions, inputs, initial);
        m_load = new LoadConditionSelector(loads, "load", LoadConditionSelector::Selection::Single, this);
        m_load->restore(initial); conditions->addRow("Load :", m_load);
        auto number = [&](QFormLayout *f, const TaskSettingRules::Integer &rule, const QString &label, int fallback) {
            auto *spin = new QSpinBox;
            spin->setObjectName(rule.key); spin->setRange(rule.minimum, rule.maximum);
            spin->setValue(initial.value(rule.key, fallback).toInt());
            spin->setAccelerated(true);
            if (QString::fromLatin1(rule.key).endsWith("_ms")) spin->setSuffix(" ms");
            f->addRow(label, spin); m_numbers[rule.key] = spin; return spin;
        };
        if (shortTest) {
            m_short = relayCombo(initial.value("relay_index", -1).toInt());
            m_short->setObjectName("shortRelay"); conditions->addRow("Short :", m_short);
            m_discharge = relayCombo(initial.value("discharge_index", -1).toInt());
            m_discharge->setObjectName("dischargeRelay"); conditions->addRow("Discharge :", m_discharge);
        }
        auto *settle = number(conditions, TaskSettingRules::settle, "Settle time :", TaskSettingRules::settle.initial);
        settle->setSingleStep(1000); settle->setToolTip("每次開機後等待穩定，再啟動負緣觸發並執行關機或短路。");
        auto *trigger = form("Trigger");
        m_source = new QComboBox; m_source->setObjectName("triggerChannel");
        for (int ch = 1; ch <= 8; ++ch) m_source->addItem(QString("CH%1").arg(ch), ch);
        m_source->setCurrentIndex(m_source->findData(initial.value("trigger_channel", 4)));
        m_source->setToolTip("負緣觸發波形：選擇示波器已啟用的類比通道；執行前檢查該機型是否具備此通道。");
        trigger->addRow("Trigger CH:", m_source);
        auto *edge = new QComboBox; edge->setObjectName("triggerEdge");
        edge->addItem("Fall"); edge->setEnabled(false); edge->setToolTip("固定使用所選 Trigger CH 的負緣觸發。");
        trigger->addRow("Edge:", edge);
        auto *levels = form("Trigger levels");
        number(levels, TaskSettingRules::trials, "Trials per level:", 1);
        m_table = new QTableWidget(0, 3); m_table->setObjectName("triggerLevels");
        m_table->setHorizontalHeaderLabels({"Step", "Trigger level (V)", "Action"});
        m_table->verticalHeader()->hide(); m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        m_table->setColumnWidth(2, 84); m_table->setMinimumHeight(175);
        levels->addRow(m_table);
        auto *add = new QPushButton("Add level"); add->setObjectName("addLevel");
        levels->addRow(add);
        auto *hint = new QLabel("依列順序執行；每次重新上電及觸發。數值最多四位小數。");
        hint->setObjectName("hintLbl"); hint->setWordWrap(true); levels->addRow(hint);
        const auto initialLevels = initial.value("trigger_levels", DescentSettings::defaults().value("trigger_levels")).toList();
        for (const auto &level : initialLevels)
            appendLevel(DescentSettings::numeric(level) ? DescentSettings::levelText(level.toDouble()) : level.toString());
        auto *toggle = new QToolButton; toggle->setText("Advanced（進階設定）"); toggle->setObjectName("advancedToggle");
        toggle->setCheckable(true); toggle->setArrowType(Qt::RightArrow); toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        layout->addWidget(toggle);
        auto *advanced = form("Timing");
        number(advanced, TaskSettingRules::triggerTimeout, "Trigger timeout:", TaskSettingRules::triggerTimeout.initial)
            ->setToolTip("每次等待觸發／確認待命的上限。已觸發後可繼續等待擷取完成，受整組 Level 總時限限制。");
        number(advanced, TaskSettingRules::phaseTimeout, "Phase time limit:", TaskSettingRules::phaseTimeout.initial)
            ->setToolTip("整組 Level 的總時限，包含各次穩定、放電、量測與 Capture；超時回報失敗。");
        if (shortTest) number(advanced, TaskSettingRules::discharge, "Discharge time:", TaskSettingRules::discharge.initial)
            ->setToolTip("放電 Relay 保持啟用的時間；時間到後解除放電，再繼續測試。");
        advanced->parentWidget()->hide();
        connect(toggle, &QToolButton::toggled, this, [toggle, advanced](bool expanded) {
            advanced->parentWidget()->setVisible(expanded); toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        });
        m_capture = new StrategyCaptureWidget(initial, body); layout->addWidget(m_capture);
        m_summary = new QLabel; m_summary->setObjectName("descentSummary"); m_summary->setWordWrap(true); layout->addWidget(m_summary);
        m_error = new QLabel; m_error->setObjectName("descentError"); m_error->setWordWrap(true); layout->addWidget(m_error);
        layout->addStretch(); scroll->setWidget(body); root->addWidget(scroll, 1);
        auto *buttons = new QHBoxLayout; buttons->addStretch();
        m_apply = new QPushButton("Apply"); m_apply->setObjectName("applyBtn");
        auto *close = new QPushButton("Close"); close->setObjectName("closeBtn");
        m_apply->setFixedWidth(100); close->setFixedWidth(100);
        buttons->addWidget(m_apply); buttons->addWidget(close); root->addLayout(buttons);
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        connect(m_apply, &QPushButton::clicked, this, [this] { if (validateLevels()) { save(); accept(); } });
        connect(add, &QPushButton::clicked, this, [this] {
            if (m_table->rowCount() >= DescentSettings::maximumLevels) return;
            appendLevel({}); rebuildActions(); updateSummary();
            m_table->setCurrentCell(m_table->rowCount() - 1, 1); m_table->editItem(m_table->currentItem());
        });
        connect(m_table, &QTableWidget::cellChanged, this, [this] { updateSummary(); });
        connect(m_source, &QComboBox::currentIndexChanged, this, [this] { updateSummary(); });
        connect(m_numbers["trials_per_level"], &QSpinBox::valueChanged, this, [this] { updateSummary(); });
        rebuildActions(); updateSummary(); resize(680, 800); setMinimumSize(560, 560);
    }
    QVariantMap config() const { return m_cfg; }
private:
    QComboBox *relayCombo(int selected)
    {
        auto *combo = new QComboBox; combo->addItem("— None —", -1);
        for (int i = 0; i < m_relays.size(); ++i) combo->addItem(QString("%1.  %2").arg(i + 1).arg(m_relays[i].label), i);
        combo->setCurrentIndex(qMax(0, combo->findData(selected))); return combo;
    }
    void appendLevel(const QString &text)
    {
        QSignalBlocker block(m_table);
        const int row = m_table->rowCount(); m_table->insertRow(row);
        auto *order = new QTableWidgetItem(QString::number(row + 1)); order->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_table->setItem(row, 0, order); m_table->setItem(row, 1, new QTableWidgetItem(text)); m_table->setRowHeight(row, 38);
    }
    void rebuildActions()
    {
        QSignalBlocker block(m_table);
        for (int row = 0; row < m_table->rowCount(); ++row) {
            m_table->item(row, 0)->setText(QString::number(row + 1));
            auto *actions = new QWidget; auto *layout = new QHBoxLayout(actions); layout->setContentsMargins(2, 1, 2, 1);
            auto *button = new QPushButton("Delete"); button->setObjectName(QString("deleteLevel_%1").arg(row));
            layout->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, row] {
                { QSignalBlocker block(m_table); m_table->removeRow(row); }
                rebuildActions(); updateSummary();
            });
            m_table->setCellWidget(row, 2, actions);
        }
    }
    bool validateLevels() const
    {
        if (m_source->currentIndex() < 0 || m_table->rowCount() == 0 || m_table->rowCount() > DescentSettings::maximumLevels) return false;
        const QRegularExpression number("^[+-]?(?:[0-9]+(?:\\.[0-9]{0,4})?|\\.[0-9]{1,4})$");
        for (int row = 0; row < m_table->rowCount(); ++row) {
            const auto text = m_table->item(row, 1)->text().trimmed(); bool ok = false;
            const double value = text.toDouble(&ok);
            if (!number.match(text).hasMatch() || !ok || !DescentSettings::validLevel(value)) return false;
        }
        return true;
    }
    void updateSummary()
    {
        QStringList levels;
        for (int row = 0; row < m_table->rowCount(); ++row) levels << m_table->item(row, 1)->text().trimmed() + " V";
        m_summary->setText(QString("%1 · Fall · %2\n%3 levels × %4 trials").arg(m_source->currentText(), levels.join(" → "))
                               .arg(m_table->rowCount()).arg(m_numbers["trials_per_level"]->value()));
        const bool valid = validateLevels(); m_apply->setEnabled(valid);
        m_error->setText(valid ? QString() : "請選擇觸發 Channel，並輸入至少一組有效 Trigger level（最多四位小數）。");
    }
    void save()
    {
        m_inputs->save(m_cfg); m_load->save(m_cfg); m_capture->save(m_cfg);
        for (auto it = m_numbers.cbegin(); it != m_numbers.cend(); ++it) m_cfg[it.key()] = it.value()->value();
        m_cfg["trigger_channel"] = m_source->currentData(); QVariantList levels;
        for (int row = 0; row < m_table->rowCount(); ++row) levels.append(m_table->item(row, 1)->text().trimmed().toDouble());
        m_cfg["trigger_levels"] = levels;
        if (m_shortTest) for (const auto &entry : {qMakePair(QString("relay"), m_short), qMakePair(QString("discharge"), m_discharge)}) {
            const int index = entry.second->currentData().toInt(); m_cfg[entry.first + "_index"] = index;
            m_cfg[entry.first + "_label"] = index >= 0 ? m_relays[index].label : QString();
            if (index >= 0) {
                QStringList values; for (const auto &value : m_relays[index].values) values << value;
                m_cfg[entry.first + "_values"] = values;
            } else m_cfg.remove(entry.first + "_values");
        }
    }
    bool m_shortTest;
    QVector<RelayOption> m_relays;
    QVariantMap m_cfg;
    InputSelectors *m_inputs;
    LoadConditionSelector *m_load;
    StrategyCaptureWidget *m_capture;
    QComboBox *m_source, *m_short = nullptr, *m_discharge = nullptr;
    QMap<QString, QSpinBox *> m_numbers;
    QTableWidget *m_table;
    QLabel *m_summary, *m_error;
    QPushButton *m_apply;
};
