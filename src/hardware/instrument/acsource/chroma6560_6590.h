#pragma once
#include "chromascpiacsource.h"

class Chroma6560_6590 final : public ChromaScpiACSource {
  public:
    explicit Chroma6560_6590(const QString &model, ICommunication *comm = nullptr);
    bool prepareOutput(bool threePhase, double phaseVoltage) override;
    void setVoltageWhileOn(double volts) override;
    void setFrequency(double hz) override;
    void setPhaseOn(double degrees) override;
    void setPowerOn() override;
    void setPowerOff() override;
    double apparentPower() override;
    double reactivePower() override;

  private:
    bool m_prepared = false;
    bool m_running = false;
};
