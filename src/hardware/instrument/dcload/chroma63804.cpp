#include "chroma63804.h"
#include <QDebug>
#include <algorithm>

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────
Chroma63804::~Chroma63804()
{
    disconnect();
}

// ─────────────────────────────────────────────
//  InstrumentBase
// ─────────────────────────────────────────────
QString Chroma63804::model()  const { return "63804"; }
QString Chroma63804::vendor() const { return "Chroma"; }

// ─────────────────────────────────────────────
//  DCLoad 必要介面
// ─────────────────────────────────────────────

// LOAD ON
void Chroma63804::setLoadOn()
{
    sendCommandWithLog("LOAD ON", "[Chroma63804]");
}

// LOAD OFF
void Chroma63804::setLoadOff()
{
    sendCommandWithLog("LOAD OFF", "[Chroma63804]");
}

// [LOAD:]MODE CURR|POW|VOLT|RES|RLC|RLCP|INRUSH|RECT
// Manual 8.6.3: 數值 0=CURR, 1=POW, 2=VOLT, 3=RES, 4=RLC, 5=RLCP, 6=INRUSH, 7=RECT
void Chroma63804::setLoadMode(const QString& mode)
{
    static const QSet<QString> validModes = {
        "CURR", "POW", "VOLT", "RES", "RLC", "RLCP", "INRUSH", "RECT"
    };
    if (!validModes.contains(mode.toUpper())) {
        qWarning() << "[Chroma63804] setLoadMode: 無效模式:" << mode;
        return;
    }
    sendCommandWithLog(
        QString("MODE %1").arg(mode.toUpper()),
        "[Chroma63804]");
}

// 對應 DCLoad::setVon → 63804 沒有 Von 概念，映射到截止電壓 TIME:VCUT
// Manual 8.6.3.10: TIME:VCUT (0~500 V)
void Chroma63804::setVon(double von)
{
    sendCommandWithLog(
        QString("TIME:VCUT %1").arg(von, 0, 'f', 3),
        "[Chroma63804]");
}

// CURR:RISE (Static 上升斜率，DC CC 模式，4~600 A/ms)
void Chroma63804::setStaticRiseSlope(double slope)
{
    sendCommandWithLog(
        QString("CURR:RISE %1").arg(static_cast<int>(slope)),
        "[Chroma63804]");
}

// CURR:FALL (Static 下降斜率，DC CC 模式，4~600 A/ms)
void Chroma63804::setStaticFallSlope(double slope)
{
    sendCommandWithLog(
        QString("CURR:FALL %1").arg(static_cast<int>(slope)),
        "[Chroma63804]");
}

// 63804 無獨立動態斜率命令，使用同一組 CURR:RISE / CURR:FALL
void Chroma63804::setDynamicRiseSlope(double slope)
{
    setStaticRiseSlope(slope);
}

void Chroma63804::setDynamicFallSlope(double slope)
{
    setStaticFallSlope(slope);
}

// ─────────────────────────────────────────────
//  setStaticCurrent
//
//  DCLoad 介面映射：
//    param.levels[0]  = Irms (AC) 或 I (DC)
//    param.levels[1]  = Ip(max) [可選]
//       → AC 模式: CURR:PEAK:MAX（CC/CP/RLC/INRUSH 模式，0~135 A）
//       → DC 模式: CURR:MAX:DC（DC 模式，0~135 A）
//  param.expectedVoltage 未使用
// ─────────────────────────────────────────────
void Chroma63804::setStaticCurrent(const StaticCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    // 切換到 CC 模式
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode != "NO SETTING")
        sendCommandWithLog("MODE CURR", "[Chroma63804] setStaticCurrent");

    // 設定主電流（levels[0]）
    const double irms = param.levels[0];
    sendCommandWithLog(
        QString("CURR %1").arg(irms, 0, 'f', 3),
        "[Chroma63804] setStaticCurrent: Irms");

    // 若有 levels[1]，設定 Ip(max)
    if (param.levels.size() >= 2) {
        const double ipMax = param.levels[1];
        // 判斷 AC / DC：AC 峰值上限最大 135 A，DC 峰值上限亦為 135 A
        // 預設走 AC 路徑 (CURR:PEAK:MAX)；DC 路徑由呼叫者用 setCurrentMaxDC() 控制
        sendCommandWithLog(
            QString("CURR:PEAK:MAX %1").arg(ipMax, 0, 'f', 3),
            "[Chroma63804] setStaticCurrent: Ip(max)");
    }

    if (param.levels.size() > 2) {
        qWarning() << "[Chroma63804] setStaticCurrent: "
                      "63804 僅支援 levels[0]=Irms / levels[1]=Ip(max)，忽略 levels[2+]";
    }
}

