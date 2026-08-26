#pragma once
#include "abstracttriggercontroller.h"
#include "msoseries456.h"
#include "autotriggerworker.h"
#include <QThread>
#include <QTimer>

class QComboBox;
class QPushButton;
class SmartStepSpinBox;
class QLabel;

// ─────────────────────────────────────────────────────────────────────────────
//  MSOSeries456TriggerController
//
//  支援機型: MSO44(B) / MSO46(B) / MSO54(B) / MSO56(B) / MSO58(B) /
//            MSO58LP / MSO64(B) / MSO66B / MSO68B / LPD64
//
//  與 DPO7000TriggerController 的差異：
//    1. setInstrument() 的 dynamic_cast 目標為 MSOSeries456
//    2. getSupportedModel() 回傳 "MSOSeries456"
//    3. setupWorkerThread() 使用 MSOSeries456*（AutoTriggerWorker 介面相同）
//
//  注意：若 AutoTriggerWorker 目前僅接受 DPO7000*，
//        需將其建構子改為 Oscilloscope* 以同時支援兩種機型。
//        所有半自動觸發必需的方法（getTriggerLevel / setTriggerLevel /
//        measureSignalPeak）在 MSOSeries456 中均已實作。
// ─────────────────────────────────────────────────────────────────────────────
class MSOSeries456TriggerController : public AbstractTriggerController
{
    Q_OBJECT
public:
    explicit MSOSeries456TriggerController(QWidget* triggerWidget,
                                           QObject* parent = nullptr);
    ~MSOSeries456TriggerController();

    // AbstractTriggerController 介面
    void          setInstrument(Oscilloscope* instrument) override;
    Oscilloscope* getInstrument() const override { return m_instrument; }
    QString       getSupportedModel() const override { return "MSOSeries456"; }

    // 具型別版本（供 ViewModel 直接呼叫，避免 dynamic_cast）
    void           setInstrument(MSOSeries456* instrument);
    MSOSeries456*  getMSOInstrument() const;

    void cleanup();

    // 供外部查詢目前 UI 狀態
    QList<int> getCheckedChannels() const;
    QString    getCurrentTriggerSource() const;

    // 直接讀 Source ComboBox，回傳 1-based 通道號（不查詢儀器）
    int getSelectedChannel() const override {
        const QString src = getCurrentTriggerSource(); // e.g. "CH2"
        if (src.startsWith("CH", Qt::CaseInsensitive)) {
            bool ok;
            const int ch = src.mid(2).toInt(&ok);
            if (ok && ch >= 1) return ch;
        }
        return 1; // fallback
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
    void onGetValueTriggered();
    void onTriggerSteadyToggled(bool on);
    void onTargetLevelChanged();
    void onStepScaleChanged();
    void updateRunStopStatus();

protected:
    void connectSignals()      override;
    void updateTriggerStatus() override;

private:
    // ── UI 元件指標（與 DPO7000TriggerController 相同的 objectName 集合）────
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
    QPushButton*      m_btnGetValue    = nullptr;
    SmartStepSpinBox* m_spinTrigLevel  = nullptr;
    SmartStepSpinBox* m_autoTrigScale  = nullptr;
    SmartStepSpinBox* m_autoTrigTarget = nullptr;
    QLabel*           m_lblTrigStatus  = nullptr;
    QLabel*           m_lblMeasMax     = nullptr;
    QLabel*           m_lblMeasMin     = nullptr;
    QLabel*           m_lblMeasRms     = nullptr;
    QLabel*           m_lblMeasMean    = nullptr;
    QLabel*           m_lblMeasAbsPeak = nullptr;
    QPushButton*      m_btnTrigSteady  = nullptr;
    QTimer*           m_statusTimer    = nullptr;

    // ── 半自動觸發執行緒 ─────────────────────────────────────────────────────
    QThread*           m_workerThread = nullptr;
    AutoTriggerWorker* m_worker       = nullptr;

    void setupWorkerThread();
    void cleanupWorkerThread();

    // ── 共用輔助 ─────────────────────────────────────────────────────────────
    bool checkInstrumentConnection() const;
    void showConnectionError()       const;
    void setRunningUI(bool running);
    void lockTriggerControls(bool lock);
    QList<QWidget*> getTriggerControlWidgets() const;
    QString formatMeasureValue(double value) const;
};
