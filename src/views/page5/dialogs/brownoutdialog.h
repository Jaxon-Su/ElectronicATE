#pragma once
#include "brownoutsettings.h"
#include "strategycapturewidget.h"
#include "dialogstyle.h"
#include "dialognumberformat.h"
#include "inputselectors.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QCheckBox>

class BrownOutDialog final : public QDialog
{
  public:
    BrownOutDialog(const InputConditionOptions &inputs, const QStringList &loads, const QVariantMap &initial,
                   int sequence, QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(QString("Row %1 — Brown Out").arg(sequence));
        setStyleSheet(Page5DialogStyle::withTypography(R"(
            QDialog, QScrollArea, QWidget#body { background: #f4f7fb; }
            QGroupBox { border: 1px solid #dce6f1; border-radius: 5px; margin-top: 12px; padding-top: 12px; }
            QGroupBox::title { subcontrol-origin: margin; left: 10px; color: #2c3e6a; }
            QComboBox, QDoubleSpinBox { background: white; color: #1a2a4a; min-height: 26px;
                border: 1px solid #c8d8ee; border-radius: 3px; padding: 3px 6px; }
            QLabel { color: #2c3e50; }
            QPushButton { min-width: 90px; padding: 6px; }
            QPushButton#applyBtn, QPushButton#closeBtn { background: #3a7bd5; color: white;
                border: none; border-radius: 4px; padding: 8px 18px; font-weight: bold; }
        )"));
        auto *root = new QVBoxLayout(this);
        auto *title = new QLabel("Brown Out");
        QFont titleFont = title->font(); titleFont.setBold(true); title->setFont(titleFont);
        root->addWidget(title);
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        auto *body = new QWidget; body->setObjectName("body");
        auto *layout = new QVBoxLayout(body);
        auto form = [&](const QString &name) {
            auto *group = new QGroupBox(name);
            auto *result = new QFormLayout(group);
            result->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            result->setVerticalSpacing(9); layout->addWidget(group); return result;
        };
        auto *conditions = form("Input / Load");
        m_inputs = new InputSelectors(conditions, inputs, initial);
        m_load = selection(loads, initial.value("load_index", -1).toInt());
        m_load->setObjectName("loadCombo"); conditions->addRow("Load:", m_load);
        auto addNumber = [&](QFormLayout *target, const char *key, const QString &label, const QString &unit = {}) {
            for (const auto &rule : BrownOut::rules) {
                if (QString::fromLatin1(rule.key) != key) continue;
                auto *spin = new Page5NumberFormat::DecimalSpinBox;
                spin->setObjectName(key); spin->setDecimals(rule.integer ? 0 : Page5NumberFormat::decimals);
                spin->setRange(rule.minimum, rule.maximum);
                spin->setValue(initial.value(key, rule.initial).toDouble());
                spin->setSuffix(unit); spin->setKeyboardTracking(false);
                m_numbers.insert(key, spin); target->addRow(label, spin); break;
            }
        };
        addNumber(conditions, "delay_ms", "Initial settle:", " ms");
        auto *ramp = form("Voltage descent");
        m_rampSource = new QComboBox;
        m_rampSource->setObjectName("rampSource");
        m_rampSource->addItem("AC Input", "AC");
        m_rampSource->addItem("DC Input", "DC");
        m_rampSource->setCurrentIndex(qMax(0, m_rampSource->findData(initial.value("ramp_source", "AC"))));
        m_rampSource->setToolTip("選擇要降低電壓的輸入；另一種已選用的輸入維持供電。結束或異常時兩者都會關閉。");
        ramp->addRow("Ramp source（降壓來源）:", m_rampSource);
        addNumber(ramp, "dc_source", "DC source to ramp:");
        const auto updateDc = [this, ramp] {
            ramp->setRowVisible(m_numbers["dc_source"], m_rampSource->currentData() == "DC");
        };
        connect(m_rampSource, &QComboBox::currentIndexChanged, this, updateDc);
        const auto selectAvailableSource = [this] {
            const bool ac = m_inputs->ac->currentData().toInt() >= 0;
            const bool dc = m_inputs->dc->currentData().toInt() >= 0;
            if (ac != dc) m_rampSource->setCurrentIndex(ac ? 0 : 1);
        };
        connect(m_inputs->ac, &QComboBox::currentIndexChanged, this, selectAvailableSource);
        connect(m_inputs->dc, &QComboBox::currentIndexChanged, this, selectAvailableSource);
        updateDc();
        auto *minimumVin = new QLabel(QString("%1 V (fixed)").arg(BrownOut::minimumInputVoltage));
        minimumVin->setObjectName("minimumVinSummary");
        ramp->addRow("Minimum Vin:", minimumVin);
        addNumber(ramp, "voltage_step", "Step down:", " V");
        m_numbers["voltage_step"]->setSingleStep(0.5);
        addNumber(ramp, "step_interval_ms", "Step interval:", " ms");
        auto *trigger = form("Trigger");
        m_mode = new QComboBox; m_mode->setObjectName("brownOutMode");
        m_mode->addItem("Fixed Fall (Vout) — Run + Single", "FIXED_FALL");
        m_mode->addItem("Track Level", "TRACK");
        m_mode->setCurrentIndex(qMax(0, m_mode->findData(initial.value("mode", "FIXED_FALL"))));
        trigger->addRow("Mode:", m_mode);
        m_direction = new QComboBox; m_direction->setObjectName("brownOutDirection");
        m_direction->addItem("Fall", "FALL"); m_direction->addItem("Rise", "RISE");
        m_direction->setCurrentIndex(qMax(0, m_direction->findData(initial.value("direction", "FALL"))));
        trigger->addRow("Search direction:", m_direction);
        addNumber(trigger, "trigger_channel", "Trigger CH:");
        addNumber(trigger, "trigger_level", "Initial trigger level:", " V / A");
        const auto updateMode = [this, trigger] {
            trigger->setRowVisible(m_direction, m_mode->currentData() == "TRACK");
            qobject_cast<QLabel *>(trigger->labelForField(m_numbers["trigger_level"]))->setText(
                m_mode->currentData() == "TRACK" ? "Initial trigger level:" : "Fall trigger level:");
            m_numbers["trigger_level"]->setSuffix(m_mode->currentData() == "TRACK" ? " V / A" : " V");
        };
        connect(m_mode, &QComboBox::currentIndexChanged, this, updateMode); updateMode();
        auto *monitor = form("Shutdown detection");
        addNumber(monitor, "monitor_output", "Load output Index:");
        addNumber(monitor, "shutdown_voltage", "Vout shutdown threshold:", " V");
        auto *advancedToggle = new QToolButton;
        advancedToggle->setObjectName("advancedToggle"); advancedToggle->setText("Advanced（進階設定）");
        advancedToggle->setCheckable(true); advancedToggle->setArrowType(Qt::RightArrow);
        advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        layout->addWidget(advancedToggle);
        auto *advanced = form("Timing（等待與關機確認）");
        advanced->parentWidget()->setObjectName("advancedSettings");
        m_ignoreAdvanced = new QCheckBox("Ignore（使用內建預設值）");
        m_ignoreAdvanced->setObjectName("ignoreAdvanced");
        m_ignoreAdvanced->setChecked(initial.value("ignore_advanced", BrownOut::defaults().value("ignore_advanced")).toBool());
        advanced->addRow(m_ignoreAdvanced);
        auto *ignoreHelp = new QLabel("勾選後使用內建預設：傳輸逾時 10 秒、關機確認 500 ms、關機後觀察 1 秒。\n"
                                      "自訂值仍會保存，取消勾選後恢復使用。");
        ignoreHelp->setWordWrap(true); advanced->addRow(ignoreHelp);
        const auto addTiming = [&](const char *key, const QString &label, const QString &help) {
            addNumber(advanced, key, label, " ms");
            m_numbers[key]->setToolTip(help);
            auto *description = new QLabel(help);
            description->setWordWrap(true);
            advanced->addRow(description);
        };
        addTiming("data_timeout_ms", "Data transfer timeout（傳輸逾時）:",
                  "已觸發後等待完整波形資料的逾時預算；實際等待也會依示波器記錄時間調整。");
        addTiming("shutdown_confirm_ms", "Confirm below threshold（關機確認）:",
                  "Vout 需持續低於關機電壓達此時間，避免瞬間下降被誤判為關機。");
        addTiming("tail_ms", "Observe after shutdown（關機後觀察）:",
                  "確認關機後繼續監看 Vout，並等待事件資料；Vout 回升超過門檻時回報失敗。");
        const auto updateAdvanced = [this](bool ignored) {
            for (const char *key : {"data_timeout_ms", "shutdown_confirm_ms", "tail_ms"})
                m_numbers[key]->setEnabled(!ignored);
        };
        connect(m_ignoreAdvanced, &QCheckBox::toggled, this, updateAdvanced);
        updateAdvanced(m_ignoreAdvanced->isChecked());
        advanced->parentWidget()->hide();
        connect(advancedToggle, &QToolButton::toggled, this, [advanced, advancedToggle](bool expanded) {
            advanced->parentWidget()->setVisible(expanded);
            advancedToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        });
        m_capture = new StrategyCaptureWidget(initial, body);
        layout->addWidget(m_capture);
        auto *captureHelp = new QLabel("Capture 開啟時，Track 每筆完成後先存檔再重新等待觸發；傳輸期間降壓與 Vout 監測持續執行。");
        captureHelp->setWordWrap(true); layout->addWidget(captureHelp);
        scroll->setWidget(body); root->addWidget(scroll);
        auto *buttons = new QHBoxLayout;
        auto *apply = new QPushButton("Apply"); apply->setObjectName("applyBtn");
        auto *close = new QPushButton("Close"); close->setObjectName("closeBtn");
        buttons->addStretch(); buttons->addWidget(apply); buttons->addWidget(close);
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        connect(apply, &QPushButton::clicked, this, [this] {
            try { const BrownOut::Settings checked(config()); accept(); }
            catch (const std::exception &e) { QMessageBox::warning(this, "Brown Out", QString::fromUtf8(e.what())); }
        });
        root->addLayout(buttons);
        resize(620, 920);
    }
    QVariantMap config() const
    {
        QVariantMap result{{"ramp_source", m_rampSource->currentData()},
                           {"load_index", m_load->currentData()}, {"load_label", m_load->currentText()},
                           {"mode", m_mode->currentData()}, {"ignore_advanced", m_ignoreAdvanced->isChecked()},
                           {"direction", m_mode->currentData() == "TRACK" ? m_direction->currentData() : QVariant("FALL")}};
        m_inputs->save(result);
        m_capture->save(result);
        for (const auto &rule : BrownOut::rules)
            result[rule.key] = rule.integer ? QVariant(int(m_numbers[rule.key]->value()))
                                           : QVariant(m_numbers[rule.key]->value());
        return result;
    }
  private:
    StrategyCaptureWidget *m_capture = nullptr;
    static QComboBox *selection(const QStringList &options, int selected)
    {
        auto *combo = new QComboBox;
        combo->addItem("— None —", -1);
        for (int i = 0; i < options.size(); ++i)
            if (!options[i].trimmed().isEmpty()) combo->addItem(options[i], i);
        combo->setCurrentIndex(qMax(0, combo->findData(selected)));
        return combo;
    }
    InputSelectors *m_inputs;
    QComboBox *m_load, *m_mode, *m_direction, *m_rampSource;
    QCheckBox *m_ignoreAdvanced;
    QMap<QString, QDoubleSpinBox *> m_numbers;
};
