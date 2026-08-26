#include "msoseries456triggerwidget.h"
#include "msoseries456triggercontroller.h"
#include "smartstepspinbox.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QFont>

// ─────────────────────────────────────────────────────────────────────────────
//  建構子
//  totalChannels: 依機型傳入 4 / 6 / 8，決定 Source ComboBox 的項目數
// ─────────────────────────────────────────────────────────────────────────────
MSOSeries456TriggerWidget::MSOSeries456TriggerWidget(int totalChannels, QWidget* parent)
    : QGroupBox(tr("Trigger"), parent)
    , m_totalChannels(totalChannels)
{
    setupUI();
    m_controller = new MSOSeries456TriggerController(this, this);
}

MSOSeries456TriggerWidget::~MSOSeries456TriggerWidget()
{
    cleanup();
}

void MSOSeries456TriggerWidget::cleanup()
{
    if (m_controller) {
        m_controller->disconnect();
        if (!m_controller->parent())
            delete m_controller;
        m_controller = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  setChannelCount：動態調整 Source ComboBox（建立後仍可呼叫）
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerWidget::setChannelCount(int totalChannels)
{
    if (totalChannels == m_totalChannels) return;
    m_totalChannels = totalChannels;
    populateSourceComboBox();
}

// ─────────────────────────────────────────────────────────────────────────────
//  setupUI
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerWidget::setupUI()
{
    setFont(QFont(font().family(), 9, QFont::Bold));
    setFixedWidth(280);

    createControls();
    setupLayout();
}

// ─────────────────────────────────────────────────────────────────────────────
//  createControls：建立所有 UI 元件
//  objectName 與 DPO7000TriggerWidget 相同，確保 Controller::findChild 有效
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerWidget::createControls()
{
    // ── Trigger Type ──────────────────────────────────────────────────────
    // MSO 4/5/6 支援: EDGE / WIDth / TIMEOut / RUNt / WINdow / LOGIc /
    //                 SETHold / TRANsition / BUS
    // 此處僅提供 EDGE（與 DPO7000 對齊），後續可依需求擴充
    m_cmbTrigType = new QComboBox;
    m_cmbTrigType->setObjectName("triggerType");
    m_cmbTrigType->addItems({ tr("EDGE") });
    for (int i = 0; i < m_cmbTrigType->count(); ++i)
        m_cmbTrigType->setItemData(i, Qt::AlignCenter, Qt::TextAlignmentRole);

    // ── Trigger Source（通道數由 m_totalChannels 決定）─────────────────
    m_cmbTrigSource = new QComboBox;
    m_cmbTrigSource->setObjectName("triggerSource");
    populateSourceComboBox();   // 填入 CH1~CHn

    // ── Slope 按鈕 ────────────────────────────────────────────────────────
    m_btnTrigRising = new QPushButton(tr("↑"));
    m_btnTrigRising->setObjectName("triggerRising");

    m_btnTrigFalling = new QPushButton(tr("↓"));
    m_btnTrigFalling->setObjectName("triggerFalling");

    m_btnTrigBoth = new QPushButton(tr("↑↓"));
    m_btnTrigBoth->setObjectName("triggerBoth");

    // ── Mode 按鈕 ─────────────────────────────────────────────────────────
    m_btnTrigAuto = new QPushButton(tr("Auto"));
    m_btnTrigAuto->setObjectName("triggerAuto");

    m_btnTrigNorm = new QPushButton(tr("Norm"));
    m_btnTrigNorm->setObjectName("triggerNorm");

    m_btnTrigSingle = new QPushButton(tr("Single"));
    m_btnTrigSingle->setObjectName("triggerSingle");

    // ── 控制按鈕 ──────────────────────────────────────────────────────────
    m_btnTrigSet = new QPushButton(tr("Set"));
    m_btnTrigSet->setObjectName("triggerSet");

    m_btnRunstop = new QPushButton(tr("Run/Stop"));
    m_btnRunstop->setObjectName("runStop");

    // ── 即時量測 ──────────────────────────────────────────────────────────
    m_btnGetValue = new QPushButton(tr("Get Value"));
    m_btnGetValue->setObjectName("getValue");

    auto makeValueLabel = [](const QString& objectName) {
        auto* label = new QLabel("--");
        label->setObjectName(objectName);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setMinimumWidth(96);
        label->setStyleSheet("QLabel { background:#f7f7f7; border:1px solid #ddd; padding:2px 4px; }");
        return label;
    };

    m_lblMeasMax     = makeValueLabel("measureMax");
    m_lblMeasMin     = makeValueLabel("measureMin");
    m_lblMeasRms     = makeValueLabel("measureRms");
    m_lblMeasMean    = makeValueLabel("measureMean");
    m_lblMeasAbsPeak = makeValueLabel("measureAbsPeak");

    // ── 半自動觸發按鈕（預設隱藏，由外部決定是否顯示）──────────────────
    m_btntrig_Steady = new QPushButton(tr("Semi-Auto Trigger OFF"));
    m_btntrig_Steady->setObjectName("btntrig_Steady");
    m_btntrig_Steady->setCheckable(true);

    // ── 觸發電平 SpinBox ──────────────────────────────────────────────────
    m_spinTrigLevel = new SmartStepSpinBox;
    m_spinTrigLevel->setObjectName("triggerLevel");
    m_spinTrigLevel->setDecimals(3);
    m_spinTrigLevel->setRange(-1000.00, 1000.00);
    if (QLineEdit* le = m_spinTrigLevel->findChild<QLineEdit*>())
        le->setAlignment(Qt::AlignCenter);

    m_spinLevelStep = new SmartStepSpinBox;
    m_spinLevelStep->setObjectName("triggerLevelStep");
    m_spinLevelStep->setDecimals(3);
    m_spinLevelStep->setRange(0.001, 1000.000);
    m_spinLevelStep->setValue(0.001);
    m_spinLevelStep->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spinTrigLevel->setStepOverride(m_spinLevelStep->value());
    connect(m_spinLevelStep, QOverload<double>::of(&SmartStepSpinBox::valueChanged),
            m_spinTrigLevel, &SmartStepSpinBox::setStepOverride);
    if (QLineEdit* le = m_spinLevelStep->findChild<QLineEdit*>())
        le->setAlignment(Qt::AlignCenter);

    // ── 半自動觸發 Step Scale ──────────────────────────────────────────────
    m_autoTrigScale = new SmartStepSpinBox;
    m_autoTrigScale->setObjectName("triggerScale");
    m_autoTrigScale->setDecimals(2);
    m_autoTrigScale->setRange(0.01, 1000.00);
    m_autoTrigScale->setValue(1.0);
    m_autoTrigScale->setButtonSymbols(QAbstractSpinBox::NoButtons);
    if (QLineEdit* le = m_autoTrigScale->findChild<QLineEdit*>())
        le->setAlignment(Qt::AlignCenter);

    // ── 半自動觸發 Target Level ────────────────────────────────────────────
    // objectName 保留原版 "triggerTaget"（拼寫與 DPO7000 相同，Controller 依此查找）
    m_autoTrigTarget = new SmartStepSpinBox;
    m_autoTrigTarget->setObjectName("triggerTaget");
    m_autoTrigTarget->setDecimals(2);
    m_autoTrigTarget->setRange(-1000.00, 1000.00);
    m_autoTrigTarget->setButtonSymbols(QAbstractSpinBox::NoButtons);
    if (QLineEdit* le = m_autoTrigTarget->findChild<QLineEdit*>())
        le->setAlignment(Qt::AlignCenter);

    // ── 狀態 Label ────────────────────────────────────────────────────────
    m_lblTrigStatus = new QLabel(tr("DISCONNECTED"));
    m_lblTrigStatus->setObjectName("triggerStatus");
    m_lblTrigStatus->setStyleSheet(
        "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
    m_lblTrigStatus->setAlignment(Qt::AlignCenter);
}

// ─────────────────────────────────────────────────────────────────────────────
//  populateSourceComboBox：根據 m_totalChannels 填充 Source 下拉選單
//  可多次呼叫（setChannelCount 會觸發）
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerWidget::populateSourceComboBox()
{
    if (!m_cmbTrigSource) return;

    m_cmbTrigSource->clear();

    // 填入 CH1 ~ CH<n>
    for (int ch = 1; ch <= m_totalChannels; ++ch)
        m_cmbTrigSource->addItem(QString("CH%1").arg(ch));

    // 置中對齊
    for (int i = 0; i < m_cmbTrigSource->count(); ++i)
        m_cmbTrigSource->setItemData(i, Qt::AlignCenter, Qt::TextAlignmentRole);

    // 預設選 CH1
    m_cmbTrigSource->setCurrentIndex(0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  setupLayout：與 DPO7000TriggerWidget 相同的佈局結構
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerWidget::setupLayout()
{
    auto* layTrigger = new QVBoxLayout(this);
    layTrigger->setSpacing(12);
    layTrigger->setContentsMargins(4, 18, 4, 8);

    // Type row
    auto* typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(tr("Type:")));
    typeRow->addWidget(m_cmbTrigType);
    layTrigger->addLayout(typeRow);

    // Source row
    auto* srcRow = new QHBoxLayout;
    srcRow->addWidget(new QLabel(tr("Source:")));
    srcRow->addWidget(m_cmbTrigSource);
    layTrigger->addLayout(srcRow);

    // Slope row
    auto* slopeRow = new QHBoxLayout;
    slopeRow->addWidget(new QLabel(tr("Slope:")));
    slopeRow->addWidget(m_btnTrigRising);
    slopeRow->addWidget(m_btnTrigFalling);
    slopeRow->addWidget(m_btnTrigBoth);
    layTrigger->addLayout(slopeRow);

    // Level row
    auto* levelRow = new QHBoxLayout;
    levelRow->addWidget(new QLabel(tr("Level:")));
    levelRow->addWidget(m_spinTrigLevel);
    levelRow->addWidget(m_btnTrigSet);
    layTrigger->addLayout(levelRow);

    auto* levelStepRow = new QHBoxLayout;
    levelStepRow->addWidget(new QLabel(tr("Step:")));
    levelStepRow->addWidget(m_spinLevelStep);
    layTrigger->addLayout(levelStepRow);

    // Mode row
    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(tr("Mode:")));
    modeRow->addWidget(m_btnTrigAuto);
    modeRow->addWidget(m_btnTrigNorm);
    modeRow->addWidget(m_btnTrigSingle);
    layTrigger->addLayout(modeRow);

    // Run/Stop row
    auto* runstopRow = new QHBoxLayout;
    runstopRow->addWidget(m_btnRunstop);
    layTrigger->addLayout(runstopRow);

    // Measurement row
    auto* measBox = new QGroupBox(tr("Measure"));
    auto* measLay = new QVBoxLayout(measBox);
    measLay->setSpacing(5);
    measLay->setContentsMargins(6, 16, 6, 6);
    measLay->addWidget(m_btnGetValue);

    auto addMeasureRow = [measLay](const QString& name, QLabel* value) {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        row->addWidget(new QLabel(name));
        row->addWidget(value);
        measLay->addLayout(row);
    };

    addMeasureRow(tr("Max:"), m_lblMeasMax);
    addMeasureRow(tr("Min:"), m_lblMeasMin);
    addMeasureRow(tr("RMS:"), m_lblMeasRms);
    addMeasureRow(tr("Mean:"), m_lblMeasMean);
    addMeasureRow(tr("Abs Peak:"), m_lblMeasAbsPeak);
    layTrigger->addWidget(measBox);

    // Semi-Auto trigger button（預設隱藏，與 DPO7000 一致保留但不顯示）
    // layTrigger->addWidget(m_btntrig_Steady);

    // Semi-Auto 參數區（預設隱藏）
    // auto* settingGroup = new QVBoxLayout;
    // settingGroup->setSpacing(6);
    // auto* autoRow = new QHBoxLayout;
    // autoRow->addWidget(new QLabel(tr("Step:")));
    // m_autoTrigScale->setFixedWidth(85);
    // autoRow->addWidget(m_autoTrigScale);
    // autoRow->addWidget(new QLabel(tr("Target:")));
    // m_autoTrigTarget->setFixedWidth(85);
    // autoRow->addWidget(m_autoTrigTarget);
    // settingGroup->addLayout(autoRow);
    // layTrigger->addLayout(settingGroup);

    // Status row
    auto* lampRow = new QHBoxLayout;
    lampRow->addWidget(m_lblTrigStatus);
    layTrigger->addLayout(lampRow);
}
