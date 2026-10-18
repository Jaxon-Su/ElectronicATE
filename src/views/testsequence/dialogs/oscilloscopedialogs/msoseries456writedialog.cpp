#include "msoseries456writedialog.h"
#include "oscshareddata.h"
#include "oscilloscopesettingrules.h"
#include "horizontalsettingslimits.h"
#include "horizontalquantityedit.h"
#include "triggermodelcatalog.h"
#include "../dialognumberformat.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <cmath>

using namespace OscSharedData;

namespace {
QWidget* valueField(QWidget *input, QLineEdit *&estimate, const QString &name)
{
    auto *field = new QWidget;
    auto *layout = new QHBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    estimate = new QLineEdit;
    estimate->setObjectName(name);
    estimate->setReadOnly(true);
    estimate->setToolTip("Calculated preview for 10 divisions; actual values are read back when the task runs.");
    layout->addWidget(input);
    layout->addWidget(estimate);
    return field;
}
QString timeText(double seconds)
{
    const double factor = seconds >= 1 ? 1 : seconds >= 1e-3 ? 1e3 : seconds >= 1e-6 ? 1e6 : seconds >= 1e-9 ? 1e9 : 1e12;
    const QString unit = factor == 1 ? "s" : factor == 1e3 ? "ms" : factor == 1e6 ? "us" : factor == 1e9 ? "ns" : "ps";
    return QString::number(seconds * factor, 'g', 12) + " " + unit + "/div";
}
}

// MSO 4/5/6 系列專屬常數已統一至 OscSharedData（oscshareddata.h）：
//   MSO456_COUPLINGS     → CH<x>:COUPling {AC|DC|DCR}
//   MSO456_BWS           → CH<x>:BANdwidth {FULl|<NR3>}
//   MSO456_TERMINATIONS  → CH<x>:TERmination {50|1e6}
//   MSO456_HORZ_MODES    → HORizontal:MODe {AUTO|MANual}
//   MSO456_HORZ_CONFIGS  → HORizontal:MODe:MANual:CONFIGure
//   MSO456_SAMPLE_RATES  → HORizontal:MODe:SAMPLERate（顯示字串）
//   MSO456_RECORD_LENGTHS→ HORizontal:MODe:RECOrdlength（顯示字串）
//
// 時間軸 / 垂直比例 / Acquire Mode 直接沿用：
//   TIMESCALES / CHANNEL_SCALES / ACQ_MODES

//  Constructor
MSOSeries456WriteDialog::MSOSeries456WriteDialog(Oscilloscope* scope, const QString& configuredModel,
                                     const QVariantMap& initCfg, int seqNo, const QString& extName,
                                     QWidget* parent)
    : OscWriteDialogBase(scope, configuredModel, initCfg, seqNo, extName, parent)
{
    m_channelCount = TriggerModelCatalog::channelCount(configuredModel.isEmpty() ? m_model : configuredModel);
    buildFrame({700, 680}, true);
    updateHorizontalControls();
}

void MSOSeries456WriteDialog::changeHorizontalInputs()
{
    // Carry the currently calculated third value into its newly editable field.
    // Do not manufacture a value while the old input pair is invalid.
    const auto preview = HorizontalSettingsLimits::evaluate(true, m_previousInputs == TimeAndRate,
        m_timescale->value(), m_sampleRate->value(), m_recordLen->value(), m_horzLimits->currentData().toString());
    const auto inputs = static_cast<HorizontalInputs>(m_horzInputs->currentIndex());
    if (preview.error.isEmpty()) {
        if (inputs == TimeAndRate) m_timescale->setValue(*preview.time);
        else m_recordLen->setValue(*preview.points);
    }
    m_previousInputs = inputs;
    updateHorizontalControls();
}

