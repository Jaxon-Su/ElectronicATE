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

protected:
    // 共用的UI查找和連接方法
    virtual void connectSignals() = 0;
    virtual void updateTriggerStatus() = 0;

    Oscilloscope* m_instrument = nullptr;
};
