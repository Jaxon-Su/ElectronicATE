#include "msoseries456triggercontroller.h"
#include "oscilloscope.h"
#include "triggermodelcatalog.h"
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include "messageservice.h"
#include "smartstepspinbox.h"
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <cmath>

MSOSeries456TriggerController::MSOSeries456TriggerController(QWidget *triggerWidget, QObject *parent)
    : AbstractTriggerController(triggerWidget, parent)
{
    if (!triggerWidget)
        return;

    m_cmbTrigType = triggerWidget->findChild<QComboBox *>("triggerType");
    m_cmbTrigSource = triggerWidget->findChild<QComboBox *>("triggerSource");
    m_btnTrigRising = triggerWidget->findChild<QPushButton *>("triggerRising");
    m_btnTrigFalling = triggerWidget->findChild<QPushButton *>("triggerFalling");
    m_btnTrigBoth = triggerWidget->findChild<QPushButton *>("triggerBoth");
    m_btnTrigAuto = triggerWidget->findChild<QPushButton *>("triggerAuto");
    m_btnTrigNorm = triggerWidget->findChild<QPushButton *>("triggerNorm");
    m_btnTrigSingle = triggerWidget->findChild<QPushButton *>("triggerSingle");
    m_btnTrigSet = triggerWidget->findChild<QPushButton *>("triggerSet");
    m_btnStepFromScale = triggerWidget->findChild<QPushButton *>("triggerStepFromScale");
    m_btnRunstop = triggerWidget->findChild<QPushButton *>("runStop");
    m_btnGetValue = triggerWidget->findChild<QPushButton *>("getValue");
    m_spinTrigLevel = triggerWidget->findChild<SmartStepSpinBox *>("triggerLevel");
    m_spinLevelStep = triggerWidget->findChild<SmartStepSpinBox *>("triggerLevelStep");
    m_lblTrigStatus = triggerWidget->findChild<QLabel *>("triggerStatus");
    m_lblMeasMax = triggerWidget->findChild<QLabel *>("measureMax");
    m_lblMeasMin = triggerWidget->findChild<QLabel *>("measureMin");
    m_lblMeasRms = triggerWidget->findChild<QLabel *>("measureRms");
    m_lblMeasMean = triggerWidget->findChild<QLabel *>("measureMean");
    m_lblMeasAbsPeak = triggerWidget->findChild<QLabel *>("measureAbsPeak");

    m_control.setLeaseFactory([this] { return m_pollingLease ? m_pollingLease() : nullptr; });
    connect(&m_control, &ManualScopeControl::commandActivityChanged, this,
            [this]
            {
                lockTriggerControls(m_control.hasActiveCommand() || m_control.isSuspended());
                emit commandActivityChanged();
            });
    connect(&m_control, &ManualScopeControl::reconnectRequested, this,
            &ITriggerController::reconnectRequested);
    connect(&m_control, &ManualScopeControl::runningObserved, this,
            &MSOSeries456TriggerController::setRunningUI);
    connect(&m_control, &ManualScopeControl::connectionChanged, this,
            &MSOSeries456TriggerController::updateTriggerStatus);
    connect(&m_control, &ManualScopeControl::errorOccurred, this,
            [this](const QString &error)
            {
                if (m_lblTrigStatus)
                    m_lblTrigStatus->setText("ERROR");
                MessageService::instance().showWarning("Oscilloscope", error);
            });

    connectSignals();
}

MSOSeries456TriggerController::~MSOSeries456TriggerController() { cleanup(); }

void MSOSeries456TriggerController::cleanup()
{
    m_control.setSuspended(true);
    disconnect(&m_control, nullptr, this, nullptr);
    disconnect();
}

void MSOSeries456TriggerController::setInstrument(Oscilloscope *instrument)
{
    m_instrument = instrument && TriggerModelCatalog::family(instrument->model()) == TriggerModelCatalog::Family::Mso456
                       ? instrument : nullptr;
    m_control.bind(m_instrument);
}

