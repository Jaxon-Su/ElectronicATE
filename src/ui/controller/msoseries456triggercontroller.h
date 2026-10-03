#pragma once
#include "abstracttriggercontroller.h"
#include "manualscopecontrol.h"

class QComboBox;
class QPushButton;
class SmartStepSpinBox;
class QLabel;

// Widget adapter for MSO 4/5/6 manual controls. I/O runs under a borrowed scope lease.
class MSOSeries456TriggerController : public AbstractTriggerController
{
    Q_OBJECT
  public:
    explicit MSOSeries456TriggerController(QWidget *triggerWidget, QObject *parent = nullptr);
    ~MSOSeries456TriggerController();

    // AbstractTriggerController 介面
    void setInstrument(Oscilloscope *instrument) override;
    Oscilloscope *getInstrument() const override { return m_instrument; }
    QString getSupportedModel() const override { return "MSOSeries456"; }

    void cleanup();
    bool isOperationActive() const override { return m_control.isBusy(); }
    bool hasActiveCommand() const override { return m_control.hasActiveCommand(); }
    void setSuspended(bool suspended) override;

    // 供外部查詢目前 UI 狀態
    QList<int> getCheckedChannels() const;
    QString getCurrentTriggerSource() const;

    // 直接讀 Source ComboBox，回傳 1-based 通道號（不查詢儀器）
    int getSelectedChannel() const override
    {
        const QString src = getCurrentTriggerSource(); // e.g. "CH2"
        if (src.startsWith("CH", Qt::CaseInsensitive))
        {
            bool ok;
            const int ch = src.mid(2).toInt(&ok);
            if (ok && ch >= 1)
                return ch;
        }
        return 1; // fallback
    }

  private slots:
    void onSingleTriggered();
    void onRunStopTriggered();
    void onAutoTriggered();
    void onNormTriggered();
    void onSetTriggered();
    void onStepFromScaleTriggered();
    void onTriggerTypeChanged();
    void onTriggerSourceChanged();
    void onSlopeRisingTriggered();
    void onSlopeFallingTriggered();
    void onSlopeBothTriggered();
    void onGetValueTriggered();
    void updateRunStopStatus();

  protected:
    void connectSignals() override;
    void updateTriggerStatus() override;

  private:
    Oscilloscope *m_instrument = nullptr;
    ManualScopeControl m_control;
    QComboBox *m_cmbTrigType = nullptr;
    QComboBox *m_cmbTrigSource = nullptr;
    QPushButton *m_btnTrigRising = nullptr;
    QPushButton *m_btnTrigFalling = nullptr;
    QPushButton *m_btnTrigBoth = nullptr;
    QPushButton *m_btnTrigAuto = nullptr;
    QPushButton *m_btnTrigNorm = nullptr;
    QPushButton *m_btnTrigSingle = nullptr;
    QPushButton *m_btnTrigSet = nullptr;
    QPushButton *m_btnStepFromScale = nullptr;
    QPushButton *m_btnRunstop = nullptr;
    QPushButton *m_btnGetValue = nullptr;
    SmartStepSpinBox *m_spinTrigLevel = nullptr;
    SmartStepSpinBox *m_spinLevelStep = nullptr;
    QLabel *m_lblTrigStatus = nullptr;
    QLabel *m_lblMeasMax = nullptr;
    QLabel *m_lblMeasMin = nullptr;
    QLabel *m_lblMeasRms = nullptr;
    QLabel *m_lblMeasMean = nullptr;
    QLabel *m_lblMeasAbsPeak = nullptr;

    // 共用輔助
    void setRunningUI(bool running);
    void lockTriggerControls(bool lock);
    QList<QWidget *> getTriggerControlWidgets() const;
    QString formatMeasureValue(double value) const;
};
