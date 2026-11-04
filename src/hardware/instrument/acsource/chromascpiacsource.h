#pragma once
#include "acsource.h"
#include "acsourcefamily.h"

// Commands documented by both the 6415/6420/6430 and 6560/6590 manuals.
class ChromaScpiACSource : public ACSource {
  public:
    QString model() const override { return m_model; }
    QString vendor() const override { return "Chroma"; }
    void setVoltage(double volts) override;
    bool readVoltageSetting(double &volts, QString &error) override;
    double measureVoltage() override;
    double measureCurrent() override;
    double realPower() override;
    double powerPfactor() override;
    double freQuency() override;
    void setPhaseOff(double) override;

  protected:
    ChromaScpiACSource(const QString &model, ACSourceFamily family, ICommunication *comm);
    bool usesScpiLineFraming() const override { return true; }
    void command(const QString &value);
    QString textReply(const QString &query);
    double scalar(const QString &query);
    void checkInstrumentError();
    static void requireRange(double value, double low, double high, const char *setting);

  private:
    const QString m_model;
};
