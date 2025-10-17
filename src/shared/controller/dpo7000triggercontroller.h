#pragma once
#include "AbstractTriggerController.h"
#include "dpo7000.h"
#include "autotriggerworker.h"
#include <QThread>
#include <QTimer>

class QComboBox;
class QPushButton;
class SmartStepSpinBox;
class QLabel;

class DPO7000TriggerController : public AbstractTriggerController
{
    Q_OBJECT
public:
    explicit DPO7000TriggerController(QWidget* triggerWidget, QObject* parent = nullptr);
    ~DPO7000TriggerController();

    void setInstrument(Oscilloscope* instrument) override;
    Oscilloscope* getInstrument() const override { return m_instrument; }
    QString getSupportedModel() const override { return "DPO7000"; }

    void setInstrument(DPO7000* instrument);
    DPO7000* getDPO7000Instrument() const;

private slots:
    void onSingleTriggered();
    void onRunStopTriggered();
    void onAutoTriggered();
    void onNormTriggered();
    void onSetTriggered();
    void onTriggerTypeChanged();
    void onTriggerSourceChanged();
    void onSlopeRisingTriggered();
    void onSlopeFallingTriggered();
    void onSlopeBothTriggered();
    void onTriggerSteadyToggled(bool on);
    void onTargetLevelChanged();
    void onStepScaleChanged();
    void updateRunStopStatus();

protected:
    void connectSignals() override;
    void updateTriggerStatus() override;

private:
    QComboBox* m_cmbTrigType = nullptr;
    QComboBox* m_cmbTrigSource = nullptr;
    QPushButton* m_btnTrigRising = nullptr;
    QPushButton* m_btnTrigFalling = nullptr;
    QPushButton* m_btnTrigBoth = nullptr;
    QPushButton* m_btnTrigAuto = nullptr;
    QPushButton* m_btnTrigNorm = nullptr;
    QPushButton* m_btnTrigSingle = nullptr;
    QPushButton* m_btnTrigSet = nullptr;
    QPushButton* m_btnRunstop = nullptr;
    SmartStepSpinBox* m_spinTrigLevel = nullptr;
    SmartStepSpinBox* m_autoTrigScale = nullptr;
    SmartStepSpinBox* m_autoTrigTarget = nullptr;
    QLabel* m_lblTrigStatus = nullptr;
    QPushButton* m_btnTrigSteady = nullptr;
    QTimer* m_statusTimer = nullptr;

    bool checkInstrumentConnection() const;
    void showConnectionError() const;
    void setRunningUI(bool running);

    QThread* m_workerThread = nullptr;
    AutoTriggerWorker* m_worker = nullptr;

    void setupWorkerThread();
    void cleanupWorkerThread();
    void lockTriggerControls(bool lock);
    QList<QWidget*> getTriggerControlWidgets() const;
};