void MSOSeries456WriteDialog::updateHorizontalControls()
{
    const bool manual = m_horzMode->currentIndex() == Manual;
    const bool timeAndRate = m_horzInputs->currentIndex() == TimeAndRate;
    m_horzInputs->setEnabled(manual);
    auto showInput = [](QWidget *input, QLineEdit *estimate, bool editable) {
        input->setVisible(editable);
        input->setEnabled(editable);
        estimate->setVisible(!editable);
    };
    showInput(m_timescale, m_timeEstimate, !manual || timeAndRate);
    showInput(m_sampleRate, m_rateEstimate, manual);
    showInput(m_recordLen, m_recordEstimate, manual && !timeAndRate);
    const auto preview = HorizontalSettingsLimits::evaluate(manual, timeAndRate,
        m_timescale->value(), m_sampleRate->value(), m_recordLen->value(), m_horzLimits->currentData().toString());
    const bool valid = preview.error.isEmpty();
    m_horizontalValid = valid;
    m_derivedValue = valid && manual ? (timeAndRate ? *preview.points : *preview.time) : 0;
    m_rateEstimate->setText("Automatic");
    m_recordEstimate->setText(!manual ? "Automatic" : valid
        ? QString::number(*preview.points, 'g', 15) + " points (Calculated)" : "Invalid inputs");
    m_timeEstimate->setText(valid ? timeText(*preview.time) + " (Calculated)" : "Invalid inputs");
    m_horizontalHint->setText(valid
        ? (manual ? "Calculated for 10 divisions. Actual values are reported in Log when this task runs."
                  : "Sample Rate and RL are automatic. Actual values are reported in Log when this task runs.")
        : preview.error);
    m_horizontalHint->setStyleSheet(valid ? "color:#6b7a99;" : "color:#b42318;");
    updateApplyState();
}

void MSOSeries456WriteDialog::updateApplyState()
{
    if (!m_settingsHint) return; // Content is still being constructed.
    QString error;
    if (!m_chIgnore->isChecked()) {
        for (int ch = 0; ch < m_channelCount && error.isEmpty(); ++ch) {
            const auto &w = m_ch[ch];
            if (!w.enable->isChecked()) continue;
            const auto scale = w.scale->currentText() == "Custom..." ? w.customEdit->text().trimmed() : w.scale->currentText();
            if (!OscilloscopeSettingRules::quantity(scale, "channel"))
                error = QString("CH%1 Scale: enter a positive value in V/mV/uV or A/mA/uA.").arg(ch + 1);
            else if (!OscilloscopeSettingRules::decode(QString("ch%1_bandwidth").arg(ch + 1), w.bw->currentText()))
                error = QString("CH%1 BW: choose Full or enter a positive frequency (for example 20MHz).").arg(ch + 1);
        }
    }
    const auto source = m_trigSource->currentText();
    if (error.isEmpty() && !m_trigIgnore->isChecked() && source.startsWith("CH") && source.mid(2).toInt() > m_channelCount)
        error = QString("Trigger source %1 is unavailable on the configured %2-channel model.").arg(source).arg(m_channelCount);
    m_settingsHint->setText(error);
    m_settingsHint->setVisible(!error.isEmpty());
    if (auto *apply = findChild<QPushButton*>("applyBtn")) {
        apply->setEnabled((m_horizontalValid || m_horzIgnore->isChecked()) && error.isEmpty());
        apply->setToolTip(!error.isEmpty() ? error : !m_horizontalValid && !m_horzIgnore->isChecked() ? m_horizontalHint->text() : QString());
    }
}

