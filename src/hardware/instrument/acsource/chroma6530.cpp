#include "chroma6530.h"
#include <QDebug>
#include <algorithm>

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────
Chroma6530::Chroma6530(ICommunication* comm)
    : ACSource(comm) {}

Chroma6530::~Chroma6530()
{
    disconnect();
}

// ─────────────────────────────────────────────
//  InstrumentBase
// ─────────────────────────────────────────────
QString Chroma6530::model()  const { return "6530"; }
QString Chroma6530::vendor() const { return "Chroma"; }

// ─────────────────────────────────────────────
//  Output Control
//  Manual 6.6.2: OUTPut[:STATe] ON | OFF
// ─────────────────────────────────────────────
void Chroma6530::setPowerOn()
{
    // Relay 預設跟著 Power ON 一起動
    sendCommandWithLog("ORELay ON",  "[Chroma6530]");
    sendCommandWithLog("OUTPut ON",  "[Chroma6530]");
}

void Chroma6530::setPowerOff()
{
    sendCommandWithLog("OUTPut OFF", "[Chroma6530]");
    sendCommandWithLog("ORELay OFF", "[Chroma6530]");
    requireOutputOff("OUTPut?");
}


// ─────────────────────────────────────────────
//  SOURce: VOLTage 子系統
//  Manual 6.6.2: [SOURce:]VOLTage[:LEVel][:IMMediate][:AMPLitude]
//  注意：6530 為純 AC 機型，電壓命令直接設定 RMS 輸出電壓
//        與 61509 的 SOURce:VOLTage:AC 不同
// ─────────────────────────────────────────────
void Chroma6530::setVoltage(double v)
{
    setVoltageRange(VoltageRange::Auto);
    sendCommandWithLog(
        QString("VOLTage %1").arg(v, 0, 'f', 1),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  RANGe HIGH | LOW | AUTO
//  Manual 6.6.2: 頂層命令，非 SOURce 子系統
//  LOW=150V / HIGH=300V / AUTO=自動切換
// ─────────────────────────────────────────────
void Chroma6530::setVoltageRange(VoltageRange range)
{
    QString param;
    switch (range) {
    case VoltageRange::Low:  param = "LOW";  break;
    case VoltageRange::High: param = "HIGH"; break;
    case VoltageRange::Auto: param = "AUTO"; break;
    }
    sendCommandWithLog(
        QString("RANGe %1").arg(param),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  SOURce: FREQuency 子系統
//  Manual 6.6.2: [SOURce:]FREQuency[:CW|:IMMediate] <NR2>
//  Valid range: 15.00 ~ 2000.00 Hz
// ─────────────────────────────────────────────
void Chroma6530::setFrequency(double f)
{
    const double clamped = std::clamp(f, kFreqMin, kFreqMax);
    if (clamped != f) {
        qWarning() << "[Chroma6530] Frequency clamped:"
                   << f << "->" << clamped;
    }
    sendCommandWithLog(
        QString("FREQuency %1").arg(clamped, 0, 'f', 2),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  Phase (ACSource 必要介面實作)
//  6530 無 SOURce:PHASe:ON/OFF 命令：
//   - setPhaseOn  → TPHase（暫態起始相位角）
//   - setPhaseOff → 不支援，輸出 qWarning 並忽略
// ─────────────────────────────────────────────
void Chroma6530::setPhaseOn(double p)
{
    setTransientPhase(p);
}

void Chroma6530::setPhaseOff(double p)
{
    Q_UNUSED(p)
    qWarning() << "[Chroma6530] setPhaseOff() 不支援：6530 無波形結束相位角命令，已忽略。"
               << "如需相位反向請使用 setPhaseReverse(true)。";
}

// ─────────────────────────────────────────────
//  電流限制
//  Manual 6.6.2: [SOURce:]CURRent[:LEVel][:IMMediate][:AMPLitude]
//  Valid range: 0.00 ~ 100.00 A
// ─────────────────────────────────────────────
void Chroma6530::setCurrentLimit(double a)
{
    sendCommandWithLog(
        QString("CURRent %1").arg(a, 0, 'f', 2),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  ORELay ON | OFF  （硬體輸出繼電器）
//  Manual 6.6.2: 注意命令為 ORELay，非 OUTPut:RELay
// ─────────────────────────────────────────────
void Chroma6530::setOutputRelay(bool on)
{
    sendCommandWithLog(
        QString("ORELay %1").arg(on ? "ON" : "OFF"),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  OUTPut:PROTection 子系統
// ─────────────────────────────────────────────

// OUTPut:PROTection:CLEar
void Chroma6530::clearProtection()
{
    sendCommandWithLog("OUTPut:PROTection:CLEar", "[Chroma6530]");
}

// OUTPut:PROTection:DELay <NR2>  (0.0 ~ 100.0，單位 0.1 s)
void Chroma6530::setProtectionDelay(double s)
{
    sendCommandWithLog(
        QString("OUTPut:PROTection:DELay %1").arg(s, 0, 'f', 1),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  相位控制
// ─────────────────────────────────────────────

// PHASe[:IMMediate]:REVerse ON|OFF  （將輸出起始相位移動 180°）
void Chroma6530::setPhaseReverse(bool on)
{
    sendCommandWithLog(
        QString("PHASe:REVerse %1").arg(on ? "ON" : "OFF"),
        "[Chroma6530]");
}

// TPHase <NR2>  (0.0 ~ 359.99 deg)
void Chroma6530::setTransientPhase(double deg)
{
    sendCommandWithLog(
        QString("TPHase %1").arg(deg, 0, 'f', 2),
        "[Chroma6530]");
}

// TPHase:SYNC PHAS | IMM
//  usePhase=true  → PHAS（依相位角同步暫態輸出）
//  usePhase=false → IMM （立即輸出，相位隨機）
void Chroma6530::setTransientPhaseSync(bool usePhase)
{
    sendCommandWithLog(
        QString("TPHase:SYNC %1").arg(usePhase ? "PHAS" : "IMM"),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  波形緩衝區控制
//  Manual 6.6.2: [SOURce:]FUNCtion:SHAPe A|B
// ─────────────────────────────────────────────
void Chroma6530::setActiveWaveBuffer(WaveBuffer buf)
{
    sendCommandWithLog(
        QString("FUNCtion:SHAPe %1").arg(buf == WaveBuffer::A ? "A" : "B"),
        "[Chroma6530]");
}

// [SOURce:]FUNCtion:SHAPe:A|B <shape>
// shape 可為: "SIN", "SQU", "CSIN<n>", "DST<1-30>", "US<1-6>"
void Chroma6530::setWaveShape(WaveBuffer buf, const QString& shape)
{
    const QString ch = (buf == WaveBuffer::A) ? "A" : "B";
    sendCommandWithLog(
        QString("FUNCtion:SHAPe:%1 %2").arg(ch, shape),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  MEASure 子系統
//  Manual 6.6.2: FETCh|MEASure[:SCALar]:...
// ─────────────────────────────────────────────

// MEASure[:SCALar]:VOLTage:AC?  → rms 輸出電壓
double Chroma6530::measureVoltage()
{
    double v = 0.0;
    queryDouble("MEASure:VOLTage:AC?", v);
    return v;
}

// MEASure[:SCALar]:CURRent:AC?  → rms 輸出電流
double Chroma6530::measureCurrent()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:AC?", i);
    return i;
}

// MEASure[:SCALar]:CURRent:AMPLitude:MAXimum?  → 峰值電流
double Chroma6530::measureCurrentPeak()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:AMPLitude:MAXimum?", i);
    return i;
}

// MEASure[:SCALar]:CURRent:CREStfactor?  → 電流峰值因數
double Chroma6530::measureCrestFactor()
{
    double cf = 0.0;
    queryDouble("MEASure:CURRent:CREStfactor?", cf);
    return cf;
}

// MEASure[:SCALar]:CURRent:INRush?  → 湧浪電流
double Chroma6530::measureInrushCurrent()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:INRush?", i);
    return i;
}

// MEASure[:SCALar]:FREQuency?  → 輸出頻率 (Hz)
double Chroma6530::freQuency()
{
    double f = 0.0;
    queryDouble("MEASure:FREQuency?", f);
    return f;
}

// MEASure[:SCALar]:POWer:AC[:REAL]?  → 真實功率 (W)
double Chroma6530::realPower()
{
    double p = 0.0;
    queryDouble("MEASure:POWer:AC?", p);
    return p;
}

// MEASure[:SCALar]:POWer:AC:APParent?  → 視在功率 (VA)
double Chroma6530::apparentPower()
{
    double s = 0.0;
    queryDouble("MEASure:POWer:AC:APParent?", s);
    return s;
}

// MEASure[:SCALar]:POWer:AC:REACtive?  → 虛功率 (VAR)
double Chroma6530::reactivePower()
{
    double q = 0.0;
    queryDouble("MEASure:POWer:AC:REACtive?", q);
    return q;
}

// MEASure[:SCALar]:POWer:AC:PFACtor?  → 功率因數
double Chroma6530::powerPfactor()
{
    double pf = 0.0;
    queryDouble("MEASure:POWer:AC:PFACtor?", pf);
    return pf;
}

// ─────────────────────────────────────────────
//  觸發系統
//  Manual 6.6.2: INITiate / TRIGger
// ─────────────────────────────────────────────

// INITiate[:IMMediate]  — 觸發系統由閒置轉為待觸發
void Chroma6530::initiateTrigger()
{
    sendCommandWithLog("INITiate", "[Chroma6530]");
}

// INITiate:CONTinuous:SEQuence1 ON|OFF
void Chroma6530::setContinuousTrigger(bool on)
{
    sendCommandWithLog(
        QString("INITiate:CONTinuous:SEQuence1 %1").arg(on ? "ON" : "OFF"),
        "[Chroma6530]");
}

// TRIGger[:SEQuence1|:TRANsient][:IMMediate]
void Chroma6530::sendTrigger()
{
    sendCommandWithLog("TRIGger", "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  STEP 模態
//  Manual 6.6.2: [SOURce:]STEP:*
// ─────────────────────────────────────────────
void Chroma6530::setStepVoltage(double v)
{
    sendCommandWithLog(
        QString("STEP:VOLTage %1").arg(v, 0, 'f', 1),
        "[Chroma6530]");
}

void Chroma6530::setStepFrequency(double f)
{
    sendCommandWithLog(
        QString("STEP:FREQuency %1").arg(f, 0, 'f', 2),
        "[Chroma6530]");
}

void Chroma6530::setStepStartPhase(double deg)
{
    sendCommandWithLog(
        QString("STEP:SPHase %1").arg(deg, 0, 'f', 2),
        "[Chroma6530]");
}

void Chroma6530::setStepWaveShape(WaveBuffer buf)
{
    sendCommandWithLog(
        QString("STEP:SHAPe %1").arg(buf == WaveBuffer::A ? "A" : "B"),
        "[Chroma6530]");
}

// dV 範圍: -300.0 ~ +300.0 V
void Chroma6530::setStepDeltaVoltage(double dv)
{
    sendCommandWithLog(
        QString("STEP:DVOLtage %1").arg(dv, 0, 'f', 1),
        "[Chroma6530]");
}

// dF 範圍: -2000.0 ~ +2000.0 Hz
void Chroma6530::setStepDeltaFrequency(double df)
{
    sendCommandWithLog(
        QString("STEP:DFRequency %1").arg(df, 0, 'f', 2),
        "[Chroma6530]");
}

// dwell 範圍: 0.000 ~ 999.999 s
void Chroma6530::setStepDwell(double sec)
{
    sendCommandWithLog(
        QString("STEP:DWELl %1").arg(sec, 0, 'f', 3),
        "[Chroma6530]");
}

// count 範圍: 0 ~ 30000
void Chroma6530::setStepCount(int n)
{
    sendCommandWithLog(
        QString("STEP:COUNt %1").arg(n),
        "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  PULSE 模態
//  Manual 6.6.2: [SOURce:]PULSe:*
// ─────────────────────────────────────────────
void Chroma6530::setPulseVoltage(double v)
{
    sendCommandWithLog(
        QString("PULSe:VOLTage %1").arg(v, 0, 'f', 1),
        "[Chroma6530]");
}

void Chroma6530::setPulseFrequency(double f)
{
    sendCommandWithLog(
        QString("PULSe:FREQuency %1").arg(f, 0, 'f', 2),
        "[Chroma6530]");
}

// deg 範圍: 0.00 ~ 359.99
void Chroma6530::setPulseStartPhase(double deg)
{
    sendCommandWithLog(
        QString("PULSe:SPHase %1").arg(deg, 0, 'f', 2),
        "[Chroma6530]");
}

void Chroma6530::setPulseWaveShape(WaveBuffer buf)
{
    sendCommandWithLog(
        QString("PULSe:SHAPe %1").arg(buf == WaveBuffer::A ? "A" : "B"),
        "[Chroma6530]");
}

// n 範圍: 1 ~ 60000
void Chroma6530::setPulseCount(int n)
{
    sendCommandWithLog(
        QString("PULSe:COUNt %1").arg(n),
        "[Chroma6530]");
}

// pct 範圍: 0.01 ~ 100.00 %
void Chroma6530::setPulseDutyCycle(double pct)
{
    sendCommandWithLog(
        QString("PULSe:DCYCle %1").arg(pct, 0, 'f', 2),
        "[Chroma6530]");
}

// ms 範圍: 1.0 ~ 999999.0 ms
void Chroma6530::setPulsePeriod(double ms)
{
    sendCommandWithLog(
        QString("PULSe:PERiod %1").arg(ms, 0, 'f', 1),
        "[Chroma6530]");
}

void Chroma6530::stopPulse()
{
    sendCommandWithLog("PULSe:QUIT", "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  LIST 模態基本參數
//  Manual 6.6.2: [SOURce:]LIST:*
// ─────────────────────────────────────────────

// n 範圍: 1 ~ 60000 (n<=0 送出 INFinity)
void Chroma6530::setListCount(int n)
{
    const QString param = (n <= 0) ? "INFinity" : QString::number(n);
    sendCommandWithLog(
        QString("LIST:COUNt %1").arg(param),
        "[Chroma6530]");
}

// useCycle=true → CYCle 基礎；false → TIME 基礎
void Chroma6530::setListBase(bool useCycle)
{
    sendCommandWithLog(
        QString("LIST:BASE %1").arg(useCycle ? "CYCle" : "TIME"),
        "[Chroma6530]");
}

void Chroma6530::stopList()
{
    sendCommandWithLog("LIST:QUIT", "[Chroma6530]");
}

// ─────────────────────────────────────────────
//  系統命令
//  Manual 6.6.2: SYSTem (僅限 RS-232C)
// ─────────────────────────────────────────────
void Chroma6530::setRemoteMode()
{
    sendCommandWithLog("SYSTem:REMote", "[Chroma6530]");
}

void Chroma6530::setLocalMode()
{
    sendCommandWithLog("SYSTem:LOCal", "[Chroma6530]");
}
