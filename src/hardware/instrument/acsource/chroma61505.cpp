#include "chroma61505.h"
#include <QDebug>
#include <algorithm>

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────
Chroma61505::Chroma61505(ICommunication* comm)
    : ACSource(comm) {}

Chroma61505::~Chroma61505()
{
    disconnect();
}

// ─────────────────────────────────────────────
//  InstrumentBase
// ─────────────────────────────────────────────
QString Chroma61505::model()  const { return "61505"; }
QString Chroma61505::vendor() const { return "Chroma"; }

// ─────────────────────────────────────────────
//  Output Control
//  Manual 8.6.2.2: OUTPut[:STATe] ON | OFF
// ─────────────────────────────────────────────
void Chroma61505::setPowerOn()
{
    // OUTPut ON
    sendCommandWithLog("OUTPut ON", "[Chroma61505]");
}

void Chroma61505::setPowerOff()
{
    // OUTPut OFF
    sendCommandWithLog("OUTPut OFF", "[Chroma61505]");
}

// ─────────────────────────────────────────────
//  SOURce Subsystem
//  Manual 8.6.2.3
// ─────────────────────────────────────────────

// [SOURce:]VOLTage[:LEVel][:IMMediate][:AMPLitude]:AC <NR2>
// Valid range: 0.0~150.0 (LOW range) / 0.0~300.0 (HIGH range), unit: V
void Chroma61505::setVoltage(double v)
{
    setVoltageRange(VoltageRange::Auto);
    sendCommandWithLog(
        QString("SOURce:VOLTage:AC %1").arg(v, 0, 'f', 3),
        "[Chroma61505]");
}

// [SOURce:]VOLTage[:LEVel][:IMMediate][:AMPLitude]:DC <NR2>
// Valid range: -424.2 ~ 424.2 V (HIGH range)
void Chroma61505::setVoltageDC(double v)
{
    sendCommandWithLog(
        QString("SOURce:VOLTage:DC %1").arg(v, 0, 'f', 3),
        "[Chroma61505]");
}

// [SOURce:]VOLTage:RANGe LOW | HIGH | AUTO
void Chroma61505::setVoltageRange(VoltageRange range)
{
    QString param;
    switch (range) {
    case VoltageRange::Low:  param = "LOW";  break;
    case VoltageRange::High: param = "HIGH"; break;
    case VoltageRange::Auto: param = "AUTO"; break;
    }
    sendCommandWithLog(
        QString("SOURce:VOLTage:RANGe %1").arg(param),
        "[Chroma61505]");
}

// [SOURce:]FREQuency[:CW|:IMMediate] <NR2>
// Valid range: 15.00 ~ 1000.00 Hz
void Chroma61505::setFrequency(double f)
{
    // 依 spec 限制範圍，防止送出非法值
    const double clamped = std::clamp(f, kFreqMin, kFreqMax);
    if (clamped != f) {
        qWarning() << "[Chroma61505] Frequency clamped:"
                   << f << "->" << clamped;
    }
    sendCommandWithLog(
        QString("SOURce:FREQuency %1").arg(clamped, 0, 'f', 2),
        "[Chroma61505]");
}

// [SOURce:]PHASe:ON <NR2>
void Chroma61505::setPhaseOn(double p)
{
    sendCommandWithLog(
        QString("SOURce:PHASe:ON %1").arg(p, 0, 'f', 3),
        "[Chroma61505]");
}

// [SOURce:]PHASe:OFF <NR2>
void Chroma61505::setPhaseOff(double p)
{
    sendCommandWithLog(
        QString("SOURce:PHASe:OFF %1").arg(p, 0, 'f', 3),
        "[Chroma61505]");
}

// [SOURce:]CURRent:LIMit <NR2>
// Valid range: 0.00 ~ max current spec (32A @ LOW / 20A @ HIGH)
void Chroma61505::setCurrentLimit(double a)
{
    sendCommandWithLog(
        QString("SOURce:CURRent:LIMit %1").arg(a, 0, 'f', 3),
        "[Chroma61505]");
}

// ─────────────────────────────────────────────
//  OUTPut Relay
//  Manual 8.6.2.2: OUTPut:RELay ON | OFF
// ─────────────────────────────────────────────
void Chroma61505::setOutputRelay(bool on)
{
    sendCommandWithLog(
        QString("OUTPut:RELay %1").arg(on ? "ON" : "OFF"),
        "[Chroma61505]");
}

// ─────────────────────────────────────────────
//  MEASure Subsystem
//  Manual 8.6.2.1
//  注意：Chroma 61505 使用 ACDC (rms) 量測電壓，
//        不同於 Delta A3000 的 MEAS:VOLT:AC?
// ─────────────────────────────────────────────

// MEASure[:SCALar]:VOLTage:ACDC?  → rms output voltage
double Chroma61505::measureVoltage()
{
    double v = 0.0;
    queryDouble("MEASure:VOLTage:ACDC?", v);
    return v;
}

// MEASure[:SCALar]:VOLTage:DC?   → DC component of output voltage
double Chroma61505::measureVoltageDC()
{
    double v = 0.0;
    queryDouble("MEASure:VOLTage:DC?", v);
    return v;
}

// MEASure[:SCALar]:CURRent:AC?   → rms output current
double Chroma61505::measureCurrent()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:AC?", i);
    return i;
}

// MEASure[:SCALar]:CURRent:AMPLitude:MAXimum?  → peak current (absolute)
double Chroma61505::measureCurrentPeak()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:AMPLitude:MAXimum?", i);
    return i;
}

// MEASure[:SCALar]:CURRent:CREStfactor?  → peak / rms ratio
double Chroma61505::measureCrestFactor()
{
    double cf = 0.0;
    queryDouble("MEASure:CURRent:CREStfactor?", cf);
    return cf;
}

// MEASure[:SCALar]:CURRent:INRush?  → inrush current
double Chroma61505::measureInrushCurrent()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:INRush?", i);
    return i;
}

// MEASure[:SCALar]:FREQuency?   → output frequency (Hz)
double Chroma61505::freQuency()
{
    double f = 0.0;
    queryDouble("MEASure:FREQuency?", f);
    return f;
}

// MEASure[:SCALar]:POWer:AC[:REAL]?  → true power (W)
double Chroma61505::realPower()
{
    double p = 0.0;
    queryDouble("MEASure:POWer:AC?", p);
    return p;
}

// MEASure[:SCALar]:POWer:AC:APParent?  → apparent power (VA)
double Chroma61505::apparentPower()
{
    double s = 0.0;
    queryDouble("MEASure:POWer:AC:APParent?", s);
    return s;
}

// MEASure[:SCALar]:POWer:AC:REACtive?  → reactive power (VAR)
double Chroma61505::reactivePower()
{
    double q = 0.0;
    queryDouble("MEASure:POWer:AC:REACtive?", q);
    return q;
}

// MEASure[:SCALar]:POWer:AC:PFACtor?  → power factor
double Chroma61505::powerPfactor()
{
    double pf = 0.0;
    queryDouble("MEASure:POWer:AC:PFACtor?", pf);
    return pf;
}