// ─────────────────────────────────────────────
//  setDynamicCurrent
//
//  ⚠ 63804 架構說明：
//    63804「無」L1/L2 自動循環動態模式（不像 6310 的 CCDL/CCDH）。
//    動態電流切換靠「重新發送 CURR:DC 指令」或「外部 TTL 觸發」實現。
//    本方法模擬單次切換：先設定初始電流，再切換到目標電流。
//
//  DynamicCurrentParam 語意（接受 Page3ViewModel 傳入格式，驅動層內部換算）：
//    levels[0]   = 初始電流 I₀ (A)          → CURR:DC <I₀>
//    levels[1]   = 目標電流 I₁ (A)          → CURR:DC <I₁>（單次切換）
//    timings[0]  = T1 停留時間 (s)          → 內部換算為上升斜率 (A/ms)
//    timings[1]  = T2 停留時間 (s)          → 內部換算為下降斜率 (A/ms)
//
//  換算公式：
//    slew (A/ms) = ΔI (A) / (T (s) × 1000)
//    ΔI = |I1 - I0|
//    結果 clamp 至 [4, 600] A/ms（手冊限制）
//
//  ⚠ 換算說明：
//    Page3 傳入 T1/T2 秒，代表「希望電流在 T 秒內完成切換」
//    63804 需要的是斜率（每毫秒幾安培），由驅動層自行換算
//    上層 Page3ViewModel 不需要任何修改
// ─────────────────────────────────────────────
void Chroma63804::setDynamicCurrent(const DynamicCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    constexpr double kMinSlew = 4.0;    // 手冊規定最小斜率 4 A/ms
    constexpr double kMaxSlew = 600.0;  // 手冊規定最大斜率 600 A/ms

    const double i0 = param.levels[0];
    const double i1 = (param.levels.size() >= 2) ? param.levels[1] : i0;
    const double deltaI = qAbs(i1 - i0);  // A

    // ── 換算輔助 lambda：T(秒) → slew(A/ms) ────────
    // 若 deltaI = 0（電流不變），斜率意義不大，給最小值
    // 若 T <= 0（無效時間），斜率給最大值（盡快切換）
    auto timeToSlew = [&](double t_sec) -> double {
        if (t_sec <= 0.0 || deltaI == 0.0)
            return kMinSlew;
        double slew = deltaI / (t_sec * 1000.0);  // A/ms
        return qBound(kMinSlew, slew, kMaxSlew);
    };

    // ── Step 1：切換到 DC CC 模式 ──────────────────
    const QString requestedMode = param.loadMode.trimmed().toUpper();
    if (requestedMode != "NO SETTING") {
        sendCommandWithLog("SYST:SETU:MODE DC", "[Chroma63804] setDynamicCurrent: DC mode");
        sendCommandWithLog("MODE CURR",         "[Chroma63804] setDynamicCurrent: CC mode");
    }

    // ── Step 2：T1/T2（秒）→ 斜率（A/ms）換算 ──────
    const double t1 = (param.timings.size() >= 1) ? param.timings[0] : 0.0;
    const double t2 = (param.timings.size() >= 2) ? param.timings[1] : t1;

    const double rise = timeToSlew(t1);
    const double fall = timeToSlew(t2);

    qDebug() << "[Chroma63804] setDynamicCurrent:"
             << "I0=" << i0 << "A, I1=" << i1 << "A, deltaI=" << deltaI << "A"
             << "| T1=" << t1 << "s → RISE=" << rise << "A/ms"
             << "| T2=" << t2 << "s → FALL=" << fall << "A/ms";

    sendCommandWithLog(
        QString("CURR:RISE %1").arg(static_cast<int>(rise)),
        "[Chroma63804] setDynamicCurrent: RISE");
    sendCommandWithLog(
        QString("CURR:FALL %1").arg(static_cast<int>(fall)),
        "[Chroma63804] setDynamicCurrent: FALL");

    // ── Step 3：設定初始電流（I₀）─────────────────
    sendCommandWithLog(
        QString("CURR:DC %1").arg(i0, 0, 'f', 3),
        "[Chroma63804] setDynamicCurrent: 初始電流 I0");

    // ── Step 4：切換到目標電流（I₁）─────────────────
    // 63804 無自動循環，直接送出目標電流實現單次動態切換
    if (param.levels.size() >= 2) {
        sendCommandWithLog(
            QString("CURR:DC %1").arg(i1, 0, 'f', 3),
            "[Chroma63804] setDynamicCurrent: 目標電流 I1（單次切換）");
    } else {
        qWarning() << "[Chroma63804] setDynamicCurrent: "
                      "levels[1] 未提供，不執行電流切換（63804 無自動循環）";
    }
}

