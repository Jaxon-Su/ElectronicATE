#pragma once
#include "abstracttriggercontroller.h"
#include "dpo4000.h"
#include "autotriggerworker.h"
#include <QThread>
#include <QTimer>

class QComboBox;
class QPushButton;
class SmartStepSpinBox;
class QLabel;

class DPO4000TriggerController : public AbstractTriggerController
{
    Q_OBJECT
public:
    explicit DPO4000TriggerController(QWidget* triggerWidget, QObject* parent = nullptr);
    ~DPO4000TriggerController();

    // AbstractTriggerController 介面實作
    void setInstrument(Oscilloscope* instrument) override;
    Oscilloscope* getInstrument() const override { return m_instrument; }
    QString getSupportedModel() const override { return "DPO4000"; }

    // DPO4000 具型別重載版本（供直接呼叫端使用）
    void setInstrument(DPO4000* instrument);
    DPO4000* getDPO4000Instrument() const;

    void cleanup();

    // 查詢輔助
    QList<int> getCheckedChannels() const;
    QString getCurrentTriggerSource() const;

    int getSelectedChannel() const override {
        const QString src = getCurrentTriggerSource();
        if (src.startsWith("CH", Qt::CaseInsensitive)) {
            bool ok;
            const int ch = src.mid(2).toInt(&ok);
            if (ok && ch >= 1) return ch;
        }
        return 1;
    }

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
    void connectSignals()      override;
    void updateTriggerStatus() override;

private:
    // UI 指標（透過 findChild 從 Widget 取得）
    QComboBox*        m_cmbTrigType    = nullptr;
    QComboBox*        m_cmbTrigSource  = nullptr;
    QPushButton*      m_btnTrigRising  = nullptr;
    QPushButton*      m_btnTrigFalling = nullptr;
    QPushButton*      m_btnTrigBoth    = nullptr;
    QPushButton*      m_btnTrigAuto    = nullptr;
    QPushButton*      m_btnTrigNorm    = nullptr;
    QPushButton*      m_btnTrigSingle  = nullptr;
    QPushButton*      m_btnTrigSet     = nullptr;
    QPushButton*      m_btnRunstop     = nullptr;
    SmartStepSpinBox* m_spinTrigLevel  = nullptr;
    SmartStepSpinBox* m_autoTrigScale  = nullptr;
    SmartStepSpinBox* m_autoTrigTarget = nullptr;
    QLabel*           m_lblTrigStatus  = nullptr;
    QPushButton*      m_btnTrigSteady  = nullptr;
    QTimer*           m_statusTimer    = nullptr;

    // 連線與 UI 輔助
    bool checkInstrumentConnection() const;
    void showConnectionError() const;
    void setRunningUI(bool running);

    // 半自動觸發 Worker Thread
    QThread*           m_workerThread = nullptr;
    AutoTriggerWorker* m_worker       = nullptr;

    void setupWorkerThread();
    void cleanupWorkerThread();
    void lockTriggerControls(bool lock);
    QList<QWidget*> getTriggerControlWidgets() const;
};
