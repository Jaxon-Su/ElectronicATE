#pragma once
#include "chromascpiacsource.h"

class Chroma6415_6420_6430 final : public ChromaScpiACSource {
  public:
    explicit Chroma6415_6420_6430(const QString &model, ICommunication *comm = nullptr);
    bool prepareOutput(bool threePhase, double phaseVoltage) override;
    void setFrequency(double hz) override;
    void setPhaseOn(double degrees) override;
    void setPowerOn() override;
    void setPowerOff() override;
    double apparentPower() override;
    double reactivePower() override;
};
