#pragma once
#include "dcload.h"

// ─────────────────────────────────────────────────────────────────────
//  Chroma 63804 Programmable AC/DC Electronic Load Driver
//  Protocol : GPIB / RS-232C (SCPI)
//  Manual   : 63800 Series Operation & Programming Manual v2.4 (2025-06)
//
//  63804 Specs:
//    Max Power       : 4,500 W
//    Current (AC)    : 0 ~ 45 A rms / 0 ~ 135 A peak
//    Current (DC)    : 0 ~ 45 A
//    Voltage (AC)    : 50 ~ 350 V rms / 500 V peak
//    Voltage (DC)    : 7.5 ~ 500 V
//    Frequency       : 45 ~ 440 Hz, DC
//    Protection      : OCP 48 A rms, OVP 360 V rms (DC: 510 V),
//                      OPP 4800 W, OTP, FANFAIL, OVPP (Vpeak > 600 V)
//
//  ⚠ 與 6310 / 63600 的重大架構差異：
//    - 63804 為 AC/DC 兩用負載，無「分檔位 (CCL/CCH)」概念
//    - 無 L1/L2 多段靜態電流機制；AC 電流直接設定 Irms
//    - 多一組 CF (Crest Factor) / PF (Power Factor) 概念參數
//    - 有 RLC / INRUSH / RECT 等 AC 專屬模式
//    - 需先用 SYST:SETU:MODE AC|DC 切換 AC / DC 工作模式
//
//  DCLoad 介面映射說明：
//    setStaticCurrent(param)
//      → 切換 MODE CURR，設定 CURR (Irms / Idc)
//        param.levels[0] = Irms (AC) 或 I (DC)
//        param.levels[1] = Ip(max)（可選）→ CURR:PEAK:MAX
//
//    setDynamicCurrent(param)
//      → 切換 MODE CURR，設定基準電流 + 斜率
//        param.levels[0]  = 電流值
//        param.timings[0] = 上升斜率 (A/ms)
//        param.timings[1] = 下降斜率 (A/ms)
//      ※ 63804 無 L1/L2 動態切換模式；
//         若需 INRUSH 模擬請直接呼叫 setInrush*() 系列方法
// ─────────────────────────────────────────────────────────────────────

class Chroma63804 : public DCLoad
{
public:
    explicit Chroma63804(ICommunication* comm = nullptr)
        : DCLoad(comm) {}

    ~Chroma63804() override;

    // ── InstrumentBase ────────────────────────────────────────────────
    QString model()  const override;
    QString vendor() const override;

    // ── DCLoad 必要介面 ───────────────────────────────────────────────

    void setLoadOn()  override;     // LOAD ON
    void setLoadOff() override;     // LOAD OFF

    int getNumSegments() const override { return 1; }  // 63804 無 L1/L2 分段

    LoadSyncCapability syncCapability() const override { return LoadSyncCapability::ParallelPosition; }

    // 設定負載模式（字串參數）
    //   有效值：CURR | POW | VOLT | RES | RLC | RLCP | INRUSH | RECT
    void setLoadMode(const QString& mode) override;

    // Von 在 63804 中對應截止電壓 TIME:VCUT
    void setVon(double von) override;

    // DC CC 模式斜率（A/ms），63804 範圍：4 ~ 600
    void setStaticRiseSlope(double slope) override;   // CURR:RISE
    void setStaticFallSlope(double slope) override;   // CURR:FALL

    // 同 Static（63804 的動態斜率使用同一組命令）
    void setDynamicRiseSlope(double slope) override;
    void setDynamicFallSlope(double slope) override;

    // ─ setStaticCurrent ─────────────────────────────────────────────
    //  param.levels[0]  = Irms (AC) 或 I (DC)
    //  param.levels[1]  = Ip(max) [可選] → CURR:PEAK:MAX (AC CC/CP/RLC)
    //                                   或 CURR:MAX:DC    (DC 模式)
    //  param.expectedVoltage 未使用（63804 為全電壓範圍負載）
    void setStaticCurrent(const StaticCurrentParam& param)  override;

