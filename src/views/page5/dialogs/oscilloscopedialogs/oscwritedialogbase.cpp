#include "oscwritedialogbase.h"
#include "oscilloscope.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QComboBox>
#include <QDebug>

// ══════════════════════════════════════════════════════
//  共用樣式（所有子類 Dialog 共用同一份）
// ══════════════════════════════════════════════════════
static const char* OSC_DLG_STYLE = R"(
    QDialog { background: #f4f7fb; }
    QLabel#titleLbl {
        font-size: 15px; font-weight: bold; color: #1a2a4a;
    }
    QLabel#subLbl { font-size: 11px; color: #6b7a99; }
    QLabel#modelBadge {
        font-size: 10px; font-weight: bold;
        color: #fff; background: #3a7bd5;
        border-radius: 8px; padding: 2px 8px;
    }
    QFrame#sep { color: #dce6f1; }
    QScrollArea { background: transparent; border: none; }
    QFrame#card {
        background: #ffffff;
        border: 1px solid #dce6f1;
        border-radius: 6px;
    }
    QGroupBox {
        font-weight: bold; font-size: 11px; color: #2c3e6a;
        border: 1px solid #dce6f1; border-radius: 5px;
        margin-top: 8px; padding-top: 6px;
    }
    QGroupBox::title {
        subcontrol-origin: margin; left: 8px; top: 0px; padding: 0 4px;
    }
    QComboBox, QDoubleSpinBox, QLineEdit {
        background: #ffffff; border: 1px solid #c8d8ee;
        border-radius: 3px; padding: 2px 6px;
        min-height: 26px; font-size: 11px; color: #1a2a4a;
    }
    QComboBox:focus, QDoubleSpinBox:focus { border-color: #3a7bd5; }
    QComboBox:disabled, QDoubleSpinBox:disabled, QLineEdit:disabled {
        background: #f0f3f8; color: #aaa;
    }

    /* ★ 修復：明確定義 spinbox 上下箭頭按鈕，否則 Qt 在 Windows 上渲染為 0px 高度 */
    QDoubleSpinBox {
        padding-right: 20px;   /* 為按鈕保留右側空間 */
    }
    /* ★ 只指定位置與尺寸，背景/箭頭圖示完全交給 Qt 系統渲染
          這樣 ▲▼ 才會正確顯示 */
    QDoubleSpinBox::up-button {
        subcontrol-origin: border;
        subcontrol-position: top right;
        width: 18px;
        height: 14px;
    }
    QDoubleSpinBox::down-button {
        subcontrol-origin: border;
        subcontrol-position: bottom right;
        width: 18px;
        height: 14px;
    }
    /* ★ 不覆寫 ::up-arrow / ::down-arrow，完全交給 Qt 渲染系統箭頭圖示 */

    QCheckBox { font-size: 11px; color: #2c3e50; }
    QLabel    { font-size: 11px; color: #2c3e50; }
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
OscWriteDialogBase::OscWriteDialogBase(Oscilloscope*      scope,
                                       const QString&     configuredModel,
                                       const QVariantMap& initCfg,
                                       int                seqNo,
                                       const QString&     extName,
                                       QWidget*           parent)
    : QDialog(parent)
    , m_scope(scope)
    , m_configuredModel(configuredModel)
    , m_cfg(initCfg)
    , m_model(scope ? scope->model() : configuredModel)
{
    setWindowTitle(QString("Row %1 — Write Oscilloscope").arg(seqNo));
    setStyleSheet(OSC_DLG_STYLE);
    Q_UNUSED(extName)
    // ★ 不在此呼叫 buildFrame()，由子類 constructor 末尾呼叫
}

// ══════════════════════════════════════════════════════
//  buildFrame — 組裝公共 UI 框架
//
//  呼叫時機：子類 constructor 末尾，子類 widget 成員初始化完畢後
//  原因：此函式呼叫 buildContentWidget()（虛函式），
//        必須等子類完全建構後 virtual dispatch 才能正確解析
// ══════════════════════════════════════════════════════
void OscWriteDialogBase::buildFrame(const QSize& minSize, bool hasSettings)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // ── 標題列 + 型號 badge ──────────────────────────
    auto* headerRow = new QHBoxLayout;
    auto* titleLbl  = new QLabel("Write Oscilloscope");
    titleLbl->setObjectName("titleLbl");
    headerRow->addWidget(titleLbl);
    headerRow->addStretch();

    if (!m_model.isEmpty()) {
        auto* badge = new QLabel(m_model);
        badge->setObjectName("modelBadge");
        headerRow->addWidget(badge);
    }
    root->addLayout(headerRow);

    // ── 連線狀態（已配置才顯示）────────────────────
    // if (!m_configuredModel.isEmpty()) {
    //     if (!m_scope) {
    //         auto* subLbl = new QLabel(
    //             QString("Configured: %1  —  not connected yet").arg(m_configuredModel));
    //         subLbl->setObjectName("subLbl");
    //         root->addWidget(subLbl);
    //         auto* banner = new QFrame;
    //         banner->setStyleSheet(
    //             "QFrame { background:#fff8e1; border:1px solid #f0c040; border-radius:4px; }");
    //         auto* bannerLay = new QHBoxLayout(banner);
    //         bannerLay->setContentsMargins(10, 6, 10, 6);
    //         auto* warnLbl = new QLabel(
    //             "⚠  No oscilloscope connected. "
    //             "You can pre-configure settings below; "
    //             "they will be sent when the instrument is connected.");
    //         warnLbl->setStyleSheet("font-size:11px; color:#7a5000; background:transparent;");
    //         warnLbl->setWordWrap(true);
    //         bannerLay->addWidget(warnLbl);
    //         root->addWidget(banner);
    //     } else {
    //         auto* subLbl = new QLabel(
    //             QString("Connected: %1 (%2)").arg(m_model, m_scope->vendor()));
    //         subLbl->setObjectName("subLbl");
    //         root->addWidget(subLbl);
    //     }
    // }

    // ── 分隔線 ──────────────────────────────────────
    auto* sep = new QFrame;
    sep->setObjectName("sep");
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    // ── 設定 UI 主體（由子類 buildContentWidget() 提供）
    root->addWidget(buildContentWidget(), 1);

    // ── 按鈕列 ──────────────────────────────────────
    auto* btnRow   = new QHBoxLayout;
    auto* closeBtn = new QPushButton("Close");
    closeBtn->setObjectName("closeBtn");
    closeBtn->setFixedWidth(100);
    btnRow->addStretch();

    if (hasSettings) {
        auto* applyBtn = new QPushButton("Apply");
        applyBtn->setObjectName("applyBtn");
        applyBtn->setFixedWidth(100);
        connect(applyBtn, &QPushButton::clicked, this, [this]() {
            saveConfig();
            qDebug() << "[OscWriteDialog] Applied:" << m_cfg;
            accept();   // ★ 儲存後關閉視窗
        });
        btnRow->addWidget(applyBtn);
    }

    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        saveConfig();
        accept();
    });
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    setMinimumSize(minSize);
}

// ══════════════════════════════════════════════════════
//  Helper
// ══════════════════════════════════════════════════════
QComboBox* OscWriteDialogBase::makeCombo(const QStringList& items, const QString& current)
{
    auto* cb = new QComboBox;
    cb->addItems(items);
    const int idx = items.indexOf(current);
    cb->setCurrentIndex(idx >= 0 ? idx : 0);
    return cb;
}
