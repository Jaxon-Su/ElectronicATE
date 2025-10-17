#pragma once
#include "acsource.h"
#include "icommunication.h"

class DeltaA3000 : public ACSource
{
public:
    DeltaA3000(ICommunication* comm = nullptr);
    ~DeltaA3000() override;


    //Instrument Base--------------------------------
    QString model() const override;
    QString vendor() const override;

    //ACSource--------------------------------
    void setVoltage(double v) override;
    void setFrequency(double f) override;

    void setPhaseOn(double p) override;
    void setPhaseOff(double p) override;

    void setPowerOn() override;
    void setPowerOff() override;


    double measureVoltage() override;
    double measureCurrent() override;


    double realPower() override;;
    double reactivePower() override;;
    double apparentPower() override;;
    double powerPfactor() override;;


    double freQuency() override;;

private:
    QString m_lastError;

};