void MSOSeries456TriggerController::connectSignals()
{
    if (m_btnTrigSingle)
        connect(m_btnTrigSingle, &QPushButton::clicked, this,
                &MSOSeries456TriggerController::onSingleTriggered);

    if (m_btnRunstop)
        connect(m_btnRunstop, &QPushButton::clicked, this,
                &MSOSeries456TriggerController::onRunStopTriggered);

    if (m_btnTrigAuto)
        connect(m_btnTrigAuto, &QPushButton::clicked, this, &MSOSeries456TriggerController::onAutoTriggered);

    if (m_btnTrigNorm)
        connect(m_btnTrigNorm, &QPushButton::clicked, this, &MSOSeries456TriggerController::onNormTriggered);

    if (m_btnTrigSet)
        connect(m_btnTrigSet, &QPushButton::clicked, this, &MSOSeries456TriggerController::onSetTriggered);

    if (m_btnStepFromScale)
        connect(m_btnStepFromScale, &QPushButton::clicked, this,
                &MSOSeries456TriggerController::onStepFromScaleTriggered);

    if (m_cmbTrigType)
        connect(m_cmbTrigType, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &MSOSeries456TriggerController::onTriggerTypeChanged);

    if (m_cmbTrigSource)
        connect(m_cmbTrigSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &MSOSeries456TriggerController::onTriggerSourceChanged);

    if (m_btnTrigRising)
        connect(m_btnTrigRising, &QPushButton::clicked, this,
                &MSOSeries456TriggerController::onSlopeRisingTriggered);

    if (m_btnTrigFalling)
        connect(m_btnTrigFalling, &QPushButton::clicked, this,
                &MSOSeries456TriggerController::onSlopeFallingTriggered);

    if (m_btnTrigBoth)
        connect(m_btnTrigBoth, &QPushButton::clicked, this,
                &MSOSeries456TriggerController::onSlopeBothTriggered);

    if (m_btnGetValue)
        connect(m_btnGetValue, &QPushButton::clicked, this,
                &MSOSeries456TriggerController::onGetValueTriggered);
}

void MSOSeries456TriggerController::onSingleTriggered() { m_control.single(); }

void MSOSeries456TriggerController::onRunStopTriggered() { m_control.toggleRun(); }

void MSOSeries456TriggerController::updateRunStopStatus() { m_control.poll(); }

void MSOSeries456TriggerController::setRunningUI(bool running)
{
    if (m_btnRunstop)
        m_btnRunstop->setText(running ? tr("Stop") : tr("Run"));

    if (m_lblTrigStatus)
    {
        if (running)
        {
            m_lblTrigStatus->setText("RUN");
            m_lblTrigStatus->setStyleSheet("QLabel { background: #7fff7f; color: #000000; "
                                           "border-radius: 7px; padding: 2px 10px; font-weight: bold; }");
        }
        else
        {
            m_lblTrigStatus->setText("STOP");
            m_lblTrigStatus->setStyleSheet("QLabel { background: #ff7f7f; color: #000000; "
                                           "border-radius: 7px; padding: 2px 10px; font-weight: bold; }");
        }
    }
}

void MSOSeries456TriggerController::onAutoTriggered() { m_control.automatic(); }

void MSOSeries456TriggerController::onNormTriggered() { m_control.normal(); }

void MSOSeries456TriggerController::onSetTriggered()
{
    if (m_spinTrigLevel)
        m_control.setLevel(m_spinTrigLevel->value());
}

void MSOSeries456TriggerController::onTriggerTypeChanged()
{
    if (m_cmbTrigType)
        m_control.setType(m_cmbTrigType->currentText());
}

void MSOSeries456TriggerController::onStepFromScaleTriggered()
{
    if (!m_spinLevelStep || !m_spinTrigLevel)
        return;
    const int channel = getSelectedChannel();
    const double maximumStep = m_spinLevelStep->maximum();
    m_control.readStep(channel, maximumStep,
                       [this, channel](const ManualScopeControl::Result &result)
                       {
                           if (channel != getSelectedChannel())
                               return;
                           QString fraction = QString::number(result.levelStep, 'f', 12).section('.', 1);
                           while (fraction.size() > 3 && fraction.endsWith('0'))
                               fraction.chop(1);
                           const int decimals = qMax(m_spinLevelStep->decimals(), int(fraction.size()));
                           m_spinLevelStep->setDecimals(decimals);
                           m_spinLevelStep->setMinimum(std::pow(10.0, -decimals));
                           m_spinTrigLevel->setDecimals(qMax(m_spinTrigLevel->decimals(), decimals));
                           m_spinLevelStep->setValue(result.levelStep);
                       });
}

