#pragma once
#include "instrumentwithcommbase.h"

class DCSource : public InstrumentWithCommBase {
  public:
    explicit DCSource(ICommunication* comm = nullptr) : InstrumentWithCommBase(comm) {}

    virtual ~DCSource() = default;

    virtual void setVoltage(double v) = 0;
    virtual void setCurrent(double f) = 0;

    virtual void setPowerOn() = 0;
    virtual void setPowerOff() = 0;

    virtual double measureVoltage() = 0;
    virtual double measureCurrent() = 0;
    virtual double measurePower() = 0;
};
