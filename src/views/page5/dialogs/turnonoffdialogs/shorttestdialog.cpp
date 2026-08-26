#include "shorttestdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDebug>

static const char* SHORT_DLG_STYLE = R"(
    QDialog { background: #f4f7fb; }

    QLabel#titleLbl {
        font-size: 15px; font-weight: bold; color: #1a2a4a;
    }
    QLabel#badgeShortTurnOn {
        font-size: 10px; font-weight: bold;
        color: #fff; background: #16a085;
        border-radius: 8px; padding: 2px 8px;
    }
    QLabel#badgeTurnOnShort {
        font-size: 10px; font-weight: bold;
        color: #fff; background: #2471a3;
        border-radius: 8px; padding: 2px 8px;
    }
    QFrame#sep { color: #dce6f1; }

    QGroupBox {
        font-weight: bold; font-size: 11px; color: #2c3e6a;
        border: 1px solid #dce6f1; border-radius: 5px;
        margin-top: 8px; padding-top: 6px;
    }
    QGroupBox::title {
        subcontrol-origin: margin; left: 8px; top: 0px; padding: 0 4px;
    }
    QLabel { font-size: 11px; color: #2c3e50; }

    /* ComboBox */
    QComboBox {
        background: #ffffff; border: 1px solid #c8d8ee;
        border-radius: 3px; padding: 3px 8px;
        min-height: 26px; font-size: 12px; color: #1a2a4a;
        min-width: 180px;
    }
    QComboBox:focus  { border-color: #3a7bd5; }
    QComboBox::drop-down {
        subcontrol-origin: padding;
        subcontrol-position: top right;
        width: 22px; border-left: 1px solid #c8d8ee;
    }
    QComboBox QAbstractItemView {
        background: #fff; border: 1px solid #c8d8ee;
        selection-background-color: #e8f0fe;
        selection-color: #1a56db;
        font-size: 12px;
    }

    /* 預覽標籤 */
    QLabel#previewLbl {
        font-size: 10px; color: #8899bb;
        padding: 2px 0px;
    }

    /* SpinBox */
    QSpinBox {
        background: #ffffff; border: 1px solid #c8d8ee;
        border-radius: 3px; padding: 2px 6px;
        min-height: 26px; font-size: 13px; color: #1a2a4a;
        padding-right: 20px;
    }
    QSpinBox:focus { border-color: #3a7bd5; }
    QSpinBox::up-button {
        subcontrol-origin: border; subcontrol-position: top right;
        width: 18px; height: 14px;
    }
    QSpinBox::down-button {
        subcontrol-origin: border; subcontrol-position: bottom right;
        width: 18px; height: 14px;
    }
    QLabel#hintLbl { font-size: 10px; color: #8899bb; }

    QPushButton#applyBtn {
        background: #5a8cdd; color: white;
        border: none; border-radius: 4px;
        padding: 6px 24px; font-size: 12px; font-weight: bold;
    }
    QPushButton#applyBtn:hover   { background: #4a7ccd; }
    QPushButton#applyBtn:pressed { background: #3a6cbb; }
    QPushButton#closeBtn {
        background: #3a7bd5; color: white;
        border: none; border-radius: 4px;
        padding: 6px 24px; font-size: 12px; font-weight: bold;
    }
    QPushButton#closeBtn:hover   { background: #4a8be5; }
    QPushButton#closeBtn:pressed { background: #2a6bc5; }
)";