//  buildContentWidget — MSO 4/5/6 完整設定 UI
QWidget* MSOSeries456WriteDialog::buildContentWidget()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    auto* inner = new QWidget;
    auto* vlay = new QVBoxLayout(inner);
    vlay->setContentsMargins(8, 4, 8, 8);
    vlay->setSpacing(8);

    //  Horizontal
    //  MSO 新增：Mode / CONFIGure / Sample Rate / Record Length
    {
        auto* grp = new QGroupBox("Horizontal");
        auto* wrap = new QVBoxLayout(grp);
        wrap->setContentsMargins(8, 2, 8, 4);
        wrap->setSpacing(2);

        m_horzIgnore = new QCheckBox("Ignore");
        m_horzIgnore->setChecked(m_cfg.value("ignore_horizontal", false).toBool() || m_cfg.value("ignore_basic", false).toBool());
        m_horzIgnore->setStyleSheet("color:#aa6600;");
        wrap->addWidget(m_horzIgnore, 0, Qt::AlignRight);

        auto* content = new QWidget;
        auto* form = new QFormLayout(content);
        form->setLabelAlignment(Qt::AlignRight);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(6);

        m_horzMode = new QComboBox;
        m_horzMode->setObjectName("horizontalMode");
        m_horzMode->addItems({"Auto", "Manual"});
        const bool manual = m_cfg.value("horz_mode", "AUTO").toString().trimmed().compare("MANUAL", Qt::CaseInsensitive) == 0;
        const bool timeAndRate = m_cfg.value("horz_config", manual ? "HORIZONTALSCALE" : "RECORDLENGTH").toString().trimmed()
                                    .compare("RECORDLENGTH", Qt::CaseInsensitive) == 0;
        m_horzMode->setCurrentIndex(manual ? Manual : Auto);
        form->addRow("Mode:", m_horzMode);
        m_horzInputs = new QComboBox;
        m_horzInputs->setObjectName("horizontalInputs");
        m_horzInputs->addItems({"Time Scale + Sample Rate", "Sample Rate + RL"});
        m_horzInputs->setCurrentIndex(timeAndRate ? TimeAndRate : RateAndLength);
        m_previousInputs = static_cast<HorizontalInputs>(m_horzInputs->currentIndex());
        form->addRow("Inputs:", m_horzInputs);

        m_timescale = new HorizontalQuantityEdit("time", TIMESCALES, m_cfg.value("timescale", "16us").toString(), "horizontalTime");
        form->addRow("Time Scale:", valueField(m_timescale, m_timeEstimate, "estimatedTime"));
        m_sampleRate = new HorizontalQuantityEdit("rate", MSO456_SAMPLE_RATES, m_cfg.value("sample_rate", "1.25GS/s").toString(), "horizontalRate");
        form->addRow("Sample Rate:", valueField(m_sampleRate, m_rateEstimate, "estimatedRate"));
        m_recordLen = new HorizontalQuantityEdit("count", MSO456_RECORD_LENGTHS, m_cfg.value("record_length", "200k").toString(), "horizontalRecordLength");
        form->addRow("Record Length:", valueField(m_recordLen, m_recordEstimate, "estimatedRecordLength"));

        m_horzPos = new TestSequenceNumberFormat::DecimalSpinBox;
        m_horzPos->setRange(OscilloscopeSettingRules::horizontalMinimum, OscilloscopeSettingRules::horizontalMaximum);
        m_horzPos->setSingleStep(1.0);
        m_horzPos->setDecimals(TestSequenceNumberFormat::decimals);
        m_horzPos->setSuffix(" %");
        m_horzPos->setValue(m_cfg.value("horz_pos", 40.0).toDouble());
        form->addRow("Position:", m_horzPos);

        m_horzLimits = new QComboBox;
        m_horzLimits->setObjectName("horizontalLimits");
        m_horzLimits->addItem("MSO44B - 31.25M (real-time)", "MSO44B");
        m_horzLimits->addItem("MSO44B - 62.5M option (real-time)", "MSO44B_62M");
        m_horzLimits->addItem("Other / instrument limits", "UNSPECIFIED");
        // The current installation uses an MSO44B; this is an explicit profile,
        // not an assertion that the generic MSO456 driver detected the model.
        const QString configured = (m_configuredModel.isEmpty() ? m_model : m_configuredModel).trimmed().toUpper();
        const QString defaultLimits = configured == "MSO44B" || configured == "MSOSERIES456" ? "MSO44B" : "UNSPECIFIED";
        m_horzLimits->setCurrentIndex(qMax(0, m_horzLimits->findData(m_cfg.value("horz_limits", defaultLimits).toString().trimmed().toUpper())));
        m_horzLimits->setToolTip("Choose the installed model/capacity. This profile is not detected from the instrument. Other models and interpolated rates require instrument readback.");
        form->addRow("Range check:", m_horzLimits);
        m_horizontalHint = new QLabel;
        m_horizontalHint->setObjectName("horizontalHint");
        m_horizontalHint->setWordWrap(true);
        form->addRow("", m_horizontalHint);

        wrap->addWidget(content);
        content->setEnabled(!m_horzIgnore->isChecked());
        connect(m_horzIgnore, &QCheckBox::toggled, content, [content](bool ig) { content->setEnabled(!ig); });

        vlay->addWidget(grp);

        connect(m_horzMode, &QComboBox::currentIndexChanged, this, &MSOSeries456WriteDialog::updateHorizontalControls);
        connect(m_horzInputs, &QComboBox::currentIndexChanged, this, &MSOSeries456WriteDialog::changeHorizontalInputs);
        connect(m_horzLimits, &QComboBox::currentIndexChanged, this, &MSOSeries456WriteDialog::updateHorizontalControls);
        for (auto *input : {m_timescale, m_sampleRate, m_recordLen})
            input->changed = [this] { updateHorizontalControls(); };
        connect(m_horzIgnore, &QCheckBox::toggled, this, &MSOSeries456WriteDialog::updateHorizontalControls);
        updateHorizontalControls();
    }

    //  Acquire
    //  MSO 新增：ACQuire:FASTAcq:STATE
    {
        auto* grp = new QGroupBox("Acquire");
        auto* wrap = new QVBoxLayout(grp);
        wrap->setContentsMargins(8, 2, 8, 4);
        wrap->setSpacing(2);

        m_acqIgnore = new QCheckBox("Ignore");
        m_acqIgnore->setChecked(m_cfg.value("ignore_acquire", false).toBool() || m_cfg.value("ignore_basic", false).toBool());
        m_acqIgnore->setStyleSheet("color:#aa6600;");
        wrap->addWidget(m_acqIgnore, 0, Qt::AlignRight);

        auto* content = new QWidget;
        auto* form = new QFormLayout(content);
        form->setLabelAlignment(Qt::AlignRight);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(6);

        m_acqMode = makeCombo(ACQ_MODES, m_cfg.value("acq_mode", "SAMple").toString());
        form->addRow("Mode:", m_acqMode);

        auto* modeHint = new QLabel("SAMple | PEAKdetect | HIRes | AVErage | ENVelope");
        modeHint->setStyleSheet("color:#8899bb;");
        form->addRow("", modeHint);

        // ACQuire:FASTAcq:STATE {ON|OFF}
        m_fastAcq = new QCheckBox("Enable FastAcq");
        m_fastAcq->setChecked(m_cfg.value("fastacq", false).toBool());
        form->addRow("FastAcq:", m_fastAcq);

        auto* fastHint = new QLabel("FastAcq may change the available horizontal settings. Check actual values in Log.");
        fastHint->setWordWrap(true);
        fastHint->setStyleSheet("color:#8899bb;");
        form->addRow("", fastHint);

        wrap->addWidget(content);
        content->setEnabled(!m_acqIgnore->isChecked());
        connect(m_acqIgnore, &QCheckBox::toggled, content, [content](bool ig) { content->setEnabled(!ig); });

        vlay->addWidget(grp);
    }

    //  Channels available on the configured model (4 / 6 / 8).
    //  MSO 差異：新增 Termination 欄
    //            Coupling 含 DCR
    //            Bandwidth 為數值型清單
    {
        auto* grp = new QGroupBox("Channels");
        auto* wrap = new QVBoxLayout(grp);
        wrap->setContentsMargins(8, 2, 8, 4);
        wrap->setSpacing(2);

        m_chIgnore = new QCheckBox("Ignore");
        m_chIgnore->setChecked(m_cfg.value("ignore_channels", false).toBool());
        m_chIgnore->setStyleSheet("color:#aa6600;");
        wrap->addWidget(m_chIgnore, 0, Qt::AlignRight);

        auto* content = new QWidget;
        auto* grid = new QGridLayout(content);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(4);

        const QStringList hdrs = {"Ch",       "Enable", "Term (Ω)",       "Scale (units/div)",
                                  "Coupling", "BW",     "Vert. Pos (div)"};
        for (int c = 0; c < hdrs.size(); ++c) {
            auto* h = new QLabel(hdrs[c]);
            h->setStyleSheet("font-weight:bold; color:#5566aa;");
            h->setAlignment(Qt::AlignCenter);
            grid->addWidget(h, 0, c);
        }

        for (int ch = 0; ch < m_channelCount; ++ch) {
            const QString pfx = QString("ch%1_").arg(ch + 1);
            auto& w = m_ch[ch];

            auto* lbl = new QLabel(QString("CH%1").arg(ch + 1));
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet("font-weight:bold; color:#2a5db0; ");

            w.enable = new QCheckBox;
            w.enable->setChecked(m_cfg.value(pfx + "enabled", ch == 0).toBool());

            // CH<x>:TERmination {50|1e6}
            w.termination =
                makeCombo(MSO456_TERMINATIONS, m_cfg.value(pfx + "termination", "1MΩ").toString());

            // CH<x>:SCAle（含 Custom 自訂值）
            w.scale = makeCombo(CHANNEL_SCALES, m_cfg.value(pfx + "scale", "100mV").toString());
            w.customEdit = new QLineEdit;
            w.customEdit->setObjectName(pfx + "customScale");
            w.customEdit->setPlaceholderText("e.g. 30V / 2A");
            w.scale->setToolTip("Use the channel's probe units. For a current probe, enter A/mA/uA using "
                                "Custom. This does not change the probe type.");
            w.customEdit->setMaximumWidth(80);

            {
                const QString saved = m_cfg.value(pfx + "scale", "100mV").toString();
                const bool wasCustom = !CHANNEL_SCALES.contains(saved) && !saved.isEmpty();
                if (wasCustom) {
                    w.scale->setCurrentText("Custom...");
                    w.customEdit->setText(TestSequenceNumberFormat::channelScale(saved));
                    w.customEdit->setVisible(true);
                } else {
                    w.customEdit->setVisible(false);
                }
            }
            connect(w.scale, QOverload<int>::of(&QComboBox::currentIndexChanged), w.customEdit,
                    [pw = &w](int) { pw->customEdit->setVisible(pw->scale->currentText() == "Custom..."); });
            connect(w.customEdit, &QLineEdit::editingFinished, w.customEdit, [edit = w.customEdit] {
                edit->setText(TestSequenceNumberFormat::channelScale(edit->text()));
            });

            auto* scaleCell = new QWidget;
            auto* scaleLay = new QHBoxLayout(scaleCell);
            scaleLay->setContentsMargins(0, 0, 0, 0);
            scaleLay->setSpacing(4);
            scaleLay->addWidget(w.scale, 1);
            scaleLay->addWidget(w.customEdit);

            // CH<x>:COUPling {AC|DC|DCR}
            w.coupling = makeCombo(MSO456_COUPLINGS, m_cfg.value(pfx + "coupling", "DC").toString());

            // CH<x>:BANdwidth {FULl|<NR3>}
            w.bw = makeCombo(MSO456_BWS, m_cfg.value(pfx + "bandwidth", "Full").toString());
            w.bw->setEditable(true);
            w.bw->setEditText(m_cfg.value(pfx + "bandwidth", "Full").toString().trimmed());
            w.bw->setObjectName(pfx + "bandwidth");

            // CH<x>:POSition <NR1>
            w.pos = new TestSequenceNumberFormat::DecimalSpinBox;
            w.pos->setRange(OscilloscopeSettingRules::positionMinimum, OscilloscopeSettingRules::positionMaximum);
            w.pos->setSingleStep(0.5);
            w.pos->setDecimals(TestSequenceNumberFormat::decimals);
            w.pos->setValue(m_cfg.value(pfx + "position", 0.0).toDouble());

            auto updateEnable = [&w, scaleCell](bool en) {
                w.termination->setEnabled(en);
                scaleCell->setEnabled(en);
                w.coupling->setEnabled(en);
                w.bw->setEnabled(en);
                w.pos->setEnabled(en);
            };
            updateEnable(w.enable->isChecked());
            connect(w.enable, &QCheckBox::toggled, updateEnable);
            connect(w.enable, &QCheckBox::toggled, this, &MSOSeries456WriteDialog::updateApplyState);
            connect(w.scale, &QComboBox::currentTextChanged, this, &MSOSeries456WriteDialog::updateApplyState);
            connect(w.customEdit, &QLineEdit::textChanged, this, &MSOSeries456WriteDialog::updateApplyState);
            connect(w.bw, &QComboBox::currentTextChanged, this, &MSOSeries456WriteDialog::updateApplyState);

            const int row = ch + 1;
            grid->addWidget(lbl, row, 0, Qt::AlignCenter);
            grid->addWidget(w.enable, row, 1, Qt::AlignCenter);
            grid->addWidget(w.termination, row, 2);
            grid->addWidget(scaleCell, row, 3);
            grid->addWidget(w.coupling, row, 4);
            grid->addWidget(w.bw, row, 5);
            grid->addWidget(w.pos, row, 6);
        }

        wrap->addWidget(content);
        content->setEnabled(!m_chIgnore->isChecked());
        connect(m_chIgnore, &QCheckBox::toggled, content, [content](bool ig) { content->setEnabled(!ig); });
        connect(m_chIgnore, &QCheckBox::toggled, this, &MSOSeries456WriteDialog::updateApplyState);

        vlay->addWidget(grp);
    }

    //  Trigger (Edge)
    //  MSO 來源清單含 LINE / AUX（取代 DPO 的 EXT）
    {
        auto* grp = new QGroupBox("Trigger (Edge)");
        auto* wrap = new QVBoxLayout(grp);
        wrap->setContentsMargins(8, 2, 8, 4);
        wrap->setSpacing(2);

        m_trigIgnore = new QCheckBox("Ignore");
        m_trigIgnore->setChecked(m_cfg.value("ignore_trigger", false).toBool());
        m_trigIgnore->setStyleSheet("color:#aa6600;");
        wrap->addWidget(m_trigIgnore, 0, Qt::AlignRight);

        auto* content = new QWidget;
        auto* form = new QFormLayout(content);
        form->setLabelAlignment(Qt::AlignRight);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(6);

        QStringList sources;
        for (int ch = 1; ch <= m_channelCount; ++ch) sources << QString("CH%1").arg(ch);
        sources << "LINE" << "AUX";
        auto source = m_cfg.value("trig_source", "CH1").toString().trimmed().toUpper();
        if (source == "AUXILIARY") source = "AUX";
        // Preserve an unavailable saved channel visibly; never silently retarget CH1.
        if (!sources.contains(source)) sources << source;
        m_trigSource = makeCombo(sources, source);
        const auto edge = m_cfg.value("trig_edge", "Rising").toString().trimmed().toUpper();
        m_trigEdge = makeCombo(TRIG_EDGES, edge == "RISE" || edge == "RISING" ? "Rising" :
                              edge == "FALL" || edge == "FALLING" ? "Falling" : "Either");
        form->addRow("Source:", m_trigSource);
        form->addRow("Edge:", m_trigEdge);

        wrap->addWidget(content);
        content->setEnabled(!m_trigIgnore->isChecked());
        connect(m_trigIgnore, &QCheckBox::toggled, content, [content](bool ig) { content->setEnabled(!ig); });
        connect(m_trigIgnore, &QCheckBox::toggled, this, &MSOSeries456WriteDialog::updateApplyState);
        connect(m_trigSource, &QComboBox::currentTextChanged, this, &MSOSeries456WriteDialog::updateApplyState);

        vlay->addWidget(grp);
    }

    m_settingsHint = new QLabel;
    m_settingsHint->setObjectName("scopeSettingsHint");
    m_settingsHint->setWordWrap(true);
    m_settingsHint->setStyleSheet("color:#b42318;");
    vlay->addWidget(m_settingsHint);
    vlay->addStretch();
    scroll->setWidget(inner);
    return scroll;
}

