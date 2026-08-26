#include "mso44bwritedialog.h"
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

// ★ MSO 4/5/6 系列專屬常數已統一至 OscSharedData（oscshareddata.h）：
//   MSO456_COUPLINGS     → CH<x>:COUPling {AC|DC|DCR}
//   MSO456_BWS           → CH<x>:BANdwidth {FULl|<NR3>}
//   MSO456_TERMINATIONS  → CH<x>:TERmination {50|1e6}
//   MSO456_HORZ_MODES    → HORizontal:MODe {AUTO|MANual}
//   MSO456_HORZ_CONFIGS  → HORizontal:MODe:MANual:CONFIGure
//   MSO456_SAMPLE_RATES  → HORizontal:MODe:SAMPLERate（顯示字串）
//   MSO456_RECORD_LENGTHS→ HORizontal:MODe:RECOrdlength（顯示字串）
//
// 時間軸 / 垂直比例 / Acquire Mode 直接沿用：
//   TIMESCALES / DPO7000_SCALES / ACQ_MODES

// ══════════════════════════════════════════════════════
//  Constructor
// ══════════════════════════════════════════════════════
MSO44BWriteDialog::MSO44BWriteDialog(Oscilloscope*      scope,
                                     const QString&     configuredModel,
                                     const QVariantMap& initCfg,
                                     int                seqNo,
                                     const QString&     extName,
                                     QWidget*           parent)
    : OscWriteDialogBase(scope, configuredModel, initCfg, seqNo, extName, parent)
{
    buildFrame({700, 680}, true);
}

// ══════════════════════════════════════════════════════
//  updateManualControls — 依 Mode 切換 MANual 專屬控件
//
//  AUTO   → Sample Rate / Record Length / CONFIGure 全部禁用
//           （三者由儀器自動決定，SCPI 不送這三條指令）
//  MANual → 全部啟用，可手動輸入後送出
// ══════════════════════════════════════════════════════
void MSO44BWriteDialog::updateManualControls(bool isManual)
{
    m_horzConfig->setEnabled(isManual);
    m_sampleRate->setEnabled(isManual);
    m_recordLen ->setEnabled(isManual);
}

