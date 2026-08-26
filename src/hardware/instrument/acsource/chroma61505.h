#pragma once
#include "acsource.h"
#include "icommunication.h"

// ─────────────────────────────────────────────
//  Chroma 61505 Programmable AC Source Driver
//  Protocol : GPIB / RS-232C (SCPI)
//  Manual   : 61505 User's Manual v1.5 (2008)
// ─────────────────────────────────────────────
class Chroma61505 : public ACSource
{
public:
    explicit Chroma61505(ICommunication* comm = nullptr);
    ~Chroma61505() override;

    // ── InstrumentBase ─────────────────────────
    QString model()  const override;
    QString vendor() const override;

    // ── ACSource (必要介面) ────────────────────
    void   setVoltage(double v)   override;  // SOURce:VOLTage:AC
    void   setFrequency(double f) override;  // SOURce:FREQuency  (15~1000 Hz)
    void   setPhaseOn(double p)   override;  // SOURce:PHASe:ON
    void   setPhaseOff(double p)  override;  // SOURce:PHASe:OFF

    void   setPowerOn()  override;           // OUTPut ON
    void   setPowerOff() override;           // OUTPut OFF

    double measureVoltage()  override;       // MEASure:VOLTage:ACDC?
    double measureCurrent()  override;       // MEASure:CURRent:AC?

    double realPower()      override;        // MEASure:POWer:AC?
    double reactivePower()  override;        // MEASure:POWer:AC:REACtive?
    double apparentPower()  override;        // MEASure:POWer:AC:APParent?
    double powerPfactor()   override;        // MEASure:POWer:AC:PFACtor?

    double freQuency()      override;        // MEASure:FREQuency?

    // ── Chroma 61505 專屬功能 ──────────────────

    // 輸出電壓範圍選擇 (LOW=150V / HIGH=300V / AUTO)
    enum class VoltageRange { Low, High, Auto };
    void setVoltageRange(VoltageRange range);    // SOURce:VOLTage:RANGe

    // DC 電壓輸出 (ACDC 耦合模式)
    void setVoltageDC(double v);                 // SOURce:VOLTage:DC  (-424.2~424.2 V)

    // 電流限制
    void setCurrentLimit(double a);              // SOURce:CURRent:LIMit

    // 輸出 Relay 控制（硬體繼電器）
    void setOutputRelay(bool on);               // OUTPut:RELay ON|OFF

    // 量測：DC 電壓
    double measureVoltageDC();                  // MEASure:VOLTage:DC?

    // 量測：電流峰值 / Crest Factor / Inrush
    double measureCurrentPeak();                // MEASure:CURRent:AMPLitude:MAXimum?
    double measureCrestFactor();                // MEASure:CURRent:CREStfactor?
    double measureInrushCurrent();              // MEASure:CURRent:INRush?

private:
    static constexpr double kFreqMin =   15.0;
    static constexpr double kFreqMax = 1000.0;
};
