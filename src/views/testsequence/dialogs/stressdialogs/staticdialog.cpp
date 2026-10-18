#include "tasksettingrules.h"
#include "../strategycapturewidget.h"
#include "../dialogstyle.h"
#include "../inputselectors.h"
#include "staticdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDebug>

static const char* STATIC_DLG_STYLE = R"(
    QDialog { background: #f4f7fb; }

    QLabel#titleLbl {
        font-weight: bold; color: #1a2a4a;
    }
    QLabel#badgeStatic {
        font-weight: bold;
        color: #fff; background: #8e44ad;
        border-radius: 8px; padding: 2px 8px;
    }
    QFrame#sep { color: #dce6f1; }

    QGroupBox {
        font-weight: bold; color: #2c3e6a;
        border: 1px solid #dce6f1; border-radius: 5px;
        margin-top: 8px; padding-top: 6px;
    }
    QGroupBox::title {
        subcontrol-origin: margin; left: 8px; top: 0px; padding: 0 4px;
    }
    QLabel { color: #2c3e50; }

    /* ComboBox */
    QComboBox {
        background: #ffffff; border: 1px solid #c8d8ee;
        border-radius: 3px; padding: 3px 8px;
        min-height: 26px; color: #1a2a4a;
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
        }

    /* 預覽標籤 */
    QLabel#previewLbl {
        color: #8899bb;
        padding: 2px 0px;
    }

    /* SpinBox 本體 */
    QSpinBox {
        background: #ffffff; border: 1px solid #c8d8ee;
        border-radius: 3px; padding: 2px 6px;
        min-height: 26px; color: #1a2a4a;
        padding-right: 20px;
    }
    QSpinBox:focus { border-color: #3a7bd5; }
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

    /* 提示文字 */
    QLabel#hintLbl {
        color: #8899bb;
    }

    QPushButton#applyBtn {
        background: #5a8cdd; color: white;
        border: none; border-radius: 4px;
        padding: 6px 24px; font-weight: bold;
    }
    QPushButton#applyBtn:hover   { background: #4a7ccd; }
    QPushButton#applyBtn:pressed { background: #3a6cbb; }
    QPushButton#closeBtn {
        background: #3a7bd5; color: white;
        border: none; border-radius: 4px;
        padding: 6px 24px; font-weight: bold;
    }
    QPushButton#closeBtn:hover   { background: #4a8be5; }
    QPushButton#closeBtn:pressed { background: #2a6bc5; }
)";

//  Constructor
StaticDialog::StaticDialog(const InputConditionOptions& inputOptions, const QStringList& loadOptions,
                           const QVariantMap& initCfg, int seqNo, QWidget* parent, Selection selection)
    : QDialog(parent), m_cfg(initCfg), m_selection(selection)
{
    setWindowTitle(QString("Row %1 — %2").arg(seqNo).arg(m_selection == Selection::Group ? "Static Group Test" : "Static Test"));
    setStyleSheet(TestSequenceDialogStyle::withTypography(STATIC_DLG_STYLE));
    buildUI(inputOptions, loadOptions);
    restoreFromCfg();
}

