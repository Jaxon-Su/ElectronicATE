
#pragma once
#include "instrumentwithcommbase.h"

class ACSource : public InstrumentWithCommBase {
  protected:
    bool throwOnWriteFailure() const override { return true; }

  public:
    explicit ACSource(ICommunication* comm = nullptr) : InstrumentWithCommBase(comm) {}

    virtual ~ACSource() = default;

    // AC Source專屬介面
    virtual void setVoltage(double v) = 0;
    // Update an already enabled output without toggling its relay or restarting the test.
    virtual void setVoltageWhileOn(double v) { setVoltage(v); }
    virtual bool readVoltageSetting(double &, QString &error)
    { error = "Programmed voltage readback is unsupported by this AC source"; return false; }
    virtual void setFrequency(double f) = 0;
    virtual void setPhaseOn(double p) = 0;
    virtual void setPhaseOff(double p) = 0;

    virtual void setPowerOn() = 0;
    virtual void setPowerOff() = 0;
    // Same readback used by the supported drivers after OFF; no output write.
    virtual bool outputEnabled() { return queryOutputEnabled("OUTPut?"); }

    virtual double measureVoltage() = 0;
    virtual double measureCurrent() = 0;

    virtual double realPower() = 0;
    virtual double reactivePower() = 0;
    virtual double apparentPower() = 0;
    virtual double powerPfactor() = 0;

    virtual double freQuency() = 0;
};