void MSOSeries456TriggerController::onTriggerSourceChanged()
{
    if (m_cmbTrigSource)
        m_control.setSource(m_cmbTrigSource->currentText());
}

void MSOSeries456TriggerController::onSlopeRisingTriggered() { m_control.setSlope("RISING"); }

void MSOSeries456TriggerController::onSlopeFallingTriggered() { m_control.setSlope("FALLING"); }

void MSOSeries456TriggerController::onSlopeBothTriggered() { m_control.setSlope("BOTH"); }

void MSOSeries456TriggerController::onGetValueTriggered()
{
    const int channel = getSelectedChannel();
    m_control.measure(channel,
                      [this, channel](const ManualScopeControl::Result &result)
                      {
                          const auto &values = result.measurements;
                          const QList<QLabel *> labels{m_lblMeasMax, m_lblMeasMin, m_lblMeasRms,
                                                       m_lblMeasMean};
                          for (int i = 0; i < labels.size(); ++i)
                              if (labels[i])
                                  labels[i]->setText(formatMeasureValue(values[i]));
                          const bool maximum = std::abs(values[0]) >= std::abs(values[1]);
                          if (m_lblMeasAbsPeak)
                              m_lblMeasAbsPeak->setText(QString("%1 %2").arg(
                                  maximum ? "Max" : "Min", formatMeasureValue(values[maximum ? 0 : 1])));
                          if (m_lblTrigStatus)
                              m_lblTrigStatus->setText(QString("CH%1 MEASURED").arg(channel));
                      });
}

// 狀態更新

void MSOSeries456TriggerController::updateTriggerStatus()
{
    if (!m_lblTrigStatus)
        return;

    if (m_control.isConnected())
    {
        m_lblTrigStatus->setText("READY");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { background: #7fff7f; border-radius:7px; padding:2px 10px; }");
    }
    else
    {
        m_lblTrigStatus->setText("DISCONNECTED");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
    }
}

// 共用輔助

void MSOSeries456TriggerController::setSuspended(bool suspended) { m_control.setSuspended(suspended); }

void MSOSeries456TriggerController::lockTriggerControls(bool lock)
{
    for (QWidget *w : getTriggerControlWidgets())
        if (w)
            w->setEnabled(!lock);
}

QList<QWidget *> MSOSeries456TriggerController::getTriggerControlWidgets() const
{
    QList<QWidget *> widgets;
    if (m_cmbTrigType)
        widgets << m_cmbTrigType;
    if (m_cmbTrigSource)
        widgets << m_cmbTrigSource;
    if (m_btnTrigRising)
        widgets << m_btnTrigRising;
    if (m_btnTrigFalling)
        widgets << m_btnTrigFalling;
    if (m_btnTrigBoth)
        widgets << m_btnTrigBoth;
    if (m_btnTrigAuto)
        widgets << m_btnTrigAuto;
    if (m_btnTrigNorm)
        widgets << m_btnTrigNorm;
    if (m_btnTrigSingle)
        widgets << m_btnTrigSingle;
    if (m_btnTrigSet)
        widgets << m_btnTrigSet;
    if (m_btnStepFromScale)
        widgets << m_btnStepFromScale;
    if (m_btnRunstop)
        widgets << m_btnRunstop;
    if (m_btnGetValue)
        widgets << m_btnGetValue;
    if (m_spinTrigLevel)
        widgets << m_spinTrigLevel;
    if (m_spinLevelStep)
        widgets << m_spinLevelStep;
    return widgets;
}

QString MSOSeries456TriggerController::formatMeasureValue(double value) const
{
    if (!std::isfinite(value))
        return "N/A";

    return QString::number(value, 'g', 6);
}

QList<int> MSOSeries456TriggerController::getCheckedChannels() const
{
    QList<int> channels;
    if (!m_cmbTrigSource)
        return channels;

    auto *model = qobject_cast<QStandardItemModel *>(m_cmbTrigSource->model());
    if (!model)
        return channels;

    for (int i = 0; i < model->rowCount(); ++i)
    {
        QStandardItem *item = model->item(i);
        if (item && item->checkState() == Qt::Checked)
            channels.append(i + 1);
    }
    return channels;
}

QString MSOSeries456TriggerController::getCurrentTriggerSource() const
{
    return m_cmbTrigSource ? m_cmbTrigSource->currentText() : "CH1";
}