    // ─ setDynamicCurrent ────────────────────────────────────────────
    //  ⚠ 63804 無 L1/L2 自動循環（不同於 6310 的 CCDL/CCDH），執行單次切換。
    //
    //  接受 Page3ViewModel 原始格式，驅動層內部自動換算：
    //    param.levels[0]   = 初始電流 I0 (A)
    //    param.levels[1]   = 目標電流 I1 (A)
    //    param.timings[0]  = T1 停留時間 (s) → 換算為 CURR:RISE (A/ms)
    //    param.timings[1]  = T2 停留時間 (s) → 換算為 CURR:FALL (A/ms)
    //
    //  換算：slew = |I1-I0| / (T × 1000)，clamp [4, 600] A/ms
    void setDynamicCurrent(const DynamicCurrentParam& param) override;

    // ── 63804 專屬：系統模式切換 ─────────────────────────────────────

    enum class OperatingMode { AC, DC };
    void setOperatingMode(OperatingMode mode);         // SYST:SETU:MODE AC|DC

    // ── 63804 專屬：LOAD 子系統 ──────────────────────────────────────

    void setShortCircuit(bool on);                    // SCIR ON|OFF
    void clearProtection();                           // PROT:CLE
    int  queryProtection();                           // PROT?  → 返回位元旗標

    void setABA(bool on);                             // ABA ON|OFF (Auto Bandwidth Adj.)

    // ── 63804 專屬：Current 子系統 ───────────────────────────────────

    // AC RMS 電流（CC 模式），0 ~ 45 A
    void setCurrentAC(double a);                      // CURR <n>

    // DC 電流，0 ~ 45 A
    void setCurrentDC(double a);                      // CURR:DC <n>

    // 最大 RMS 電流上限（不支援並聯模式），0 ~ 45 A
    void setCurrentHigh(double a);                    // CURR:HIGH <n>

    // Irms(max)，僅 AC CR 模式，0 ~ 45 A
    void setCurrentMaxAC(double a);                   // CURR:MAX <n>

    // Ip(max)，DC 模式，0 ~ 135 A
    void setCurrentMaxDC(double a);                   // CURR:MAX:DC <n>

    // Ip(max)，AC CC/CP/RLC/INRUSH 模式，0 ~ 135 A
    void setCurrentPeakMax(double a);                 // CURR:PEAK:MAX <n>

    // ── 63804 專屬：Power 子系統 ─────────────────────────────────────

    // AC 功率（CP 模式），0 ~ 4500 W
    void setPowerAC(double w);                        // POW <n>

    // DC 功率，0 ~ 4500 W
    void setPowerDC(double w);                        // POW:DC <n>

    // 最大功率上限，0 ~ 4500 W（不支援並聯模式）
    void setPowerHigh(double w);                      // POW:HIGH <n>

    // ── 63804 專屬：Resistance 子系統 ────────────────────────────────

    // AC 電阻（CR 模式），1.11 ~ 2500 Ω
    void setResistanceAC(double ohm);                 // RES <n>

    // DC 電阻，1.00 ~ 2500 Ω
    void setResistanceDC(double ohm);                 // RES:DC <n>

    // CR 模式電流斜率（A/ms），4 ~ 600
    void setResistanceRise(double slope);             // RES:RISE <n>
    void setResistanceFall(double slope);             // RES:FALL <n>

    // ── 63804 專屬：Voltage 子系統 ───────────────────────────────────

    // DC CV 模式電壓，7.50 ~ 500.00 V
    void setVoltageDC(double v);                      // VOLT:DC <n>

    // ── 63804 專屬：Crest Factor 子系統 ──────────────────────────────

    // AC CC/CP 模式峰值因素，1.414 ~ 5.000
    void setCrestFactorAC(double cf);                 // CFAC <n>

    // DC 整流模式峰值因素，1.414 ~ 5.000
    void setCrestFactorDC(double cf);                 // CFAC:DC <n>

    // ── 63804 專屬：Power Factor 子系統 ──────────────────────────────

    // 功率因素，-1.000 ~ +1.000（負值為超前，正值為落後）
    void setPowerFactor(double pf);                   // PFAC <n>

    // CF/PF 模式選擇（僅 AC CC/CP 模式）
    enum class CfPfMode { CF, PF, All };
    void setCfPfMode(CfPfMode mode);                  // SYST:SETU:CFPF:MODE CF|PF|ALL

    enum class CfPfPriority { CF, PF };
    void setCfPfPriority(CfPfPriority prio);          // SYST:SETU:CFPF:PRIO CF|PF

    // ── 63804 專屬：RLC 子系統（AC RLC 模式）────────────────────────

