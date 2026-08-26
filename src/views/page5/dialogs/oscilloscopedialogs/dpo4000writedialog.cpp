#include "dpo4000writedialog.h"
#include "oscshareddata.h"

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

using namespace OscSharedData;

// ══════════════════════════════════════════════════════
//  Constructor
// ══════════════════════════════════════════════════════
DPO4000WriteDialog::DPO4000WriteDialog(Oscilloscope*      scope,
                                       const QString&     configuredModel,
                                       const QVariantMap& initCfg,
                                       int                seqNo,
                                       const QString&     extName,
                                       QWidget*           parent)
    : OscWriteDialogBase(scope, configuredModel, initCfg, seqNo, extName, parent)
{
    buildFrame({680, 560}, true);
}

// ══════════════════════════════════════════════════════
//  buildContentWidget — DPO4000 完整設定 UI
// ══════════════════════════════════════════════════════
QWidget* DPO4000WriteDialog::buildContentWidget()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    auto* inner = new QWidget;
    auto* vlay  = new QVBoxLayout(inner);
    vlay->setContentsMargins(8, 4, 8, 8);
    vlay->setSpacing(8);

    // ════════════════════════════════
    //  Horizontal
    // ════════════════════════════════
    auto* horzGrp  = new QGroupBox("Horizontal");
    auto* horzWrap = new QVBoxLayout(horzGrp);
    horzWrap->setContentsMargins(8, 2, 8, 4);
    horzWrap->setSpacing(2);

    m_horzIgnore = new QCheckBox("Ignore");
    m_horzIgnore->setChecked(m_cfg.value("ignore_horizontal", false).toBool());
    m_horzIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
    horzWrap->addWidget(m_horzIgnore, 0, Qt::AlignRight);

    auto* horzContent = new QWidget;
    auto* horzForm    = new QFormLayout(horzContent);
    horzForm->setLabelAlignment(Qt::AlignRight);
    horzForm->setHorizontalSpacing(12);
    horzForm->setVerticalSpacing(6);

    m_timescale = makeCombo(TIMESCALES, m_cfg.value("timescale", "200ms").toString());
    horzForm->addRow("Timescale (s/div):", m_timescale);

    m_horzPos = new QDoubleSpinBox;
    m_horzPos->setRange(0.0, 100.0);
    m_horzPos->setSingleStep(1.0);
    m_horzPos->setDecimals(1);
    m_horzPos->setSuffix(" %");
    m_horzPos->setValue(m_cfg.value("horz_pos", 40.0).toDouble());
    horzForm->addRow("Position:", m_horzPos);

    horzWrap->addWidget(horzContent);
    horzContent->setEnabled(!m_horzIgnore->isChecked());
    connect(m_horzIgnore, &QCheckBox::toggled, horzContent,
            [horzContent](bool ig){ horzContent->setEnabled(!ig); });

    vlay->addWidget(horzGrp);

    // ════════════════════════════════
    //  Acquire
    // ════════════════════════════════
    auto* acqGrp  = new QGroupBox("Acquire");
    auto* acqWrap = new QVBoxLayout(acqGrp);
    acqWrap->setContentsMargins(8, 2, 8, 4);
    acqWrap->setSpacing(2);

    m_acqIgnore = new QCheckBox("Ignore");
    m_acqIgnore->setChecked(m_cfg.value("ignore_acquire", false).toBool());
    m_acqIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
    acqWrap->addWidget(m_acqIgnore, 0, Qt::AlignRight);

    auto* acqContent = new QWidget;
    auto* acqForm    = new QFormLayout(acqContent);
    acqForm->setLabelAlignment(Qt::AlignRight);
    acqForm->setHorizontalSpacing(12);
    acqForm->setVerticalSpacing(6);

    // DPO4000 不支援 WFMDB，使用專屬清單
    m_acqMode = makeCombo(DPO4000_ACQ_MODES, m_cfg.value("acq_mode", "SAMple").toString());
    acqForm->addRow("Mode:", m_acqMode);

    auto* hint = new QLabel("SAMple | PEAKdetect | HIRes | AVErage | ENVelope");
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size: 10px; color:#8899bb;");
    acqForm->addRow("", hint);

    acqWrap->addWidget(acqContent);
    acqContent->setEnabled(!m_acqIgnore->isChecked());
    connect(m_acqIgnore, &QCheckBox::toggled, acqContent,
            [acqContent](bool ig){ acqContent->setEnabled(!ig); });

    vlay->addWidget(acqGrp);

    // ════════════════════════════════
    //  Channels CH1 ~ CH4
    // ════════════════════════════════
    auto* chGrp  = new QGroupBox("Channels");
    auto* chWrap = new QVBoxLayout(chGrp);
    chWrap->setContentsMargins(8, 2, 8, 4);
    chWrap->setSpacing(2);

    m_chIgnore = new QCheckBox("Ignore");
    m_chIgnore->setChecked(m_cfg.value("ignore_channels", false).toBool());
    m_chIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
    chWrap->addWidget(m_chIgnore, 0, Qt::AlignRight);

    auto* chContent = new QWidget;
    auto* chGrid    = new QGridLayout(chContent);
    chGrid->setHorizontalSpacing(6);
    chGrid->setVerticalSpacing(4);

    const QStringList hdrs = {
        "Ch", "Enable", "Scale (V/div)", "Coupling", "Bandwidth", "Vert. Pos (div)"
    };
    for (int c = 0; c < hdrs.size(); ++c) {
        auto* h = new QLabel(hdrs[c]);
        h->setStyleSheet("font-weight:bold; font-size:10px; color:#5566aa;");
        h->setAlignment(Qt::AlignCenter);
        chGrid->addWidget(h, 0, c);
    }

    for (int ch = 0; ch < 4; ++ch) {
        const QString pfx = QString("ch%1_").arg(ch + 1);
        auto& w = m_ch[ch];

        auto* lbl = new QLabel(QString("CH%1").arg(ch + 1));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-weight:bold; color:#2a5db0; font-size:12px;");

        // Enable（CH1 預設 on，其餘 off）
        w.enable = new QCheckBox;
        w.enable->setChecked(m_cfg.value(pfx + "enabled", ch == 0).toBool());

        // Scale + Custom 輸入框
        w.scale = makeCombo(DPO4000_SCALES,
                            m_cfg.value(pfx + "scale", "100mV").toString());

        w.customEdit = new QLineEdit;
        w.customEdit->setPlaceholderText("e.g. 30V");
        w.customEdit->setMaximumWidth(80);

        // 若上次儲存的是自訂值，恢復顯示
        {
            const QString saved    = m_cfg.value(pfx + "scale", "100mV").toString();
            const bool    wasCustom = !DPO4000_SCALES.contains(saved) && !saved.isEmpty();
            if (wasCustom) {
                w.scale->setCurrentText("Custom...");
                w.customEdit->setText(saved);
                w.customEdit->setVisible(true);
            } else {
                w.customEdit->setVisible(false);
            }
        }

        connect(w.scale, QOverload<int>::of(&QComboBox::currentIndexChanged),
                w.customEdit, [this_w = &w](int /*idx*/) {
                    this_w->customEdit->setVisible(
                        this_w->scale->currentText() == "Custom...");
                });

        auto* scaleCell = new QWidget;
        auto* scaleLay  = new QHBoxLayout(scaleCell);
        scaleLay->setContentsMargins(0, 0, 0, 0);
        scaleLay->setSpacing(4);
        scaleLay->addWidget(w.scale, 1);
        scaleLay->addWidget(w.customEdit);

        // DPO4000 頻寬限制：Full / 250MHz / 20MHz
        w.coupling = makeCombo(COUPLINGS,
                               m_cfg.value(pfx + "coupling", "DC").toString());
        w.bw       = makeCombo(DPO4000_BWS,
                               m_cfg.value(pfx + "bandwidth", "Full").toString());

        w.pos = new QDoubleSpinBox;
        w.pos->setRange(-8.0, 8.0);
        w.pos->setSingleStep(0.5);
        w.pos->setDecimals(1);
        w.pos->setValue(m_cfg.value(pfx + "position", 0.0).toDouble());

        // Enable toggle → 子控件 enabled/disabled
        auto updateEnable = [&w, scaleCell](bool en) {
            scaleCell->setEnabled(en);
            w.coupling->setEnabled(en);
            w.bw->setEnabled(en);
            w.pos->setEnabled(en);
        };
        updateEnable(w.enable->isChecked());
        connect(w.enable, &QCheckBox::toggled, updateEnable);

        const int row = ch + 1;
        chGrid->addWidget(lbl,        row, 0, Qt::AlignCenter);
        chGrid->addWidget(w.enable,   row, 1, Qt::AlignCenter);
        chGrid->addWidget(scaleCell,  row, 2);
        chGrid->addWidget(w.coupling, row, 3);
        chGrid->addWidget(w.bw,       row, 4);
        chGrid->addWidget(w.pos,      row, 5);
    }

    chWrap->addWidget(chContent);
    chContent->setEnabled(!m_chIgnore->isChecked());
    connect(m_chIgnore, &QCheckBox::toggled, chContent,
            [chContent](bool ig){ chContent->setEnabled(!ig); });

    vlay->addWidget(chGrp);

    // ════════════════════════════════
    //  Trigger (Edge)
    // ════════════════════════════════
    auto* trigGrp  = new QGroupBox("Trigger (Edge)");
    auto* trigWrap = new QVBoxLayout(trigGrp);
    trigWrap->setContentsMargins(8, 2, 8, 4);
    trigWrap->setSpacing(2);

    m_trigIgnore = new QCheckBox("Ignore");
    m_trigIgnore->setChecked(m_cfg.value("ignore_trigger", false).toBool());
    m_trigIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
    trigWrap->addWidget(m_trigIgnore, 0, Qt::AlignRight);

    auto* trigContent = new QWidget;
    auto* trigForm    = new QFormLayout(trigContent);
    trigForm->setLabelAlignment(Qt::AlignRight);
    trigForm->setHorizontalSpacing(12);
    trigForm->setVerticalSpacing(6);

    m_trigSource = makeCombo(TRIG_SOURCES_DPO,
                             m_cfg.value("trig_source", "CH1").toString());
    m_trigEdge   = makeCombo(TRIG_EDGES,
                             m_cfg.value("trig_edge",   "Rising").toString());
    trigForm->addRow("Source:", m_trigSource);
    trigForm->addRow("Edge:",   m_trigEdge);

    trigWrap->addWidget(trigContent);
    trigContent->setEnabled(!m_trigIgnore->isChecked());
    connect(m_trigIgnore, &QCheckBox::toggled, trigContent,
            [trigContent](bool ig){ trigContent->setEnabled(!ig); });

    vlay->addWidget(trigGrp);
    vlay->addStretch();
    scroll->setWidget(inner);
    return scroll;
}