//  buildUI
void StaticDialog::buildUI(const InputConditionOptions& inputOptions, const QStringList& loadOptions)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // 標題列 + Badge
    auto* headerRow = new QHBoxLayout;
    auto* titleLbl = new QLabel("Static");
    titleLbl->setObjectName("titleLbl");
    if (m_selection == Selection::Group)
        titleLbl->setText(titleLbl->text() + " Group Test");
    headerRow->addWidget(titleLbl);
    headerRow->addStretch();

    auto* badge = new QLabel("STATIC");
    badge->setObjectName("badgeStatic");
    headerRow->addWidget(badge);
    root->addLayout(headerRow);

    // 分隔線
    auto* sep = new QFrame;
    sep->setObjectName("sep");
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    //  設定區
    auto* grp = new QGroupBox("Settings");
    auto* form = new QFormLayout(grp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);

    // Input Table 選項
    m_inputs = new InputSelectors(form, inputOptions, m_cfg);

    // Load Table 選項
    m_loadSelection = new LoadConditionSelector(loadOptions, "load", m_selection, this);
    form->addRow(m_selection == Selection::Group ? "Loads :" : "Load :", m_loadSelection);

    // Duration
    m_targetCombo = new QComboBox;
    m_targetCombo->setObjectName("measureTarget");
    m_targetCombo->addItem("BOTH", "BOTH");
    m_targetCombo->addItem("Rise", "MAX");
    m_targetCombo->addItem("Fall", "MIN");
    m_targetCombo->setCurrentIndex(qMax(0, m_targetCombo->findData(m_cfg.value("measure_target", "BOTH").toString().trimmed().toUpper())));
    m_targetCombo->setToolTip("BOTH: Rise then Fall. Every valid trial records Max/Min/RMS/Mean. Rise selects the highest-Max record; Fall selects the lowest-Min record.");
    form->addRow("Search direction:", m_targetCombo);

    m_capture = new StrategyCaptureWidget(m_cfg, this);
    form->addRow("", m_capture);
    m_autoPeriod = new QCheckBox("Auto Period (target 5 cycles)");
    m_autoPeriod->setObjectName("autoPeriod");
    m_autoPeriod->setChecked(m_cfg.value("auto_period", false).toBool());
    m_autoPeriod->setToolTip("After Input/Load ON and settling, use AUTO trigger to measure the trigger channel period and adjust Timescale to 5 cycles (accept 3-8). Applies only to this task.");
    form->addRow("", m_autoPeriod);

    auto addWait = [&](const char *label, const TaskSettingRules::Integer &rule) {
        auto *spin = new QSpinBox;
        spin->setObjectName(rule.key);
        spin->setRange(rule.minimum, rule.maximum);
        spin->setSingleStep(1000);
        spin->setSuffix(" ms");
        spin->setValue(m_cfg.value(rule.key, rule.initial).toInt());
        spin->setAccelerated(true);
        spin->setMinimumWidth(180);
        form->addRow(label, spin);
        return spin;
    };
    m_powerOffMs = addWait("Power-off wait:", TaskSettingRules::powerOffWait);
    m_startupMs = addWait("Startup wait:", TaskSettingRules::startupWait);
    m_spinMs = addWait("Load settle time:", TaskSettingRules::settle);
    m_powerOffMs->setToolTip("確認 Load、Input OFF 後，等待再設定下一筆。請依 DUT 關機／放電時間設定。");
    m_startupMs->setToolTip("Input ON 後，等待 DUT 啟動再開啟 Load。");
    m_spinMs->setToolTip("Load ON 後，等待穩定再開始量測。");
    auto *hint = new QLabel("每筆（含 Group）先確認 Load／Input OFF，再設定、Input ON、Load ON、量測。\n"
                           "量測後立即 Load OFF → Input OFF。等待時間需依 DUT 設定，並非電壓歸零偵測。");
    hint->setWordWrap(true);
    hint->setObjectName("hintLbl");
    form->addRow("", hint);

    root->addWidget(grp);
    root->addStretch();

    // 按鈕列
    auto* btnRow = new QHBoxLayout;
    auto* applyBtn = new QPushButton("Apply");
    auto* closeBtn = new QPushButton("Close");
    applyBtn->setObjectName("applyBtn");
    closeBtn->setObjectName("closeBtn");
    applyBtn->setFixedWidth(100);
    closeBtn->setFixedWidth(100);

    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        saveConfig();
        qDebug() << "[StaticDialog] Applied:" << m_cfg;
        accept();
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnRow->addStretch();
    btnRow->addWidget(applyBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    setMinimumSize(m_selection == Selection::Group ? 520 : 400, 340);
    if (m_selection == Selection::Group) {
        ensurePolished();
        setMinimumHeight(sizeHint().height());
    }
}

//  restoreFromCfg
void StaticDialog::restoreFromCfg()
{
    // 還原 Load
    m_loadSelection->restore(m_cfg);

    // 還原 Duration（spinbox 已在 buildUI 中由 m_cfg 初始化，此處無需重複）
}

//  saveConfig
void StaticDialog::saveConfig()
{
    // Input
    m_inputs->save(m_cfg);

    // Load
    m_loadSelection->save(m_cfg);

    // Duration
    m_cfg["delay_ms"] = m_spinMs->value();
    m_cfg["power_off_ms"] = m_powerOffMs->value();
    m_cfg["startup_ms"] = m_startupMs->value();
    m_capture->save(m_cfg);
    m_cfg["auto_period"] = m_autoPeriod->isChecked();
    m_cfg["measure_target"] = m_targetCombo->currentData().toString();
}
