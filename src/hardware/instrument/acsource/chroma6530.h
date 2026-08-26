#pragma once
#include "acsource.h"
#include "icommunication.h"

// ─────────────────────────────────────────────
//  Chroma 6530 Programmable AC Source Driver
//  Protocol : GPIB / RS-232C (SCPI)
//  Manual   : 6530/6520/6512 User's Manual v1.8 (2021-03)
//
//  Specs (6530):
//    Max Power        : 3 KVA
//    Voltage Range    : 0~150 V (LOW) / 0~300 V (HIGH)
//    Max Current RMS  : 30 A (LOW) / 15 A (HIGH)
//    Max Current Peak : 90 A (LOW, 15-100 Hz) / 45 A (HIGH)
//    Frequency Range  : 15~2000 Hz
//    Interface        : GPIB / RS-232C
//
//  NOTE: 6530 SCPI 指令集與 61509 系列有較大差異：
//    - 電壓設定為 VOLTage <NR2>，非 SOURce:VOLTage:AC
//    - 電壓檔位為獨立頂層命令 RANGe HIGH|LOW|AUTO
//    - 輸出繼電器為 ORELay ON|OFF，非 OUTPut:RELay
//    - 無 DC 電壓輸出 (純 AC 機型)
//    - setPhaseOn 對應 TPHase（暫態相位角）
//    - setPhaseOff 在此機型不支援，呼叫將產生警告
// ─────────────────────────────────────────────
class Chroma6530 : public ACSource
{
public:
    explicit Chroma6530(ICommunication* comm = nullptr);
    ~Chroma6530() override;

    // ── InstrumentBase ─────────────────────────
    QString model()  const override;
    QString vendor() const override;

    // ── ACSource (必要介面) ────────────────────
    // [SOURce:]VOLTage[:LEVel][:IMMediate][:AMPLitude] <NR2>
    void   setVoltage(double v)   override;

    // [SOURce:]FREQuency[:CW|:IMMediate] <NR2>   (15~2000 Hz)
    void   setFrequency(double f) override;

    // TPHase <NR2>  (暫態相位角，0.0~359.99°；6530 無 SOURce:PHASe:ON 命令)
    void   setPhaseOn(double p)   override;

    // ※ 6530 不支援波形結束相位角，呼叫此方法將輸出 qWarning 並忽略
    void   setPhaseOff(double p)  override;

    void   setPowerOn()  override;   // OUTPut ON
    void   setPowerOff() override;   // OUTPut OFF

    double measureVoltage()  override;   // MEASure:VOLTage:AC?
    double measureCurrent()  override;   // MEASure:CURRent:AC?

    double realPower()      override;    // MEASure:POWer:AC?
    double reactivePower()  override;    // MEASure:POWer:AC:REACtive?
    double apparentPower()  override;    // MEASure:POWer:AC:APParent?
    double powerPfactor()   override;    // MEASure:POWer:AC:PFACtor?

    double freQuency()      override;    // MEASure:FREQuency?

    // ── Chroma 6530 專屬功能 ──────────────────

    // 輸出電壓檔位 (LOW=150V / HIGH=300V / AUTO)
    enum class VoltageRange { Low, High, Auto };
    void setVoltageRange(VoltageRange range);      // RANGe LOW|HIGH|AUTO

    // 均方根電流限制（軟體 OCP 保護）
    void setCurrentLimit(double a);                // [SOURce:]CURRent[:LEVel][:IMMediate][:AMPLitude]

    // 輸出繼電器 (硬體)
    void setOutputRelay(bool on);                  // ORELay ON|OFF

    // 保護控制
    void clearProtection();                        // OUTPut:PROTection:CLEar
    void setProtectionDelay(double s);             // OUTPut:PROTection:DELay (0.0~100.0，單位 0.1s)

    // 相位反向（輸出起始相位移動 180°）
    void setPhaseReverse(bool on);                 // PHASe[:IMMediate]:REVerse ON|OFF

    // 暫態相位角（輸出同步相位基準）
    void setTransientPhase(double deg);            // TPHase <NR2>  (0.0~359.99)
    void setTransientPhaseSync(bool usePhase);     // TPHase:SYNC PHAS|IMM

    // 波形緩衝區選擇
    enum class WaveBuffer { A, B };
    void setActiveWaveBuffer(WaveBuffer buf);      // [SOURce:]FUNCtion:SHAPe A|B

    // 波形形狀設定（目標緩衝區 A 或 B）
    // shape: "SIN", "SQU", "CSIN<pct>", "DST<1-30>", "US<1-6>"
    void setWaveShape(WaveBuffer buf, const QString& shape);
    // [SOURce:]FUNCtion:SHAPe:A|B <shape>

    // 量測：電流峰值 / Crest Factor / Inrush
    double measureCurrentPeak();                   // MEASure:CURRent:AMPLitude:MAXimum?
    double measureCrestFactor();                   // MEASure:CURRent:CREStfactor?
    double measureInrushCurrent();                 // MEASure:CURRent:INRush?

    // 觸發系統
    void initiateTrigger();                        // INITiate[:IMMediate]
    void setContinuousTrigger(bool on);            // INITiate:CONTinuous:SEQuence1 ON|OFF
    void sendTrigger();                            // TRIGger[:SEQuence1|:TRANsient][:IMMediate]

    // STEP 模態參數
    void setStepVoltage(double v);                 // [SOURce:]STEP:VOLTage  (0.0~300.0)
    void setStepFrequency(double f);               // [SOURce:]STEP:FREQuency (15.0~2000.0)
    void setStepStartPhase(double deg);            // [SOURce:]STEP:SPHase   (0.00~359.99)
    void setStepWaveShape(WaveBuffer buf);         // [SOURce:]STEP:SHAPe A|B
    void setStepDeltaVoltage(double dv);           // [SOURce:]STEP:DVOLtage (-300~300)
    void setStepDeltaFrequency(double df);         // [SOURce:]STEP:DFRequency (-2000~2000)
    void setStepDwell(double sec);                 // [SOURce:]STEP:DWELl    (0.000~999.999 s)
    void setStepCount(int n);                      // [SOURce:]STEP:COUNt    (0~30000)

    // PULSE 模態參數
    void setPulseVoltage(double v);                // [SOURce:]PULSe:VOLTage   (0.0~300.0)
    void setPulseFrequency(double f);              // [SOURce:]PULSe:FREQuency  (0~2000 Hz)
    void setPulseStartPhase(double deg);           // [SOURce:]PULSe:SPHase    (0.00~359.99)
    void setPulseWaveShape(WaveBuffer buf);        // [SOURce:]PULSe:SHAPe A|B
    void setPulseCount(int n);                     // [SOURce:]PULSe:COUNt     (1~60000)
    void setPulseDutyCycle(double pct);            // [SOURce:]PULSe:DCYCle    (0.01~100.00 %)
    void setPulsePeriod(double ms);                // [SOURce:]PULSe:PERiod    (1.0~999999.0 ms)
    void stopPulse();                              // [SOURce:]PULSe:QUIT

    // LIST 模態基本參數
    void setListCount(int n);                      // [SOURce:]LIST:COUNt <n>|INFinity (1~60000)
    void setListBase(bool useCycle);               // [SOURce:]LIST:BASE TIME|CYCle
    void stopList();                               // [SOURce:]LIST:QUIT

    // 系統命令
    void setRemoteMode();                          // SYSTem:REMote
    void setLocalMode();                           // SYSTem:LOCal

private:
    static constexpr double kFreqMin =   15.0;
    static constexpr double kFreqMax = 2000.0;
};