// ══════════════════════════════════════════════════════
//  buildContentWidget — MSO 4/5/6 完整設定 UI
// ══════════════════════════════════════════════════════
QWidget* MSO44BWriteDialog::buildContentWidget()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    auto* inner = new QWidget;
    auto* vlay  = new QVBoxLayout(inner);
    vlay->setContentsMargins(8, 4, 8, 8);
    vlay->setSpacing(8);

    // ════════════════════════════════
    //  Horizontal
    //  MSO 新增：Mode / CONFIGure / Sample Rate / Record Length
    // ════════════════════════════════
    {
        auto* grp  = new QGroupBox("Horizontal");
        auto* wrap = new QVBoxLayout(grp);
        wrap->setContentsMargins(8, 2, 8, 4);
        wrap->setSpacing(2);

        m_horzIgnore = new QCheckBox("Ignore");
        m_horzIgnore->setChecked(m_cfg.value("ignore_horizontal", false).toBool());
        m_horzIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
        wrap->addWidget(m_horzIgnore, 0, Qt::AlignRight);

        auto* content = new QWidget;
        auto* form    = new QFormLayout(content);
        form->setLabelAlignment(Qt::AlignRight);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(6);

        // ── HORizontal:MODe ──────────────────────────
        m_horzMode = makeCombo(MSO456_HORZ_MODES,
                               m_cfg.value("horz_mode", "AUTO").toString());
        form->addRow("Mode:", m_horzMode);

        // ── HORizontal:SCAle ─────────────────────────
        m_timescale = makeCombo(TIMESCALES,
                                m_cfg.value("timescale", "200ms").toString());
        form->addRow("Timescale (s/div):", m_timescale);

        // ── HORizontal:POSition ──────────────────────
        m_horzPos = new QDoubleSpinBox;
        m_horzPos->setRange(0.0, 100.0);
        m_horzPos->setSingleStep(1.0);
        m_horzPos->setDecimals(1);
        m_horzPos->setSuffix(" %");
        m_horzPos->setValue(m_cfg.value("horz_pos", 50.0).toDouble());
        form->addRow("Position:", m_horzPos);

        // ── 分隔提示 ─────────────────────────────────
        auto* sepLbl = new QLabel("── MANual mode only ──────────────────");
        sepLbl->setStyleSheet("font-size:10px; color:#aab8cc;");
        form->addRow("", sepLbl);

        // ── HORizontal:MODe:MANual:CONFIGure ─────────
        // 決定調整 Sample Rate 時，Timescale 或 RecordLength 哪個跟著變
        m_horzConfig = makeCombo(MSO456_HORZ_CONFIGS,
                                 m_cfg.value("horz_config", "HORIZontalscale").toString());
        form->addRow("Adjust on SR change:", m_horzConfig);

        auto* cfgHint = new QLabel(
            "HORIZontalscale → Timescale 跟著 SR 變\n"
            "RECORDLength    → Record Length 跟著 SR 變");
        cfgHint->setWordWrap(true);
        cfgHint->setStyleSheet("font-size:10px; color:#8899bb;");
        form->addRow("", cfgHint);

        // ── HORizontal:MODe:SAMPLERate ───────────────
        m_sampleRate = makeCombo(MSO456_SAMPLE_RATES,
                                 m_cfg.value("sample_rate", "6.25GS/s").toString());
        form->addRow("Sample Rate:", m_sampleRate);

        // ── HORizontal:MODe:RECOrdlength ─────────────
        m_recordLen = makeCombo(MSO456_RECORD_LENGTHS,
                                m_cfg.value("record_length", "10k").toString());
        form->addRow("Record Length:", m_recordLen);

        auto* rlHint = new QLabel(
            "RecordLength = SampleRate × Timescale × 10\n"
            "三者聯動，儀器依 CONFIGure 設定決定哪個自動調整。");
        rlHint->setWordWrap(true);
        rlHint->setStyleSheet("font-size:10px; color:#8899bb;");
        form->addRow("", rlHint);

        wrap->addWidget(content);
        content->setEnabled(!m_horzIgnore->isChecked());
        connect(m_horzIgnore, &QCheckBox::toggled, content,
                [content](bool ig){ content->setEnabled(!ig); });

        vlay->addWidget(grp);

        // ── Mode 切換 → MANual 控件 enabled/disabled ─
        const bool initManual = (m_horzMode->currentText() == "MANual");
        updateManualControls(initManual);

        connect(m_horzMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
                    updateManualControls(m_horzMode->currentText() == "MANual");
                });
    }

    // ════════════════════════════════
    //  Acquire
    //  MSO 新增：ACQuire:FASTAcq:STATE
    // ════════════════════════════════
    {
        auto* grp  = new QGroupBox("Acquire");
        auto* wrap = new QVBoxLayout(grp);
        wrap->setContentsMargins(8, 2, 8, 4);
        wrap->setSpacing(2);

        m_acqIgnore = new QCheckBox("Ignore");
        m_acqIgnore->setChecked(m_cfg.value("ignore_acquire", false).toBool());
        m_acqIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
        wrap->addWidget(m_acqIgnore, 0, Qt::AlignRight);

        auto* content = new QWidget;
        auto* form    = new QFormLayout(content);
        form->setLabelAlignment(Qt::AlignRight);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(6);

        m_acqMode = makeCombo(ACQ_MODES,
                              m_cfg.value("acq_mode", "SAMple").toString());
        form->addRow("Mode:", m_acqMode);

        auto* modeHint = new QLabel(
            "SAMple | PEAKdetect | HIRes | AVErage | ENVelope");
        modeHint->setStyleSheet("font-size:10px; color:#8899bb;");
        form->addRow("", modeHint);

        // ACQuire:FASTAcq:STATE {ON|OFF}
        m_fastAcq = new QCheckBox("Enable FastAcq");
        m_fastAcq->setChecked(m_cfg.value("fastacq", false).toBool());
        form->addRow("FastAcq:", m_fastAcq);

        auto* fastHint = new QLabel(
            "FastAcq ON 時 Record Length 由儀器控制，波形更新率大幅提升。\n"
            "搭配 AUTO mode 效果最佳；MANual mode 下仍可啟用。");
        fastHint->setWordWrap(true);
        fastHint->setStyleSheet("font-size:10px; color:#8899bb;");
        form->addRow("", fastHint);

        wrap->addWidget(content);
        content->setEnabled(!m_acqIgnore->isChecked());
        connect(m_acqIgnore, &QCheckBox::toggled, content,
                [content](bool ig){ content->setEnabled(!ig); });

        vlay->addWidget(grp);
    }

    // ════════════════════════════════
    //  Channels CH1 ~ CH4
    //  MSO 差異：新增 Termination 欄
    //            Coupling 含 DCR
    //            Bandwidth 為數值型清單
    // ════════════════════════════════
    {
        auto* grp  = new QGroupBox("Channels");
        auto* wrap = new QVBoxLayout(grp);
        wrap->setContentsMargins(8, 2, 8, 4);
        wrap->setSpacing(2);

        m_chIgnore = new QCheckBox("Ignore");
        m_chIgnore->setChecked(m_cfg.value("ignore_channels", false).toBool());
        m_chIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
        wrap->addWidget(m_chIgnore, 0, Qt::AlignRight);

        auto* content = new QWidget;
        auto* grid    = new QGridLayout(content);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(4);

        const QStringList hdrs = {
            "Ch", "Enable", "Term (Ω)",
            "Scale (V/div)", "Coupling", "BW", "Vert. Pos (div)"
        };
        for (int c = 0; c < hdrs.size(); ++c) {
            auto* h = new QLabel(hdrs[c]);
            h->setStyleSheet("font-weight:bold; font-size:10px; color:#5566aa;");
            h->setAlignment(Qt::AlignCenter);
            grid->addWidget(h, 0, c);
        }

        for (int ch = 0; ch < 4; ++ch) {
            const QString pfx = QString("ch%1_").arg(ch + 1);
            auto& w = m_ch[ch];

            auto* lbl = new QLabel(QString("CH%1").arg(ch + 1));
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet("font-weight:bold; color:#2a5db0; font-size:12px;");

            w.enable = new QCheckBox;
            w.enable->setChecked(m_cfg.value(pfx + "enabled", ch == 0).toBool());

            // CH<x>:TERmination {50|1e6}
            w.termination = makeCombo(
                MSO456_TERMINATIONS,
                m_cfg.value(pfx + "termination", "1MΩ").toString());

            // CH<x>:SCAle（含 Custom 自訂值）
            w.scale = makeCombo(DPO7000_SCALES,
                                m_cfg.value(pfx + "scale", "100mV").toString());
            w.customEdit = new QLineEdit;
            w.customEdit->setPlaceholderText("e.g. 30V");
            w.customEdit->setMaximumWidth(80);

            {
                const QString saved    = m_cfg.value(pfx + "scale", "100mV").toString();
                const bool    wasCustom = !DPO7000_SCALES.contains(saved) && !saved.isEmpty();
                if (wasCustom) {
                    w.scale->setCurrentText("Custom...");
                    w.customEdit->setText(saved);
                    w.customEdit->setVisible(true);
                } else {
                    w.customEdit->setVisible(false);
                }
            }
            connect(w.scale, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    w.customEdit, [pw = &w](int) {
                        pw->customEdit->setVisible(
                            pw->scale->currentText() == "Custom...");
                    });

            auto* scaleCell = new QWidget;
            auto* scaleLay  = new QHBoxLayout(scaleCell);
            scaleLay->setContentsMargins(0, 0, 0, 0);
            scaleLay->setSpacing(4);
            scaleLay->addWidget(w.scale, 1);
            scaleLay->addWidget(w.customEdit);

            // CH<x>:COUPling {AC|DC|DCR}
            w.coupling = makeCombo(
                MSO456_COUPLINGS,
                m_cfg.value(pfx + "coupling", "DC").toString());

            // CH<x>:BANdwidth {FULl|<NR3>}
            w.bw = makeCombo(
                MSO456_BWS,
                m_cfg.value(pfx + "bandwidth", "Full").toString());

            // CH<x>:POSition <NR1>
            w.pos = new QDoubleSpinBox;
            w.pos->setRange(-8.0, 8.0);
            w.pos->setSingleStep(0.5);
            w.pos->setDecimals(1);
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

            const int row = ch + 1;
            grid->addWidget(lbl,           row, 0, Qt::AlignCenter);
            grid->addWidget(w.enable,      row, 1, Qt::AlignCenter);
            grid->addWidget(w.termination, row, 2);
            grid->addWidget(scaleCell,     row, 3);
            grid->addWidget(w.coupling,    row, 4);
            grid->addWidget(w.bw,          row, 5);
            grid->addWidget(w.pos,         row, 6);
        }

        wrap->addWidget(content);
        content->setEnabled(!m_chIgnore->isChecked());
        connect(m_chIgnore, &QCheckBox::toggled, content,
                [content](bool ig){ content->setEnabled(!ig); });

        vlay->addWidget(grp);
    }

    // ════════════════════════════════
    //  Trigger (Edge)
    //  MSO 來源清單含 LINE / AUX（取代 DPO 的 EXT）
    // ════════════════════════════════
    {
        auto* grp  = new QGroupBox("Trigger (Edge)");
        auto* wrap = new QVBoxLayout(grp);
        wrap->setContentsMargins(8, 2, 8, 4);
        wrap->setSpacing(2);

        m_trigIgnore = new QCheckBox("Ignore");
        m_trigIgnore->setChecked(m_cfg.value("ignore_trigger", false).toBool());
        m_trigIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
        wrap->addWidget(m_trigIgnore, 0, Qt::AlignRight);

        auto* content = new QWidget;
        auto* form    = new QFormLayout(content);
        form->setLabelAlignment(Qt::AlignRight);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(6);

        m_trigSource = makeCombo(TRIG_SOURCES_MSO,
                                 m_cfg.value("trig_source", "CH1").toString());
        m_trigEdge   = makeCombo(TRIG_EDGES,
                                 m_cfg.value("trig_edge",   "Rising").toString());
        form->addRow("Source:", m_trigSource);
        form->addRow("Edge:",   m_trigEdge);

        wrap->addWidget(content);
        content->setEnabled(!m_trigIgnore->isChecked());
        connect(m_trigIgnore, &QCheckBox::toggled, content,
                [content](bool ig){ content->setEnabled(!ig); });

        vlay->addWidget(grp);
    }

    vlay->addStretch();
    scroll->setWidget(inner);
    return scroll;
}