// ══════════════════════════════════════════════════════
//  saveConfig — 收集 UI 狀態 → m_cfg
// ══════════════════════════════════════════════════════
void DPO4000WriteDialog::saveConfig()
{
    if (!m_timescale) return;

    m_cfg["ignore_horizontal"] = m_horzIgnore->isChecked();
    m_cfg["ignore_acquire"]    = m_acqIgnore->isChecked();
    m_cfg["ignore_channels"]   = m_chIgnore->isChecked();
    m_cfg["ignore_trigger"]    = m_trigIgnore->isChecked();

    m_cfg["timescale"]   = m_timescale->currentText();
    m_cfg["horz_pos"]    = m_horzPos->value();
    m_cfg["acq_mode"]    = m_acqMode->currentText();
    m_cfg["trig_source"] = m_trigSource->currentText();
    m_cfg["trig_edge"]   = m_trigEdge->currentText();

    for (int ch = 0; ch < 4; ++ch) {
        const QString pfx = QString("ch%1_").arg(ch + 1);
        auto& w = m_ch[ch];

        m_cfg[pfx + "enabled"]   = w.enable->isChecked();
        m_cfg[pfx + "coupling"]  = w.coupling->currentText();
        m_cfg[pfx + "bandwidth"] = w.bw->currentText();
        m_cfg[pfx + "position"]  = w.pos->value();

        if (w.scale->currentText() == "Custom...") {
            const QString custom = w.customEdit ? w.customEdit->text().trimmed() : QString();
            m_cfg[pfx + "scale"] = custom.isEmpty() ? "100mV" : custom;
        } else {
            m_cfg[pfx + "scale"] = w.scale->currentText();
        }
    }
}