// ─────────────────────────────────────────────
//  系統模式切換
// ─────────────────────────────────────────────

// SYST:SETU:MODE AC|DC
void Chroma63804::setOperatingMode(OperatingMode mode)
{
    sendCommandWithLog(
        QString("SYST:SETU:MODE %1").arg(mode == OperatingMode::AC ? "AC" : "DC"),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  LOAD 子系統
// ─────────────────────────────────────────────

// [LOAD:]SCIRcuit ON|OFF
void Chroma63804::setShortCircuit(bool on)
{
    sendCommandWithLog(
        QString("SCIR %1").arg(on ? "ON" : "OFF"),
        "[Chroma63804]");
}

// [LOAD:]PROTect:CLE
void Chroma63804::clearProtection()
{
    sendCommandWithLog("PROT:CLE", "[Chroma63804]");
}

// [LOAD:]PROTect?  → 返回位元旗標
// Bit0=FE, Bit1=UV(>2ms), Bit2=UV(>8ms), Bit3=OVP,
// Bit4=OVPP, Bit5=OCP, Bit6=OPP, Bit7=OTP,
// Bit8=FANFAIL, Bit9=LDF, Bit10=FREQERR, Bit11=PWRFAIL
int Chroma63804::queryProtection()
{
    double v = 0.0;
    queryDouble("PROT?", v);
    return static_cast<int>(v);
}

// [LOAD:]ABA ON|OFF  (Auto Bandwidth Adjustment)
void Chroma63804::setABA(bool on)
{
    sendCommandWithLog(
        QString("ABA %1").arg(on ? "ON" : "OFF"),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  Current 子系統
// ─────────────────────────────────────────────

// CURR <n>  → AC RMS 電流，0 ~ 45 A
void Chroma63804::setCurrentAC(double a)
{
    sendCommandWithLog(
        QString("CURR %1").arg(a, 0, 'f', 3),
        "[Chroma63804]");
}

// CURR:DC <n>  → DC 電流，0 ~ 45 A
void Chroma63804::setCurrentDC(double a)
{
    sendCommandWithLog(
        QString("CURR:DC %1").arg(a, 0, 'f', 3),
        "[Chroma63804]");
}

// CURR:HIGH <n>  → 最大 RMS 電流上限，0 ~ 45 A（不支援並聯模式）
void Chroma63804::setCurrentHigh(double a)
{
    sendCommandWithLog(
        QString("CURR:HIGH %1").arg(static_cast<int>(a)),
        "[Chroma63804]");
}

// CURR:MAX <n>  → Irms(max)，AC CR 模式，0 ~ 45 A
void Chroma63804::setCurrentMaxAC(double a)
{
    sendCommandWithLog(
        QString("CURR:MAX %1").arg(a, 0, 'f', 3),
        "[Chroma63804]");
}

// CURR:MAX:DC <n>  → Ip(max)，DC 模式，0 ~ 135 A
void Chroma63804::setCurrentMaxDC(double a)
{
    sendCommandWithLog(
        QString("CURR:MAX:DC %1").arg(a, 0, 'f', 3),
        "[Chroma63804]");
}

// CURR:PEAK:MAX <n>  → Ip(max)，AC CC/CP/RLC/INRUSH，0 ~ 135 A
void Chroma63804::setCurrentPeakMax(double a)
{
    sendCommandWithLog(
        QString("CURR:PEAK:MAX %1").arg(a, 0, 'f', 3),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  Power 子系統
// ─────────────────────────────────────────────

// POW <n>  → AC 功率，0 ~ 4500 W
void Chroma63804::setPowerAC(double w)
{
    sendCommandWithLog(
        QString("POW %1").arg(w, 0, 'f', 2),
        "[Chroma63804]");
}

// POW:DC <n>  → DC 功率，0 ~ 4500 W
void Chroma63804::setPowerDC(double w)
{
    sendCommandWithLog(
        QString("POW:DC %1").arg(w, 0, 'f', 2),
        "[Chroma63804]");
}

// POW:HIGH <n>  → 最大功率上限（不支援並聯模式），0 ~ 4500 W
void Chroma63804::setPowerHigh(double w)
{
    sendCommandWithLog(
        QString("POW:HIGH %1").arg(static_cast<int>(w)),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  Resistance 子系統
// ─────────────────────────────────────────────

// RES <n>  → AC CR 模式電阻，1.11 ~ 2500 Ω
void Chroma63804::setResistanceAC(double ohm)
{
    sendCommandWithLog(
        QString("RES %1").arg(ohm, 0, 'f', 2),
        "[Chroma63804]");
}

// RES:DC <n>  → DC CR 模式電阻，1.00 ~ 2500 Ω
void Chroma63804::setResistanceDC(double ohm)
{
    sendCommandWithLog(
        QString("RES:DC %1").arg(ohm, 0, 'f', 2),
        "[Chroma63804]");
}

// RES:RISE <n>  → CR 電流上升斜率 (A/ms)，4 ~ 600
void Chroma63804::setResistanceRise(double slope)
{
    sendCommandWithLog(
        QString("RES:RISE %1").arg(static_cast<int>(slope)),
        "[Chroma63804]");
}

// RES:FALL <n>  → CR 電流下降斜率 (A/ms)，4 ~ 600
void Chroma63804::setResistanceFall(double slope)
{
    sendCommandWithLog(
        QString("RES:FALL %1").arg(static_cast<int>(slope)),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  Voltage 子系統
// ─────────────────────────────────────────────

// VOLT:DC <n>  → DC CV 模式，7.50 ~ 500.00 V
void Chroma63804::setVoltageDC(double v)
{
    sendCommandWithLog(
        QString("VOLT:DC %1").arg(v, 0, 'f', 2),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  Crest Factor 子系統
// ─────────────────────────────────────────────

// CFAC <n>  → AC CC/CP 模式，1.414 ~ 5.000
void Chroma63804::setCrestFactorAC(double cf)
{
    sendCommandWithLog(
        QString("CFAC %1").arg(cf, 0, 'f', 3),
        "[Chroma63804]");
}

// CFAC:DC <n>  → DC 整流模式，1.414 ~ 5.000
void Chroma63804::setCrestFactorDC(double cf)
{
    sendCommandWithLog(
        QString("CFAC:DC %1").arg(cf, 0, 'f', 3),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  Power Factor 子系統
// ─────────────────────────────────────────────

// PFAC <n>  → -1.000 ~ +1.000（負=超前，正=落後）
void Chroma63804::setPowerFactor(double pf)
{
    sendCommandWithLog(
        QString("PFAC %1").arg(pf, 0, 'f', 3),
        "[Chroma63804]");
}

// SYST:SETU:CFPF:MODE CF|PF|ALL
void Chroma63804::setCfPfMode(CfPfMode mode)
{
    QString param;
    switch (mode) {
    case CfPfMode::CF:  param = "CF";  break;
    case CfPfMode::PF:  param = "PF";  break;
    case CfPfMode::All: param = "ALL"; break;
    }
    sendCommandWithLog(
        QString("SYST:SETU:CFPF:MODE %1").arg(param),
        "[Chroma63804]");
}

// SYST:SETU:CFPF:PRIO CF|PF
void Chroma63804::setCfPfPriority(CfPfPriority prio)
{
    sendCommandWithLog(
        QString("SYST:SETU:CFPF:PRIO %1").arg(prio == CfPfPriority::CF ? "CF" : "PF"),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  RLC 子系統（AC RLC 模式）
// ─────────────────────────────────────────────

// RLC:CAP <n>  → 100 ~ 9999 uF
void Chroma63804::setRlcCapacitance(int uF)
{
    sendCommandWithLog(
        QString("RLC:CAP %1").arg(uF),
        "[Chroma63804]");
}

// RLC:LS <n>  → 0 ~ 9999 uH
void Chroma63804::setRlcInductance(int uH)
{
    sendCommandWithLog(
        QString("RLC:LS %1").arg(uH),
        "[Chroma63804]");
}

// RLC:RL <n>  → 1.11 ~ 9999.99 Ω（63804）
void Chroma63804::setRlcLoadResistance(double ohm)
{
    sendCommandWithLog(
        QString("RLC:RL %1").arg(ohm, 0, 'f', 2),
        "[Chroma63804]");
}

// RLC:RS <n>  → 0 ~ 9.999 Ω
void Chroma63804::setRlcSeriesResistance(double ohm)
{
    sendCommandWithLog(
        QString("RLC:RS %1").arg(ohm, 0, 'f', 3),
        "[Chroma63804]");
}

// RLC:POW <n>  → RLC CP 模式功率，100 ~ 4500 W
void Chroma63804::setRlcPower(double w)
{
    sendCommandWithLog(
        QString("RLC:POW %1").arg(w, 0, 'f', 2),
        "[Chroma63804]");
}

// RLC:PFAC <n>  → RLC CP 模式功率因素，0.400 ~ 0.750
void Chroma63804::setRlcPowerFactor(double pf)
{
    sendCommandWithLog(
        QString("RLC:PFAC %1").arg(pf, 0, 'f', 3),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  Inrush Current 子系統
// ─────────────────────────────────────────────

// INR:CAP <n>  → 100 ~ 9999 uF
void Chroma63804::setInrushCapacitance(int uF)
{
    sendCommandWithLog(
        QString("INR:CAP %1").arg(uF),
        "[Chroma63804]");
}

// INR:LS <n>  → 0 ~ 9999 uH
void Chroma63804::setInrushInductance(int uH)
{
    sendCommandWithLog(
        QString("INR:LS %1").arg(uH),
        "[Chroma63804]");
}

// INR:RL <n>  → 1.11 ~ 9999.99 Ω
void Chroma63804::setInrushLoadResistance(double ohm)
{
    sendCommandWithLog(
        QString("INR:RL %1").arg(ohm, 0, 'f', 2),
        "[Chroma63804]");
}

// INR:RS <n>  → 0 ~ 9.999 Ω
void Chroma63804::setInrushSeriesResistance(double ohm)
{
    sendCommandWithLog(
        QString("INR:RS %1").arg(ohm, 0, 'f', 3),
        "[Chroma63804]");
}

// INR:PHAS <n>  → 啟動相位，0 ~ 359 °
void Chroma63804::setInrushPhase(double deg)
{
    sendCommandWithLog(
        QString("INR:PHAS %1").arg(static_cast<int>(deg)),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  DC 整流子系統
// ─────────────────────────────────────────────

// SYNC:DC ON(線路同步) | OFF(使用者設定頻率)
void Chroma63804::setDcRectSync(bool lineSync)
{
    sendCommandWithLog(
        QString("SYNC:DC %1").arg(lineSync ? "ON" : "OFF"),
        "[Chroma63804]");
}

// FREQ:DC <n>  → 40 ~ 440 Hz（SYNC=OFF 時有效）
void Chroma63804::setDcRectFrequency(double hz)
{
    sendCommandWithLog(
        QString("FREQ:DC %1").arg(hz, 0, 'f', 1),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  Time 子系統
// ─────────────────────────────────────────────

// TIME:MODE OFF|HOLD|TRAN
void Chroma63804::setTimeMode(TimeMode mode)
{
    QString param;
    switch (mode) {
    case TimeMode::Off:      param = "OFF";  break;
    case TimeMode::Hold:     param = "HOLD"; break;
    case TimeMode::Transfer: param = "TRAN"; break;
    }
    sendCommandWithLog(
        QString("TIME:MODE %1").arg(param),
        "[Chroma63804]");
}

// TIME:TOUT <n>  → 0 ~ 215999 s
void Chroma63804::setTimeTimeout(int sec)
{
    sendCommandWithLog(
        QString("TIME:TOUT %1").arg(sec),
        "[Chroma63804]");
}

// TIME:VCUT <n>  → 0 ~ 500.000 V
void Chroma63804::setTimeCutoffVoltage(double v)
{
    sendCommandWithLog(
        QString("TIME:VCUT %1").arg(v, 0, 'f', 3),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  量測子系統
// ─────────────────────────────────────────────

// MEAS:CURR?  → RMS / 即時電流
double Chroma63804::measureCurrent()
{
    double v = 0.0;
    queryDouble("MEAS:CURR?", v);
    return v;
}

// MEAS:CURR:AMPL:MAX?  → 峰值電流
double Chroma63804::measureCurrentPeakMax()
{
    double v = 0.0;
    queryDouble("MEAS:CURR:AMPL:MAX?", v);
    return v;
}

// MEAS:CURR:CRES?  → 電流峰值因素
double Chroma63804::measureCurrentCrestFactor()
{
    double v = 0.0;
    queryDouble("MEAS:CURR:CRES?", v);
    return v;
}

// MEAS:CURR:PEAK:NEG?  → 負峰值電流
double Chroma63804::measureCurrentPeakNeg()
{
    double v = 0.0;
    queryDouble("MEAS:CURR:PEAK:NEG?", v);
    return v;
}

// MEAS:CURR:PEAK:POS?  → 正峰值電流
double Chroma63804::measureCurrentPeakPos()
{
    double v = 0.0;
    queryDouble("MEAS:CURR:PEAK:POS?", v);
    return v;
}

// MEAS:CURR:AMPL:HOLD ON|OFF
void Chroma63804::setPeakCurrentHold(bool hold)
{
    sendCommandWithLog(
        QString("MEAS:CURR:AMPL:HOLD %1").arg(hold ? "ON" : "OFF"),
        "[Chroma63804]");
}

// MEAS:FREQ?  → 頻率 (Hz)
double Chroma63804::measureFrequency()
{
    double v = 0.0;
    queryDouble("MEAS:FREQ?", v);
    return v;
}

// MEAS:POW?  → 實功率 (W)
double Chroma63804::measureRealPower()
{
    double v = 0.0;
    queryDouble("MEAS:POW?", v);
    return v;
}

// MEAS:POW:AMPL:MAX?  → 峰值功率
double Chroma63804::measurePeakPower()
{
    double v = 0.0;
    queryDouble("MEAS:POW:AMPL:MAX?", v);
    return v;
}

// MEAS:POW:APP?  → 視在功率 (VA)
double Chroma63804::measureApparentPower()
{
    double v = 0.0;
    queryDouble("MEAS:POW:APP?", v);
    return v;
}

// MEAS:POW:PFAC?  → 功率因素
double Chroma63804::measurePowerFactor()
{
    double v = 0.0;
    queryDouble("MEAS:POW:PFAC?", v);
    return v;
}

// MEAS:POW:REAC?  → 虛功率 (VAR)
double Chroma63804::measureReactivePower()
{
    double v = 0.0;
    queryDouble("MEAS:POW:REAC?", v);
    return v;
}

// MEAS:RES?  → 電阻 (Ω)
double Chroma63804::measureResistance()
{
    double v = 0.0;
    queryDouble("MEAS:RES?", v);
    return v;
}

// MEAS:VOLT?  → RMS / 即時電壓
double Chroma63804::measureVoltage()
{
    double v = 0.0;
    queryDouble("MEAS:VOLT?", v);
    return v;
}

// MEAS:VOLT:AMPL:MAX?  → 峰值電壓
double Chroma63804::measureVoltagePeakMax()
{
    double v = 0.0;
    queryDouble("MEAS:VOLT:AMPL:MAX?", v);
    return v;
}

// MEAS:VOLT:DC?  → Vdc（每週期平均值）
double Chroma63804::measureVoltageDC()
{
    double v = 0.0;
    queryDouble("MEAS:VOLT:DC?", v);
    return v;
}

// MEAS:VOLT:THD?  → 電壓總諧波失真
double Chroma63804::measureVoltageTHD()
{
    double v = 0.0;
    queryDouble("MEAS:VOLT:THD?", v);
    return v;
}

// MEAS:VOLT:OVER?  → 過衝電壓（DC 模式）
double Chroma63804::measureVoltageOvershoot()
{
    double v = 0.0;
    queryDouble("MEAS:VOLT:OVER?", v);
    return v;
}

// MEAS:VOLT:UND?  → 過低電壓（DC 模式）
double Chroma63804::measureVoltageUndershoot()
{
    double v = 0.0;
    queryDouble("MEAS:VOLT:UND?", v);
    return v;
}

// MEAS:TIME:HOLD?  → 保留時間 (ms)
double Chroma63804::measureHoldTime()
{
    double v = 0.0;
    queryDouble("MEAS:TIME:HOLD?", v);
    return v;
}

// MEAS:TIME:TRAN?  → 傳送時間 (ms)
double Chroma63804::measureTransferTime()
{
    double v = 0.0;
    queryDouble("MEAS:TIME:TRAN?", v);
    return v;
}

// MEAS:RLC:SET:CAP?  → RLC CP 模式 C 量測值
double Chroma63804::measureRlcCapacitance()
{
    double v = 0.0;
    queryDouble("MEAS:RLC:SET:CAP?", v);
    return v;
}

// MEAS:RLC:SET:LS?  → RLC CP 模式 Ls 量測值
double Chroma63804::measureRlcInductance()
{
    double v = 0.0;
    queryDouble("MEAS:RLC:SET:LS?", v);
    return v;
}

// MEAS:RLC:SET:RL?  → RLC CP 模式 RL 量測值
double Chroma63804::measureRlcLoadResistance()
{
    double v = 0.0;
    queryDouble("MEAS:RLC:SET:RL?", v);
    return v;
}

// MEAS:RLC:SET:RS?  → RLC CP 模式 RS 量測值
double Chroma63804::measureRlcSeriesResistance()
{
    double v = 0.0;
    queryDouble("MEAS:RLC:SET:RS?", v);
    return v;
}

// ─────────────────────────────────────────────
//  並聯 / 相位選擇
// ─────────────────────────────────────────────

// PAR:STAT?  → 0=錯誤, 1=單相並聯, 2=三相並聯
int Chroma63804::queryParallelStatus()
{
    double v = 0.0;
    queryDouble("PAR:STAT?", v);
    return static_cast<int>(v);
}

// PHAS:SEL ALL|A|B|C  （三相並聯使用）
void Chroma63804::setPhaseSelect(PhaseSelect phase)
{
    QString param;
    switch (phase) {
    case PhaseSelect::All: param = "ALL"; break;
    case PhaseSelect::A:   param = "A";   break;
    case PhaseSelect::B:   param = "B";   break;
    case PhaseSelect::C:   param = "C";   break;
    }
    sendCommandWithLog(
        QString("PHAS:SEL %1").arg(param),
        "[Chroma63804]");
}

// ─────────────────────────────────────────────
//  系統設定
// ─────────────────────────────────────────────

// SYST:DEF:RECA ON
void Chroma63804::recallDefault()
{
    sendCommandWithLog("SYST:DEF:RECA ON", "[Chroma63804]");
}