// ══════════════════════════════════════════════════════
//  Constructor
// ══════════════════════════════════════════════════════
ShortTestDialog::ShortTestDialog(Mode                        mode,
                                 const QStringList&          inputOptions,
                                 const QStringList&          loadOptions,
                                 const QVector<RelayOption>& relayOptions,
                                 const QVariantMap&          initCfg,
                                 int                         seqNo,
                                 QWidget*                    parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_relayOptions(relayOptions)
    , m_cfg(initCfg)
{
    const QString title = (mode == ShortThenTurnOn) ? "Short then Turn On"
                                                     : "Turn On then Short";
    setWindowTitle(QString("Row %1 — %2").arg(seqNo).arg(title));
    setStyleSheet(SHORT_DLG_STYLE);
    buildUI(inputOptions, loadOptions, relayOptions);
    restoreFromCfg();
}

// ══════════════════════════════════════════════════════
//  buildUI
// ══════════════════════════════════════════════════════
void ShortTestDialog::buildUI(const QStringList&          inputOptions,
                              const QStringList&          loadOptions,
                              const QVector<RelayOption>& relayOptions)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // ── 標題列 + Badge ───────────────────────────────
    auto* headerRow = new QHBoxLayout;
    const QString titleText = (m_mode == ShortThenTurnOn) ? "Short then Turn On"
                                                           : "Turn On then Short";
    auto* titleLbl = new QLabel(titleText);
    titleLbl->setObjectName("titleLbl");
    headerRow->addWidget(titleLbl);
    headerRow->addStretch();

    const QString badgeText = (m_mode == ShortThenTurnOn) ? "SHORT→ON" : "ON→SHORT";
    const QString badgeName = (m_mode == ShortThenTurnOn) ? "badgeShortTurnOn"
                                                           : "badgeTurnOnShort";
    auto* badge = new QLabel(badgeText);
    badge->setObjectName(badgeName);
    headerRow->addWidget(badge);
    root->addLayout(headerRow);

    // ── 分隔線 ──────────────────────────────────────
    auto* sep = new QFrame;
    sep->setObjectName("sep");
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    // ══════════════════════════════════════════════
    //  設定區
    // ══════════════════════════════════════════════
    auto* grp  = new QGroupBox("Settings");
    auto* form = new QFormLayout(grp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);

    // ── 1. Input ─────────────────────────────────
    m_inputCombo = new QComboBox;
    m_inputCombo->addItem("— None —", -1);
    for (int i = 0; i < inputOptions.size(); ++i)
        m_inputCombo->addItem(
            QString("%1.  %2").arg(i + 1).arg(inputOptions[i]), i);
    form->addRow("Input :", m_inputCombo);

    auto* inputPreview = new QLabel("—");
    inputPreview->setObjectName("previewLbl");
    form->addRow("", inputPreview);

    // ── 2. Load ──────────────────────────────────
    m_loadCombo = new QComboBox;
    m_loadCombo->addItem("— None —", -1);
    for (int i = 0; i < loadOptions.size(); ++i)
        m_loadCombo->addItem(
            QString("%1.  %2").arg(i + 1).arg(loadOptions[i]), i);
    form->addRow("Load :", m_loadCombo);

    auto* loadPreview = new QLabel("—");
    loadPreview->setObjectName("previewLbl");
    form->addRow("", loadPreview);

    // ── 3. Short (RL) ────────────────────────────
    m_relayCombo = new QComboBox;
    m_relayCombo->addItem("— None —", -1);
    for (int i = 0; i < relayOptions.size(); ++i)
        m_relayCombo->addItem(
            QString("%1.  %2").arg(i + 1).arg(relayOptions[i].label), i);
    form->addRow("Short :", m_relayCombo);

    auto* relayPreview = new QLabel("—");
    relayPreview->setObjectName("previewLbl");
    form->addRow("", relayPreview);

    // ── 4. Discharge ─────────────────────────────
    m_dischargeCombo = new QComboBox;
    m_dischargeCombo->addItem("— None —", -1);
    for (int i = 0; i < relayOptions.size(); ++i)
        m_dischargeCombo->addItem(
            QString("%1.  %2").arg(i + 1).arg(relayOptions[i].label), i);
    form->addRow("Discharge :", m_dischargeCombo);

    auto* dischargePreview = new QLabel("—");
    dischargePreview->setObjectName("previewLbl");
    form->addRow("", dischargePreview);

    // ── 5. Duration ──────────────────────────────
    m_spinMs = new QSpinBox;
    m_spinMs->setRange(0, 3'600'000);
    m_spinMs->setSingleStep(1000);
    m_spinMs->setSuffix(" ms");
    m_spinMs->setValue(m_cfg.value("delay_ms", 5000).toInt());
    m_spinMs->setAccelerated(true);
    m_spinMs->setMinimumWidth(180);
    form->addRow("Duration :", m_spinMs);

    auto* hint = new QLabel("Step: 1000 ms　｜　Range: 0 ms ~ 3,600,000 ms (1 hr)");
    hint->setObjectName("hintLbl");
    form->addRow("", hint);

    root->addWidget(grp);
    root->addStretch();

    // ── 動態預覽 — Input ─────────────────────────
    connect(m_inputCombo, &QComboBox::currentIndexChanged,
            this, [this, inputOptions, inputPreview](int) {
                const int dataIdx = m_inputCombo->currentData().toInt();
                if (dataIdx < 0 || dataIdx >= inputOptions.size()) {
                    inputPreview->setText("—");
                    return;
                }
                const QStringList parts = inputOptions[dataIdx].split('/');
                if (parts.size() == 3)
                    inputPreview->setText(
                        QString("Vin: %1  Frequency: %2  Phase: %3")
                            .arg(parts[0], parts[1], parts[2]));
                else
                    inputPreview->setText(inputOptions[dataIdx]);
            });

    // ── 動態預覽 — Load ──────────────────────────
    connect(m_loadCombo, &QComboBox::currentIndexChanged,
            this, [this, loadOptions, loadPreview](int) {
                const int dataIdx = m_loadCombo->currentData().toInt();
                if (dataIdx < 0 || dataIdx >= loadOptions.size()) {
                    loadPreview->setText("—");
                    return;
                }
                loadPreview->setText(
                    QString("Load row:  %1").arg(loadOptions[dataIdx]));
            });

    // ── 動態預覽 — Short ─────────────────────────
    connect(m_relayCombo, &QComboBox::currentIndexChanged,
            this, [this, relayPreview](int) {
                const int dataIdx = m_relayCombo->currentData().toInt();
                if (dataIdx < 0 || dataIdx >= m_relayOptions.size()) {
                    relayPreview->setText("—");
                    return;
                }
                relayPreview->setText(
                    QString("Short:  %1").arg(m_relayOptions[dataIdx].label));
            });

    // ── 動態預覽 — Discharge ──────────────────────
    connect(m_dischargeCombo, &QComboBox::currentIndexChanged,
            this, [this, dischargePreview](int) {
                const int dataIdx = m_dischargeCombo->currentData().toInt();
                if (dataIdx < 0 || dataIdx >= m_relayOptions.size()) {
                    dischargePreview->setText("—");
                    return;
                }
                dischargePreview->setText(
                    QString("Discharge:  %1").arg(m_relayOptions[dataIdx].label));
            });

    // ── 按鈕列 ──────────────────────────────────
    auto* btnRow   = new QHBoxLayout;
    auto* applyBtn = new QPushButton("Apply");
    auto* closeBtn = new QPushButton("Close");
    applyBtn->setObjectName("applyBtn");
    closeBtn->setObjectName("closeBtn");
    applyBtn->setFixedWidth(100);
    closeBtn->setFixedWidth(100);

    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        saveConfig();
        qDebug() << "[ShortTestDialog] Applied:" << m_cfg;
        accept();
    });
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        saveConfig();
        accept();
    });

    btnRow->addStretch();
    btnRow->addWidget(applyBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    setMinimumSize(420, 490);
}