// ══════════════════════════════════════════════════════
//  saveConfig — 收集 UI 狀態 → m_cfg
//
//  ViewModel 送出 SCPI 時的對應關係：
//
//  key              SCPI 指令
//  ──────────────────────────────────────────────────────────────────
//  "horz_mode"      HORizontal:MODe {AUTO|MANual}
//  "timescale"      HORizontal:SCAle <NR3>          （例："200ms"→200e-3）
//  "horz_pos"       HORizontal:POSition <NR3>        （0~100，%）
//  ── MANual 模式才送出以下三條 ─────────────────────────────────────
//  "horz_config"    HORizontal:MODe:MANual:CONFIGure {HORIZontalscale|RECORDLength}
//  "sample_rate"    HORizontal:MODe:SAMPLERate <NR1> （例："6.25GS/s"→6.25e9）
//  "record_length"  HORizontal:MODe:RECOrdlength <NR1>（例："10k"→10000）
//  ─────────────────────────────────────────────────────────────────
//  "acq_mode"       ACQuire:MODe {SAMple|PEAKdetect|…}
//  "fastacq"        ACQuire:FASTAcq:STATE {ON|OFF}
//  "ch<n>_enabled"  DISplay:WAVEView1:CH<n>:STATE {ON|OFF}
//  "ch<n>_term"     CH<n>:TERmination {50|1000000}   （"50Ω"→50, "1MΩ"→1e6）
//  "ch<n>_scale"    CH<n>:SCAle <NR3>                （例："100mV"→100e-3）
//  "ch<n>_coupling" CH<n>:COUPling {AC|DC|DCR}
//  "ch<n>_bandwidth"CH<n>:BANdwidth {FULl|<NR3>}     （"Full"→FULl, "200MHz"→200e6）
//  "ch<n>_position" CH<n>:POSition <NR1>              （格）
// ══════════════════════════════════════════════════════
void MSO44BWriteDialog::saveConfig()
{
    if (!m_timescale) return;

    m_cfg["ignore_horizontal"] = m_horzIgnore->isChecked();
    m_cfg["ignore_acquire"]    = m_acqIgnore->isChecked();
    m_cfg["ignore_channels"]   = m_chIgnore->isChecked();
    m_cfg["ignore_trigger"]    = m_trigIgnore->isChecked();

    // ── Horizontal ────────────────────────────────────
    m_cfg["horz_mode"] = m_horzMode->currentText();
    m_cfg["timescale"] = m_timescale->currentText();
    m_cfg["horz_pos"]  = m_horzPos->value();

    // MANual 專屬：不論 Mode 為何都存入 m_cfg，
    // 由 ViewModel 判斷 horz_mode == "MANual" 再決定是否送出 SCPI
    m_cfg["horz_config"]   = m_horzConfig->currentText();
    m_cfg["sample_rate"]   = m_sampleRate->currentText();
    m_cfg["record_length"] = m_recordLen->currentText();

    // ── Acquire ───────────────────────────────────────
    m_cfg["acq_mode"] = m_acqMode->currentText();
    m_cfg["fastacq"]  = m_fastAcq->isChecked();

    // ── Trigger ───────────────────────────────────────
    m_cfg["trig_source"] = m_trigSource->currentText();
    m_cfg["trig_edge"]   = m_trigEdge->currentText();

    // ── Channels ─────────────────────────────────────
    for (int ch = 0; ch < 4; ++ch) {
        const QString pfx = QString("ch%1_").arg(ch + 1);
        auto& w = m_ch[ch];

        m_cfg[pfx + "enabled"]     = w.enable->isChecked();
        m_cfg[pfx + "termination"] = w.termination->currentText();
        m_cfg[pfx + "coupling"]    = w.coupling->currentText();
        m_cfg[pfx + "bandwidth"]   = w.bw->currentText();
        m_cfg[pfx + "position"]    = w.pos->value();

        if (w.scale->currentText() == "Custom...") {
            const QString custom = w.customEdit ? w.customEdit->text().trimmed() : QString();
            m_cfg[pfx + "scale"] = custom.isEmpty() ? "100mV" : custom;
        } else {
            m_cfg[pfx + "scale"] = w.scale->currentText();
        }
    }
}