    void setRlcCapacitance(int uF);                   // RLC:CAP <n>  (100~9999 uF)
    void setRlcInductance(int uH);                    // RLC:LS  <n>  (0~9999 uH)
    void setRlcLoadResistance(double ohm);            // RLC:RL  <n>  (1.11~9999.99 Ω)
    void setRlcSeriesResistance(double ohm);          // RLC:RS  <n>  (0~9.999 Ω)

    // RLC CP 模式專屬
    void setRlcPower(double w);                       // RLC:POW  <n> (100~4500 W)
    void setRlcPowerFactor(double pf);                // RLC:PFAC <n> (0.400~0.750)

    // ── 63804 專屬：Inrush Current 子系統（AC 浪湧電流模式）──────────

    void setInrushCapacitance(int uF);                // INR:CAP  <n>  (100~9999 uF)
    void setInrushInductance(int uH);                 // INR:LS   <n>  (0~9999 uH)
    void setInrushLoadResistance(double ohm);         // INR:RL   <n>  (1.11~9999.99 Ω)
    void setInrushSeriesResistance(double ohm);       // INR:RS   <n>  (0~9.999 Ω)
    void setInrushPhase(double deg);                  // INR:PHAS <n>  (0~359 °)

    // ── 63804 專屬：DC 整流子系統 ────────────────────────────────────

    void setDcRectSync(bool lineSync);                // SYNC:DC ON(線路同步) | OFF(使用者設定頻率)
    void setDcRectFrequency(double hz);               // FREQ:DC <n>  (40~440 Hz)

    // ── 63804 專屬：Time 子系統 ──────────────────────────────────────

    enum class TimeMode { Off, Hold, Transfer };
    void setTimeMode(TimeMode mode);                  // TIME:MODE OFF|HOLD|TRAN

    // 最大計時時間，0 ~ 215999 s
    void setTimeTimeout(int sec);                     // TIME:TOUT <n>

    // 截止電壓，0 ~ 500.000 V（低於此電壓計時器停止）
    void setTimeCutoffVoltage(double v);              // TIME:VCUT <n>

    // ── 63804 專屬：量測 ─────────────────────────────────────────────

    double measureCurrent();                          // MEAS:CURR?      (RMS / 即時)
    double measureCurrentPeakMax();                   // MEAS:CURR:AMPL:MAX?
    double measureCurrentCrestFactor();               // MEAS:CURR:CRES?
    double measureCurrentPeakNeg();                   // MEAS:CURR:PEAK:NEG?
    double measureCurrentPeakPos();                   // MEAS:CURR:PEAK:POS?

    double measureFrequency();                        // MEAS:FREQ?

    double measureRealPower();                        // MEAS:POW?
    double measurePeakPower();                        // MEAS:POW:AMPL:MAX?
    double measureApparentPower();                    // MEAS:POW:APP?
    double measurePowerFactor();                      // MEAS:POW:PFAC?
    double measureReactivePower();                    // MEAS:POW:REAC?

    double measureResistance();                       // MEAS:RES?

    double measureVoltage();                          // MEAS:VOLT?      (RMS / 即時)
    double measureVoltagePeakMax();                   // MEAS:VOLT:AMPL:MAX?
    double measureVoltageDC();                        // MEAS:VOLT:DC?
    double measureVoltageTHD();                       // MEAS:VOLT:THD?
    double measureVoltageOvershoot();                 // MEAS:VOLT:OVER? (DC 模式)
    double measureVoltageUndershoot();                // MEAS:VOLT:UND?  (DC 模式)

    double measureHoldTime();                         // MEAS:TIME:HOLD? (ms)
    double measureTransferTime();                     // MEAS:TIME:TRAN? (ms)

    double measureRlcCapacitance();                   // MEAS:RLC:SET:CAP?
    double measureRlcInductance();                    // MEAS:RLC:SET:LS?
    double measureRlcLoadResistance();                // MEAS:RLC:SET:RL?
    double measureRlcSeriesResistance();              // MEAS:RLC:SET:RS?

    void setPeakCurrentHold(bool hold);               // MEAS:CURR:AMPL:HOLD ON|OFF

    // ── 並聯 / 相位選擇（多台並聯使用）─────────────────────────────

    int queryParallelStatus();                        // PAR:STAT?  0=錯誤 1=單相 2=三相

    enum class PhaseSelect { All, A, B, C };
    void setPhaseSelect(PhaseSelect phase);           // PHAS:SEL ALL|A|B|C

    // ── 系統設定 ─────────────────────────────────────────────────────

    void recallDefault();                             // SYST:DEF:RECA ON
};
