// ACSource.h
#pragma once
#include "../InstrumentWithCommBase.h"

class ACSource : public InstrumentWithCommBase
{
public:
    explicit ACSource(ICommunication* comm = nullptr)
        : InstrumentWithCommBase(comm) {}

    virtual ~ACSource() = default;

    // AC Source專屬介面
    virtual void setVoltage(double v) = 0;
    virtual void setFrequency(double f) = 0;
    virtual void setPhaseOn(double p) = 0;
    virtual void setPhaseOff(double p) = 0;

    virtual void setPowerOn() = 0;
    virtual void setPowerOff() = 0;

    virtual double measureVoltage() = 0;
    virtual double measureCurrent() = 0;

    virtual double realPower() = 0;
    virtual double reactivePower() = 0;
    virtual double apparentPower() = 0;
    virtual double powerPfactor() = 0;


    virtual double freQuency() = 0;
};


