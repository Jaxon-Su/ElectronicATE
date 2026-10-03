#pragma once
#include "dcsource.h"
#include "dcsourcespec/chroma62000spec.h"

// Supports the documented 62000H models, not other 62000 families.
// Communication is borrowed, as with the DC Load drivers.
class Chroma62000 : public DCSource {
public:
    explicit Chroma62000(const QString& subModel, ICommunication* comm = nullptr);
    QString model() const override { return m_spec.model; }
    QString vendor() const override { return "Chroma"; }
    const DCSourceSpec& spec() const { return m_spec; }
    void setVoltage(double voltage) override;
    // CV: current limit; CC: regulated current target. Not an OCP setting.
    void setCurrent(double current) override;
    void setPowerOn() override;
    void setPowerOff() override;
    double measureVoltage() override;
    double measureCurrent() override;
    double measurePower() override;
private:
    DCSourceSpec m_spec;
    void send(const QString& command);
    double measure(const QString& command);
    void validate(double value, double maximum, const QString& quantity);
    [[noreturn]] void fail(const QString& message);
};
