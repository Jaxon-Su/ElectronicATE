#include "relaydialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include <QPushButton>
#include <QFont>
#include <QDebug>

static const char* RELAY_DLG_STYLE = R"(
    QDialog { background: #f4f7fb; }
    QLabel#titleLbl {
        font-size: 15px; font-weight: bold; color: #1a2a4a;
    }
    QLabel#badge {
        font-size: 10px; font-weight: bold;
        color: #fff; background: #8e44ad;
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
    QComboBox {
        background: #ffffff; border: 1px solid #c8d8ee;
        border-radius: 3px; padding: 3px 8px;
        min-height: 26px; font-size: 12px; color: #1a2a4a;
        min-width: 180px;
    }
    QComboBox:focus { border-color: #8e44ad; }
    QComboBox::drop-down {
        subcontrol-origin: padding; subcontrol-position: top right;
        width: 22px; border-left: 1px solid #c8d8ee;
    }
    QComboBox QAbstractItemView {
        background: #fff; border: 1px solid #c8d8ee;
        selection-background-color: #f3e8ff; selection-color: #6c3483;
        font-size: 12px;
    }
    QTableWidget {
        background: #ffffff; border: 1px solid #dce6f1;
        border-radius: 3px; gridline-color: #e8eef6; font-size: 11px;
    }
    QHeaderView::section {
        background: #edf2fb; border: none;
        border-right: 1px solid #dce6f1; border-bottom: 1px solid #dce6f1;
        padding: 3px 6px; font-size: 10px; font-weight: bold; color: #2c3e6a;
    }
    QTableWidget::item { padding: 2px 6px; color: #2c3e50; }
    QPushButton#applyBtn {
        background: #7d3c98; color: white;
        border: none; border-radius: 4px;
        padding: 6px 24px; font-size: 12px; font-weight: bold;
    }
    QPushButton#applyBtn:hover   { background: #6c3483; }
    QPushButton#applyBtn:pressed { background: #5b2c6f; }
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
RelayDialog::RelayDialog(const QVector<RelayOption>& relayOptions,
                         const QVariantMap&           initCfg,
                         int                          seqNo,
                         QWidget*                     parent)
    : QDialog(parent)
    , m_seqNo(seqNo)
    , m_relayOptions(relayOptions)
    , m_cfg(initCfg)
{
    setWindowTitle(QString("Row %1 — Relay").arg(seqNo));
    setStyleSheet(RELAY_DLG_STYLE);
    buildUI();
    restoreFromCfg();
}

// ══════════════════════════════════════════════════════
//  buildUI
// ══════════════════════════════════════════════════════
void RelayDialog::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // ── 標題列 + Badge ───────────────────────────────
    auto* headerRow = new QHBoxLayout;
    auto* titleLbl  = new QLabel("Relay");
    titleLbl->setObjectName("titleLbl");
    headerRow->addWidget(titleLbl);
    headerRow->addStretch();
    auto* badge = new QLabel("RELAY");
    badge->setObjectName("badge");
    headerRow->addWidget(badge);
    root->addLayout(headerRow);

    // ── 分隔線 ──────────────────────────────────────
    auto* sep = new QFrame;
    sep->setObjectName("sep");
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    // ── 選擇區 ───────────────────────────────────────
    auto* selGrp = new QGroupBox("Relay Selection");
    auto* form   = new QFormLayout(selGrp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    m_relayCombo = new QComboBox;
    m_relayCombo->addItem("— None —", -1);
    for (int i = 0; i < m_relayOptions.size(); ++i)
        m_relayCombo->addItem(
            QString("%1.  %2").arg(i + 1).arg(m_relayOptions[i].label), i);

    form->addRow("Relay :", m_relayCombo);
    root->addWidget(selGrp);
    root->addStretch();

    // ── ComboBox 變更（無預覽，僅更新 config）────────
    connect(m_relayCombo, &QComboBox::currentIndexChanged,
            this, [this](int) { /* 選擇後直接存入 config，無需預覽 */ });

    // ── 按鈕列 ──────────────────────────────────────
    auto* btnRow   = new QHBoxLayout;
    auto* applyBtn = new QPushButton("Apply");
    auto* closeBtn = new QPushButton("Close");
    applyBtn->setObjectName("applyBtn");
    closeBtn->setObjectName("closeBtn");
    applyBtn->setFixedWidth(100);
    closeBtn->setFixedWidth(100);

    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        saveConfig();
        qDebug() << "[RelayDialog] Applied:" << m_cfg;
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

    setMinimumSize(420, 300);
}

// updatePreviewTable — 預覽已移除，保留空實作避免連結錯誤
void RelayDialog::updatePreviewTable(int /*relayIdx*/)
{
    // Preview table removed
}

// ══════════════════════════════════════════════════════
//  restoreFromCfg
// ══════════════════════════════════════════════════════
void RelayDialog::restoreFromCfg()
{
    const int savedIdx = m_cfg.value("relay_index", -1).toInt();
    if (savedIdx < 0) return;
    for (int i = 0; i < m_relayCombo->count(); ++i) {
        if (m_relayCombo->itemData(i).toInt() == savedIdx) {
            m_relayCombo->setCurrentIndex(i);
            break;
        }
    }
}

// ══════════════════════════════════════════════════════
//  saveConfig
// ══════════════════════════════════════════════════════
void RelayDialog::saveConfig()
{
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
}
