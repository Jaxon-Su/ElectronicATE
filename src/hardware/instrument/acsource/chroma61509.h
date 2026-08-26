#pragma once
#include "acsource.h"
#include "icommunication.h"

// ─────────────────────────────────────────────
//  Chroma 61509 Programmable AC Source Driver
//  Protocol : GPIB / RS-232C / USB / Ethernet (SCPI)
//  Manual   : 61507/61508/61509 User's Manual v1.3 (2025-04)
//
//  Specs (61509):
//    Total Power     : 6 kVA
//    Per Phase Power : 2 kVA
//    AC Output (1-Phase) Current RMS : 60 A (LOW) / 30 A (HIGH)
//    AC Output (3-Phase/phase) Current RMS : 20 A (LOW) / 10 A (HIGH)
//    Voltage Range   : 0~175 V (LOW) / 0~350 V (HIGH)
//    Frequency Range : 15~2000 Hz (Standard)
//    DC Voltage      : ±247.5 V (LOW) / ±494.9 V (HIGH)
// ─────────────────────────────────────────────
class Chroma61509 : public ACSource
{
public:
    explicit Chroma61509(ICommunication* comm = nullptr);
    ~Chroma61509() override;

    // ── InstrumentBase ─────────────────────────
    QString model()  const override;
    QString vendor() const override;

    // ── ACSource (必要介面) ────────────────────
    void   setVoltage(double v)   override;   // SOURce:VOLTage:AC
    void   setFrequency(double f) override;   // SOURce:FREQuency  (15~2000 Hz)
    void   setPhaseOn(double p)   override;   // SOURce:PHASe:ON
    void   setPhaseOff(double p)  override;   // SOURce:PHASe:OFF

    void   setPowerOn()  override;            // OUTPut ON
    void   setPowerOff() override;            // OUTPut OFF

    double measureVoltage()  override;        // MEASure:VOLTage:AC?
    double measureCurrent()  override;        // MEASure:CURRent:AC?

    double realPower()      override;         // MEASure:POWer:AC?
    double reactivePower()  override;         // MEASure:POWer:AC:REACtive?
    double apparentPower()  override;         // MEASure:POWer:AC:APParent?
    double powerPfactor()   override;         // MEASure:POWer:AC:PFACtor?

    double freQuency()      override;         // MEASure:FREQuency?

    // ── Chroma 61509 專屬功能 ──────────────────

    // 輸出電壓範圍 (LOW=175V / HIGH=350V / AUTO)
    enum class VoltageRange { Low, High, Auto };
    void setVoltageRange(VoltageRange range);    // SOURce:VOLTage:RANGe

    // DC 電壓輸出
    void setVoltageDC(double v);                 // SOURce:VOLTage:DC  (±247.5/±494.9 V)

    // 電流限制
    void setCurrentLimit(double a);              // SOURce:CURRent:LIMit

    // OCP 延遲時間 (0~5.0 s, step 0.1 s)
    void setCurrentDelay(double s);              // SOURce:CURRent:DELay

    // 電源保護 (OPP)
    void setPowerProtection(double w);           // SOURce:POWer:PROTection

    // 輸出 Relay
    void setOutputRelay(bool on);                // OUTPut:RELay ON|OFF

    // 輸出耦合模式
    enum class OutputCoupling { AC, DC, ACDC };
    void setOutputCoupling(OutputCoupling mode); // OUTPut:COUPling AC|DC|ACDC

    // 輸出操作模式
    void setOutputMode(const QString& mode);     // OUTPut:MODE FIXED|LIST|PULSE|STEP|SYNTH|INTERHAR

    // 保護清除
    void clearProtection();                      // OUTPut:PROTection:CLEar

    // 可程式輸出阻抗
    void setImpedanceState(bool on);             // OUTPut:IMPedance:STATe
    void setImpedanceResistor(double ohm);       // OUTPut:IMPedance:RESistor (0~1.00 Ω)
    void setImpedanceInductor(double mH);        // OUTPut:IMPedance:INDuction (0~2.00 mH)

    // 輸出 Slew Rate
    void setSlewVoltageAC(double vPerMs);        // OUTPut:SLEW:VOLTage:AC   (0~1200 V/ms)
    void setSlewVoltageDC(double vPerMs);        // OUTPut:SLEW:VOLTage:DC   (0~1200 V/ms)
    void setSlewFrequency(double hzPerMs);       // OUTPut:SLEW:FREQuency    (0~1600 Hz/ms)

    // 三相相位差設定
    void setPhaseP12(double deg);               // SOURce:PHASe:P12 (0~359.9)
    void setPhaseP13(double deg);               // SOURce:PHASe:P13 (0~359.9)

    // 多相位選擇 (1|2|3)
    void setPhaseSelect(int phase);             // INSTrument:NSELect

    // 相位模式 (THREE | SINGLE)
    void setInstrumentPhase(const QString& mode); // INSTrument:PHASe

    // 電壓限制
    void setVoltageLimitAC(double v);            // SOURce:VOLTage:LIMit:AC
    void setVoltageLimitDCPlus(double v);        // SOURce:VOLTage:LIMit:DC:PLUS
    void setVoltageLimitDCMinus(double v);       // SOURce:VOLTage:LIMit:DC:MINus

    // 量測：AC/DC/ACDC 電壓
    double measureVoltageAC();                   // MEASure:VOLTage:AC?
    double measureVoltageDC();                   // MEASure:VOLTage:DC?
    double measureVoltageACDC();                 // MEASure:VOLTage:ACDC?

    // 量測：電流各類
    double measureCurrentDC();                   // MEASure:CURRent:DC?
    double measureCurrentACDC();                 // MEASure:CURRent:ACDC?
    double measureCurrentPeak();                 // MEASure:CURRent:AMPLitude:MAXimum?
    double measureCrestFactor();                 // MEASure:CURRent:CREStfactor?
    double measureInrushCurrent();               // MEASure:CURRent:INRush?

    // 量測：三相線電壓
    double measureLineV12();                     // MEASure:LINE:V12?
    double measureLineV23();                     // MEASure:LINE:V23?
    double measureLineV31();                     // MEASure:LINE:V31?

    // 量測：三相總功率
    double measureTotalRealPower();              // MEASure:POWer:AC:TOTal?
    double measureTotalApparentPower();          // MEASure:POWer:AC:TOTal:APParent?

private:
    static constexpr double kFreqMin =   15.0;
    static constexpr double kFreqMax = 2000.0;
};
