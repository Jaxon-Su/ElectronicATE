#include "delaydialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QSpinBox>
#include <QPushButton>
#include <QDebug>

// ══════════════════════════════════════════════════════
//  共用樣式（與 oscwritedialogbase 風格一致）
// ══════════════════════════════════════════════════════
static const char* DELAY_DLG_STYLE = R"(
    QDialog { background: #f4f7fb; }
    QLabel#titleLbl {
        font-size: 15px; font-weight: bold; color: #1a2a4a;
    }
    QLabel#taskBadge {
        font-size: 10px; font-weight: bold;
        color: #fff; background: #3a7bd5;
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

    /* SpinBox 本體 */
    QSpinBox {
        background: #ffffff; border: 1px solid #c8d8ee;
        border-radius: 3px; padding: 2px 6px;
        min-height: 26px; font-size: 13px; color: #1a2a4a;
        padding-right: 20px;
    }
    QSpinBox:focus { border-color: #3a7bd5; }

    /* SpinBox 箭頭按鈕：只指定位置與尺寸，讓系統渲染 ▲▼ */
    QSpinBox::up-button {
        subcontrol-origin: border;
        subcontrol-position: top right;
        width: 18px; height: 14px;
    }
    QSpinBox::down-button {
        subcontrol-origin: border;
        subcontrol-position: bottom right;
        width: 18px; height: 14px;
    }

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
DelayDialog::DelayDialog(const QVariantMap& initCfg,
                         int                seqNo,
                         QWidget*           parent)
    : QDialog(parent)
    , m_cfg(initCfg)
{
    setWindowTitle(QString("Row %1 — Delay").arg(seqNo));
    setStyleSheet(DELAY_DLG_STYLE);
    buildUI();
}

// ══════════════════════════════════════════════════════
//  buildUI
// ══════════════════════════════════════════════════════
void DelayDialog::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // ── 標題列 + badge ──────────────────────────────
    auto* headerRow = new QHBoxLayout;
    auto* titleLbl  = new QLabel("Delay");
    titleLbl->setObjectName("titleLbl");
    headerRow->addWidget(titleLbl);
    headerRow->addStretch();

    auto* badge = new QLabel("DELAY");
    badge->setObjectName("taskBadge");
    headerRow->addWidget(badge);
    root->addLayout(headerRow);

    // ── 分隔線 ──────────────────────────────────────
    auto* sep = new QFrame;
    sep->setObjectName("sep");
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    // ── 設定區 ──────────────────────────────────────
    auto* grp  = new QGroupBox("Delay Settings");
    auto* form = new QFormLayout(grp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    // SpinBox：單位 ms，步進 1000ms，範圍 0 ~ 3,600,000ms（1小時）
    m_spinMs = new QSpinBox;
    m_spinMs->setRange(0, 3'600'000);
    m_spinMs->setSingleStep(1000);          // ★ 滾輪 / 箭頭每次 1000ms
    m_spinMs->setSuffix(" ms");
    m_spinMs->setValue(m_cfg.value("delay_ms", 5000).toInt());
    m_spinMs->setAccelerated(true);         // 長按箭頭加速
    m_spinMs->setMinimumWidth(180);

    form->addRow("Duration:", m_spinMs);

    // 提示文字
    auto* hint = new QLabel("Step: 1000 ms　｜　Range: 0 ms ~ 3,600,000 ms (1 hr)");
    hint->setStyleSheet("font-size: 10px; color: #8899bb;");
    form->addRow("", hint);

    root->addWidget(grp);
    root->addStretch();

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
        qDebug() << "[DelayDialog] Applied:" << m_cfg;
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

    setMinimumSize(360, 240);
}

// ══════════════════════════════════════════════════════
//  saveConfig
// ══════════════════════════════════════════════════════
void DelayDialog::saveConfig()
{
    if (!m_spinMs) return;
    m_cfg["delay_ms"] = m_spinMs->value();
}
