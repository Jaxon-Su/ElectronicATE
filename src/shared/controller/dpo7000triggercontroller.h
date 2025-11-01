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

    void cleanup();

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

// ## 實際執行流程

//     ### **完整調用鏈**
// ```
//     1. Page1 配置改變
//    ↓
//     2. Page3ViewModel::onPage1ConfigChanged()
//    → createOscilloscopes()  // 創建示波器對象
//    ↓
//     3. Page3::onPage1ConfigChanged()
//    → setTriggerModel("DPO7000")
//    → createTriggerWidget()
//    ↓
//     4. TriggerWidgetFactory::createTriggerWidget()
//    → 創建 DPO7000TriggerWidget (UI)
//    → TriggerControllerFactory::createTriggerController()
//       → new DPO7000TriggerController()  // 具體類
//       → 返回 AbstractTriggerController* // 基類指標
//    ↓
//     5. Page3 發射信號
//    → emit triggerWidgetCreated("DPO7000", controller)
//    ↓
//     6. Page3ViewModel::onTriggerWidgetCreated()
//    → m_currentTriggerController = controller;  // 保存基類指標
// → connectTriggerController()
//       → controller->setInstrument(oscilloscope);  // 多態調用
