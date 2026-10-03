#include "tasksettingrules.h"
#include "../dialogstyle.h"
#include "../inputselectors.h"
#include "turnonoffdialog.h"
#include "transientsettingswidget.h"
#include <QScrollArea>

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

//  樣式（與 DelayDialog / OscWriteDialog 風格一致）
static const char* TURN_DLG_STYLE = R"(
    QDialog { background: #f4f7fb; }

    QLabel#titleLbl {
        font-weight: bold; color: #1a2a4a;
    }
    QLabel#badgeTurnOn {
        font-weight: bold;
        color: #fff; background: #27ae60;
        border-radius: 8px; padding: 2px 8px;
    }
    QLabel#badgeTurnOff {
        font-weight: bold;
        color: #fff; background: #e74c3c;
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

    /* SpinBox */
    QSpinBox {
        background: #ffffff; border: 1px solid #c8d8ee;
        border-radius: 3px; padding: 2px 6px;
        min-height: 26px; color: #1a2a4a;
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
    QLabel#hintLbl { color: #8899bb; }

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
TurnOnOffDialog::TurnOnOffDialog(Mode mode, const QStringList& inputOptions, const QStringList& loadOptions,
                                 const QVector<RelayOption>& relayOptions, const QVariantMap& initCfg,
                                 int seqNo, QWidget* parent, Selection selection)
    : QDialog(parent), m_mode(mode), m_seqNo(seqNo), m_relayOptions(relayOptions), m_cfg(initCfg), m_selection(selection)
{
    const QString title = (mode == TurnOn) ? "Turn On" : "Turn Off";
    setWindowTitle(QString("Row %1 — %2").arg(seqNo).arg(title + (m_selection == Selection::Group ? " Group Test" : "")));
    setStyleSheet(Page5DialogStyle::withTypography(TURN_DLG_STYLE));
    buildUI(inputOptions, loadOptions, relayOptions);
    restoreFromCfg();
}

//  buildUI
void TurnOnOffDialog::buildUI(const QStringList& inputOptions, const QStringList& loadOptions,
                              const QVector<RelayOption>& relayOptions)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // 標題列 + Badge
    auto* headerRow = new QHBoxLayout;
    auto* titleLbl = new QLabel((m_mode == TurnOn) ? "Turn On" : "Turn Off");
    titleLbl->setObjectName("titleLbl");
    if (m_selection == Selection::Group)
        titleLbl->setText(titleLbl->text() + " Group Test");
    headerRow->addWidget(titleLbl);
    headerRow->addStretch();

    auto* badge = new QLabel((m_mode == TurnOn) ? "TURN ON" : "TURN OFF");
    badge->setObjectName((m_mode == TurnOn) ? "badgeTurnOn" : "badgeTurnOff");
    headerRow->addWidget(badge);
    root->addLayout(headerRow);

    // 分隔線
    auto* sep = new QFrame;
    sep->setObjectName("sep");
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    //  設定區：Input 選項 + Load 選項
    auto* grp = new QGroupBox("Settings");
    auto* form = new QFormLayout(grp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);

    // Input Table 選項
    m_inputCombo = new QComboBox;
    m_inputCombo->addItem("— None —", -1);
    for (int i = 0; i < inputOptions.size(); ++i)
        if (!inputOptions[i].trimmed().isEmpty())
            m_inputCombo->addItem(QString("%1.  %2").arg(i + 1).arg(inputOptions[i]),
                                  i); // userData = 索引值

    addInputSelectors(form, m_inputCombo, inputOptions);

    // Input 預覽（Vin / Freq / Phase）
    auto* inputPreview = new QLabel("—");
    inputPreview->setObjectName("previewLbl");
    form->addRow("", inputPreview);

    // Load Table 選項
    m_loadSelection = new LoadConditionSelector(loadOptions, "load", m_selection, this);
    form->addRow(m_selection == Selection::Group ? "Loads :" : "Load :", m_loadSelection);

    // Discharge（TurnOn 專用）
    if (m_mode == TurnOn) {
        m_dischargeCombo = new QComboBox;
        m_dischargeCombo->addItem("— None —", -1);
        for (int i = 0; i < relayOptions.size(); ++i)
            m_dischargeCombo->addItem(QString("%1.  %2").arg(i + 1).arg(relayOptions[i].label), i);
        form->addRow("Discharge :", m_dischargeCombo);

        auto* dischargePreview = new QLabel("—");
        dischargePreview->setObjectName("previewLbl");
        form->addRow("", dischargePreview);

        connect(m_dischargeCombo, &QComboBox::currentIndexChanged, this, [this, dischargePreview](int) {
            const int dataIdx = m_dischargeCombo->currentData().toInt();
            if (dataIdx < 0 || dataIdx >= m_relayOptions.size()) {
                dischargePreview->setText("—");
                return;
            }
            dischargePreview->setText(QString("Discharge:  %1").arg(m_relayOptions[dataIdx].label));
        });
    }

    // Duration（TurnOff 專用）
    if (m_mode == TurnOff) {
        m_spinMs = new QSpinBox;
        m_spinMs->setRange(TaskSettingRules::settle.minimum, TaskSettingRules::settle.maximum);
        m_spinMs->setSingleStep(1000);
        m_spinMs->setSuffix(" ms");
        m_spinMs->setValue(m_cfg.value(TaskSettingRules::settle.key, TaskSettingRules::settle.initial).toInt());
        m_spinMs->setAccelerated(true);
        m_spinMs->setMinimumWidth(180);
        form->addRow("Settle time :", m_spinMs);

        auto* hint = new QLabel("每次開機後等待穩定，再抓穩態參考並測試關機。\nStep: 1000 ms　｜　Range: 0 "
                                "ms ~ 3,600,000 ms (1 hr)");
        hint->setWordWrap(true);
        m_spinMs->setToolTip("每次開機後等待穩定，再抓穩態參考並測試關機。");
        hint->setObjectName("hintLbl");
        form->addRow("", hint);
    }

    m_triggerSettings = new TransientSettingsWidget(m_cfg, m_mode == TurnOn, m_mode == TurnOn);
    form->addRow(m_triggerSettings);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(grp);
    root->addWidget(scroll, 1);
    root->addStretch();

    // 動態預覽 — Input
    connect(m_inputCombo, &QComboBox::currentIndexChanged, this, [this, inputOptions, inputPreview](int idx) {
        const int dataIdx = m_inputCombo->currentData().toInt();
        if (dataIdx < 0 || dataIdx >= inputOptions.size()) {
            inputPreview->setText("—");
            return;
        }
        // 格式："Vin/Freq/Phase" 拆解顯示
        const QStringList parts = inputOptions[dataIdx].split('/');
        if (parts.size() == 4)
            inputPreview->setText(
                QString("Vin: %1  Frequency: %2  Phase: %3").arg(parts[1], parts[2], parts[3]));
        else
            inputPreview->setText(inputOptions[dataIdx]);
    });

    // 動態預覽 — Load

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
        qDebug() << "[TurnOnOffDialog] Applied:" << m_cfg;
        accept();
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnRow->addStretch();
    btnRow->addWidget(applyBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    setMinimumSize(m_selection == Selection::Group ? 560 : 420,
                   m_selection == Selection::Group ? 700 : 490);
}

//  restoreFromCfg  — 從上次儲存的設定還原 ComboBox
void TurnOnOffDialog::restoreFromCfg()
{
    // 還原 Input 選擇
    const int inputIdx = m_cfg.value("input_index", -1).toInt();
    if (inputIdx >= 0) {
        for (int i = 0; i < m_inputCombo->count(); ++i) {
            if (m_inputCombo->itemData(i).toInt() == inputIdx) {
                m_inputCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // 還原 Load 選擇
    m_loadSelection->restore(m_cfg);

    // 還原 Discharge 選擇（TurnOn 專用）
    if (m_dischargeCombo) {
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
}

//  saveConfig
void TurnOnOffDialog::saveConfig()
{
    m_triggerSettings->save(m_cfg);
    // Input
    const int inputIdx = m_inputCombo->currentData().toInt();
    const QString inputLbl = (inputIdx >= 0)
                                 ? m_inputCombo->currentText().section("  ", 1) // 去掉 "1.  " 前綴
                                 : QString();
    m_cfg["input_index"] = inputIdx;
    m_cfg["input_label"] = inputLbl;

    // Load
    m_loadSelection->save(m_cfg);

    if (m_spinMs)
        m_cfg["delay_ms"] = m_spinMs->value();

    // Discharge（TurnOn 專用）
    if (m_dischargeCombo) {
        const int dischargeIdx = m_dischargeCombo->currentData().toInt();
        m_cfg["discharge_index"] = dischargeIdx;
        m_cfg["discharge_label"] = (dischargeIdx >= 0 && dischargeIdx < m_relayOptions.size())
                                       ? m_relayOptions[dischargeIdx].label
                                       : QString();
        if (dischargeIdx >= 0 && dischargeIdx < m_relayOptions.size()) {
            QStringList vl;
            for (const auto& v : m_relayOptions[dischargeIdx].values)
                vl << v;
            m_cfg["discharge_values"] = vl;
        } else {
            m_cfg.remove("discharge_values");
        }
    }
}
