#include "dpo7000triggerwidget.h"
#include "DPO7000TriggerController.h"
#include "smartstepspinbox.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QFont>

DPO7000TriggerWidget::DPO7000TriggerWidget(QWidget* parent)
    : QGroupBox(tr("Trigger"), parent)
{
    setupUI();
    m_controller = new DPO7000TriggerController(this, this);
}

DPO7000TriggerWidget::~DPO7000TriggerWidget()
{
    cleanup();
}

void DPO7000TriggerWidget::cleanup()
{
    // QPointer 會自動檢測物件是否已刪除
    if (m_controller) {
        // 斷開所有信號連接
        m_controller->disconnect();

        // 如果控制器沒有父物件，則手動刪除
        if (!m_controller->parent()) {
            delete m_controller;
        }
        m_controller = nullptr;
    }
}

void DPO7000TriggerWidget::setupUI()
{
    setFont(QFont(font().family(), 9, QFont::Bold));
    setFixedWidth(280);

    createControls();
    setupLayout();
}

void DPO7000TriggerWidget::createControls()
{
    // Trigger Type
    m_cmbTrigType = new QComboBox;
    m_cmbTrigType->setObjectName("triggerType");
    m_cmbTrigType->addItems({tr("Edge")});
    for (int i = 0; i < m_cmbTrigType->count(); ++i)
        m_cmbTrigType->setItemData(i, Qt::AlignCenter, Qt::TextAlignmentRole);

    // Trigger Source
    m_cmbTrigSource = new QComboBox;
    m_cmbTrigSource->setObjectName("triggerSource");
    m_cmbTrigSource->addItems({tr("CH1"), tr("CH2"), tr("CH3"), tr("CH4")});
    for (int i = 0; i < m_cmbTrigSource->count(); ++i)
        m_cmbTrigSource->setItemData(i, Qt::AlignCenter, Qt::TextAlignmentRole);

    // Slope buttons
    m_btnTrigRising = new QPushButton(tr("↑"));
    m_btnTrigRising->setObjectName("triggerRising");

    m_btnTrigFalling = new QPushButton(tr("↓"));
    m_btnTrigFalling->setObjectName("triggerFalling");

    m_btnTrigBoth = new QPushButton(tr("↑↓"));
    m_btnTrigBoth->setObjectName("triggerBoth");

    // Mode buttons
    m_btnTrigAuto = new QPushButton(tr("Auto"));
    m_btnTrigAuto->setObjectName("triggerAuto");

    m_btnTrigNorm = new QPushButton(tr("Norm"));
    m_btnTrigNorm->setObjectName("triggerNorm");

    m_btnTrigSingle = new QPushButton(tr("Single"));
    m_btnTrigSingle->setObjectName("triggerSingle");

    // Control buttons
    m_btnTrigSet = new QPushButton(tr("Set"));
    m_btnTrigSet->setObjectName("triggerSet");

    m_btnRunstop = new QPushButton(tr("Run/Stop"));
    m_btnRunstop->setObjectName("runStop");

    // Semi-Auto trigger button
    m_btntrig_Steady = new QPushButton(tr("Semi-Auto Trigger OFF"));
    m_btntrig_Steady->setObjectName("btntrig_Steady");
    m_btntrig_Steady->setCheckable(true);

    // Trigger level
    m_spinTrigLevel = new SmartStepSpinBox;
    m_spinTrigLevel->setObjectName("triggerLevel");
    m_spinTrigLevel->setDecimals(2);
    m_spinTrigLevel->setRange(-1000.00, 1000.00);
    if (QLineEdit *le = m_spinTrigLevel->findChild<QLineEdit*>())
        le->setAlignment(Qt::AlignCenter);

    // Auto Trigger Scale
    m_autoTrigScale = new SmartStepSpinBox;
    m_autoTrigScale->setObjectName("triggerScale");
    m_autoTrigScale->setDecimals(2);
    m_autoTrigScale->setRange(0.01, 1000.00);
    m_autoTrigScale->setValue(1.0);
    m_autoTrigScale->setButtonSymbols(QAbstractSpinBox::NoButtons);
    if (QLineEdit *le = m_autoTrigScale->findChild<QLineEdit*>())
        le->setAlignment(Qt::AlignCenter);

    // Auto Trigger Target
    m_autoTrigTarget = new SmartStepSpinBox;
    m_autoTrigTarget->setObjectName("triggerTaget");
    m_autoTrigTarget->setDecimals(2);
    m_autoTrigTarget->setRange(-1000.00, 1000.00);
    m_autoTrigTarget->setButtonSymbols(QAbstractSpinBox::NoButtons);
    if (QLineEdit *le = m_autoTrigTarget->findChild<QLineEdit*>())
        le->setAlignment(Qt::AlignCenter);

    // Status label
    m_lblTrigStatus = new QLabel(tr("DISCONNECTED"));
    m_lblTrigStatus->setObjectName("triggerStatus");
    m_lblTrigStatus->setStyleSheet(
        "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
    m_lblTrigStatus->setAlignment(Qt::AlignCenter);
}

void DPO7000TriggerWidget::setupLayout()
{
    auto *layTrigger = new QVBoxLayout(this);
    layTrigger->setSpacing(12);
    layTrigger->setContentsMargins(4, 18, 4, 8);

    // Type row
    auto typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(tr("Type:")));
    typeRow->addWidget(m_cmbTrigType);
    layTrigger->addLayout(typeRow);

    // Source row
    auto srcRow = new QHBoxLayout;
    srcRow->addWidget(new QLabel(tr("Source:")));
    srcRow->addWidget(m_cmbTrigSource);
    layTrigger->addLayout(srcRow);

    // Slope row
    auto slopeRow = new QHBoxLayout;
    slopeRow->addWidget(new QLabel(tr("Slope:")));
    slopeRow->addWidget(m_btnTrigRising);
    slopeRow->addWidget(m_btnTrigFalling);
    slopeRow->addWidget(m_btnTrigBoth);
    layTrigger->addLayout(slopeRow);

    // Level row
    auto levelRow = new QHBoxLayout;
    levelRow->addWidget(new QLabel(tr("Level:")));
    levelRow->addWidget(m_spinTrigLevel);
    levelRow->addWidget(m_btnTrigSet);
    layTrigger->addLayout(levelRow);

    // Mode row
    auto modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(tr("Mode:")));
    modeRow->addWidget(m_btnTrigAuto);
    modeRow->addWidget(m_btnTrigNorm);
    modeRow->addWidget(m_btnTrigSingle);
    layTrigger->addLayout(modeRow);

    // Run/Stop row
    auto runstopRow = new QHBoxLayout;
    runstopRow->addWidget(m_btnRunstop);
    layTrigger->addLayout(runstopRow);

    // Semi-Auto trigger button
    layTrigger->addWidget(m_btntrig_Steady);

    // Setting area
    auto settingGroup = new QVBoxLayout;
    settingGroup->setSpacing(6);

    // Scale row
    auto autoRow = new QHBoxLayout;
    autoRow->addWidget(new QLabel(tr("Step:")));
    m_autoTrigScale->setFixedWidth(85);
    autoRow->addWidget(m_autoTrigScale);
    autoRow->addWidget(new QLabel(tr("Target:")));
    m_autoTrigTarget->setFixedWidth(85);
    autoRow->addWidget(m_autoTrigTarget);
    settingGroup->addLayout(autoRow);

    layTrigger->addLayout(settingGroup);

    // Status row
    auto lampRow = new QHBoxLayout;
    lampRow->addWidget(m_lblTrigStatus);
    layTrigger->addLayout(lampRow);
}