// Save the draft only. SCPI compilation and actual-value readback belong to the Driver.
void MSOSeries456WriteDialog::saveConfig()
{
    if (!m_timescale)
        return;

    m_cfg["ignore_horizontal"] = m_horzIgnore->isChecked();
    m_cfg["ignore_acquire"] = m_acqIgnore->isChecked();
    m_cfg["ignore_channels"] = m_chIgnore->isChecked();
    m_cfg["ignore_trigger"] = m_trigIgnore->isChecked();
    // Express the existing combined flag using the two visible section choices.
    m_cfg.remove("ignore_basic");

    // Horizontal
    const auto mode = static_cast<HorizontalMode>(m_horzMode->currentIndex());
    const bool timeAndRate = m_horzInputs->currentIndex() == TimeAndRate;
    m_cfg["horz_limits"] = m_horzLimits->currentData().toString();
    m_cfg["horz_mode"] = mode == Auto ? "AUTO" : "MANual";
    // An invalid inactive editor (for example after switching back to Auto)
    // must not replace a previously valid value or create invalid XML.
    auto saveQuantity = [this](const QString &key, HorizontalQuantityEdit *input, const QString &kind) {
        if (const auto value = OscilloscopeSettingRules::quantity(input->text(), kind)) {
            if (kind != "count" || *value <= 9007199254740991.0)
                m_cfg[key] = input->text();
        }
    };
    saveQuantity("timescale", m_timescale, "time");
    m_cfg.remove("auto_period");
    m_cfg["horz_pos"] = m_horzPos->value();

    // Keep the existing XML keys. The service decodes only the selected pair;
    // the derived third value is a preview, never an independent command.
    m_cfg["horz_config"] = timeAndRate ? "RECORDLength" : "HORIZontalscale";
    saveQuantity("sample_rate", m_sampleRate, "rate");
    saveQuantity("record_length", m_recordLen, "count");
    if (m_derivedValue > 0 && std::isfinite(m_derivedValue)) {
        if (mode == Manual && timeAndRate)
            m_cfg["record_length"] = QString::number(m_derivedValue, 'g', 15);
        else if (mode == Manual && !timeAndRate)
            m_cfg["timescale"] = QString::number(m_derivedValue, 'g', 15) + "s";
    }

    // Acquire
    m_cfg["acq_mode"] = m_acqMode->currentText();
    m_cfg["fastacq"] = m_fastAcq->isChecked();

    // Trigger
    m_cfg["trig_source"] = m_trigSource->currentText();
    m_cfg["trig_edge"] = m_trigEdge->currentText();

    // Channels
    for (int ch = 0; ch < m_channelCount; ++ch) {
        const QString pfx = QString("ch%1_").arg(ch + 1);
        auto& w = m_ch[ch];

        m_cfg[pfx + "enabled"] = w.enable->isChecked();
        m_cfg[pfx + "termination"] = w.termination->currentText();
        m_cfg[pfx + "coupling"] = w.coupling->currentText();
        if (OscilloscopeSettingRules::decode(pfx + "bandwidth", w.bw->currentText()))
            m_cfg[pfx + "bandwidth"] = w.bw->currentText().trimmed();
        m_cfg[pfx + "position"] = w.pos->value();

        const QString scale = w.scale->currentText() == "Custom..." ? w.customEdit->text().trimmed() : w.scale->currentText();
        // Ignore/disabled editors may contain an unfinished draft. Preserve the
        // previous valid setting instead of corrupting XML or inventing 100mV.
        if (OscilloscopeSettingRules::quantity(scale, "channel"))
            m_cfg[pfx + "scale"] = scale;
    }
}