// ══════════════════════════════════════════════════════
//  restoreFromCfg
// ══════════════════════════════════════════════════════
void ShortTestDialog::restoreFromCfg()
{
    const int inputIdx = m_cfg.value("input_index", -1).toInt();
    if (inputIdx >= 0) {
        for (int i = 0; i < m_inputCombo->count(); ++i) {
            if (m_inputCombo->itemData(i).toInt() == inputIdx) {
                m_inputCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    const int loadIdx = m_cfg.value("load_index", -1).toInt();
    if (loadIdx >= 0) {
        for (int i = 0; i < m_loadCombo->count(); ++i) {
            if (m_loadCombo->itemData(i).toInt() == loadIdx) {
                m_loadCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    const int relayIdx = m_cfg.value("relay_index", -1).toInt();
    if (relayIdx >= 0) {
        for (int i = 0; i < m_relayCombo->count(); ++i) {
            if (m_relayCombo->itemData(i).toInt() == relayIdx) {
                m_relayCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    const int dischargeIdx = m_cfg.value("discharge_index", -1).toInt();
    if (dischargeIdx >= 0) {
        for (int i = 0; i < m_dischargeCombo->count(); ++i) {
            if (m_dischargeCombo->itemData(i).toInt() == dischargeIdx) {
                m_dischargeCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

// ══════════════════════════════════════════════════════
//  saveConfig
// ══════════════════════════════════════════════════════
void ShortTestDialog::saveConfig()
{
    // Input
    const int inputIdx = m_inputCombo->currentData().toInt();
    m_cfg["input_index"] = inputIdx;
    m_cfg["input_label"] = (inputIdx >= 0)
                               ? m_inputCombo->currentText().section("  ", 1)
                               : QString();

    // Load
    const int loadIdx = m_loadCombo->currentData().toInt();
    m_cfg["load_index"] = loadIdx;
    m_cfg["load_label"] = (loadIdx >= 0)
                              ? m_loadCombo->currentText().section("  ", 1)
                              : QString();

    // Relay
    const int relayIdx = m_relayCombo->currentData().toInt();
    m_cfg["relay_index"] = relayIdx;
    m_cfg["relay_label"] = (relayIdx >= 0 && relayIdx < m_relayOptions.size())
                               ? m_relayOptions[relayIdx].label
                               : QString();
    if (relayIdx >= 0 && relayIdx < m_relayOptions.size()) {
        QStringList vl;
        for (const auto& v : m_relayOptions[relayIdx].values) vl << v;
        m_cfg["relay_values"] = vl;
    } else {
        m_cfg.remove("relay_values");
    }

    // Discharge
    const int dischargeIdx = m_dischargeCombo->currentData().toInt();
    m_cfg["discharge_index"] = dischargeIdx;
    m_cfg["discharge_label"] = (dischargeIdx >= 0 && dischargeIdx < m_relayOptions.size())
                                   ? m_relayOptions[dischargeIdx].label
                                   : QString();
    if (dischargeIdx >= 0 && dischargeIdx < m_relayOptions.size()) {
        QStringList vl;
        for (const auto& v : m_relayOptions[dischargeIdx].values) vl << v;
        m_cfg["discharge_values"] = vl;
    } else {
        m_cfg.remove("discharge_values");
    }

    // Duration
    m_cfg["delay_ms"] = m_spinMs->value();
}
