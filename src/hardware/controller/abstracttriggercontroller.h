#pragma once
#include <QObject>
#include "oscilloscope.h"

class AbstractTriggerController : public QObject
{
    Q_OBJECT
public:
    explicit AbstractTriggerController(QWidget* triggerWidget, QObject* parent = nullptr)
        : QObject(parent) {}
    virtual ~AbstractTriggerController() = default;

    // 純虛擬方法 - 所有子類必須實作
    virtual void setInstrument(Oscilloscope* instrument) = 0;
    virtual Oscilloscope* getInstrument() const = 0;
    virtual QString getSupportedModel() const = 0;

    // 回傳 trigger widget 目前 Source ComboBox 所選的通道號（1-based）。
    // 直接讀 UI，不查詢儀器，確保與使用者選擇一致。
    // 子類如有 Source ComboBox 應 override；預設回傳 0（未知）。
    virtual int getSelectedChannel() const { return 0; }

signals:
    // 偵測到通訊中斷時發出，由 Page3ViewModel 重建示波器連線
    void reconnectRequested();

protected:
    // 共用的UI查找和連接方法
    virtual void connectSignals() = 0;
    virtual void updateTriggerStatus() = 0;

    Oscilloscope* m_instrument = nullptr;

    // 重連節流計數：每 RECONNECT_INTERVAL 次 timer tick 發一次 reconnectRequested
    // timer 週期 500ms，設 10 → 約 5 秒重試一次
    static constexpr int kReconnectInterval = 10;
    int m_reconnectCounter = 0;
};
