#include "chroma61509.h"
#include <QDebug>
#include <algorithm>

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────
Chroma61509::Chroma61509(ICommunication* comm)
    : ACSource(comm) {}

Chroma61509::~Chroma61509()
{
    disconnect();
}

// ─────────────────────────────────────────────
//  InstrumentBase
// ─────────────────────────────────────────────
QString Chroma61509::model()  const { return "61509"; }
QString Chroma61509::vendor() const { return "Chroma"; }

// ─────────────────────────────────────────────
//  Output Control
//  Manual 9.5.2.4: OUTPut[:STATe] ON | OFF
// ─────────────────────────────────────────────
void Chroma61509::setPowerOn()
{
    sendCommandWithLog("OUTPut ON", "[Chroma61509]");
}

void Chroma61509::setPowerOff()
{
    sendCommandWithLog("OUTPut OFF", "[Chroma61509]");
}

// ─────────────────────────────────────────────
//  SOURce Subsystem
//  Manual 9.5.2.5
// ─────────────────────────────────────────────

// [SOURce:]VOLTage[:LEVel][:IMMediate][:AMPLitude]:AC <NR2>
// Valid range: 0.0~175.0 (LOW) / 0.0~350.0 (HIGH), unit: V
void Chroma61509::setVoltage(double v)
{
    // setVoltageRange(VoltageRange::Auto);
    sendCommandWithLog(
        QString("SOURce:VOLTage:AC %1").arg(v, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]VOLTage[:LEVel][:IMMediate][:AMPLitude]:DC <NR2>
// Valid range: -247.5~+247.5 V (LOW) / -494.9~+494.9 V (HIGH)
void Chroma61509::setVoltageDC(double v)
{
    sendCommandWithLog(
        QString("SOURce:VOLTage:DC %1").arg(v, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]VOLTage:RANGe LOW | HIGH | AUTO
void Chroma61509::setVoltageRange(VoltageRange range)
{
    QString param;
    switch (range) {
    case VoltageRange::Low:  param = "LOW";  break;
    case VoltageRange::High: param = "HIGH"; break;
    case VoltageRange::Auto: param = "AUTO"; break;
    }
    sendCommandWithLog(
        QString("SOURce:VOLTage:RANGe %1").arg(param),
        "[Chroma61509]");
}

// [SOURce:]VOLTage:LIMit:AC <NR2>
void Chroma61509::setVoltageLimitAC(double v)
{
    sendCommandWithLog(
        QString("SOURce:VOLTage:LIMit:AC %1").arg(v, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]VOLTage:LIMit:DC:PLUS <NR2>
void Chroma61509::setVoltageLimitDCPlus(double v)
{
    sendCommandWithLog(
        QString("SOURce:VOLTage:LIMit:DC:PLUS %1").arg(v, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]VOLTage:LIMit:DC:MINus <NR2>
void Chroma61509::setVoltageLimitDCMinus(double v)
{
    sendCommandWithLog(
        QString("SOURce:VOLTage:LIMit:DC:MINus %1").arg(v, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]FREQuency[:CW|:IMMediate] <NR2>
// Valid range: 15.00 ~ 2000.00 Hz
void Chroma61509::setFrequency(double f)
{
    const double clamped = std::clamp(f, kFreqMin, kFreqMax);
    if (clamped != f) {
        qWarning() << "[Chroma61509] Frequency clamped:"
                   << f << "->" << clamped;
    }
    sendCommandWithLog(
        QString("SOURce:FREQuency %1").arg(clamped, 0, 'f', 2),
        "[Chroma61509]");
}

// [SOURce:]PHASe:ON <NR2>  (0.0 ~ 359.9 deg)
void Chroma61509::setPhaseOn(double p)
{
    sendCommandWithLog(
        QString("SOURce:PHASe:ON %1").arg(p, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]PHASe:OFF <NR2>  (0.0 ~ 360.0 deg; 360.0 = IMMED)
void Chroma61509::setPhaseOff(double p)
{
    sendCommandWithLog(
        QString("SOURce:PHASe:OFF %1").arg(p, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]PHASe:P12 <NR2>  相位1與相位2之相位差 (0.0 ~ 359.9 deg)
void Chroma61509::setPhaseP12(double deg)
{
    sendCommandWithLog(
        QString("SOURce:PHASe:P12 %1").arg(deg, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]PHASe:P13 <NR2>  相位1與相位3之相位差 (0.0 ~ 359.9 deg)
void Chroma61509::setPhaseP13(double deg)
{
    sendCommandWithLog(
        QString("SOURce:PHASe:P13 %1").arg(deg, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]CURRent:LIMit <NR2>
// Valid range: 0.0 ~ 60 A (1-Phase LOW range)
void Chroma61509::setCurrentLimit(double a)
{
    sendCommandWithLog(
        QString("SOURce:CURRent:LIMit %1").arg(a, 0, 'f', 3),
        "[Chroma61509]");
}

// [SOURce:]CURRent:DELay <NR2>  (0.0 ~ 5.0 s, step 0.1 s)
void Chroma61509::setCurrentDelay(double s)
{
    sendCommandWithLog(
        QString("SOURce:CURRent:DELay %1").arg(s, 0, 'f', 1),
        "[Chroma61509]");
}

// [SOURce:]POWer:PROTection <NR2>  (0.0 ~ 6000 W for 61509)
void Chroma61509::setPowerProtection(double w)
{
    sendCommandWithLog(
        QString("SOURce:POWer:PROTection %1").arg(w, 0, 'f', 3),
        "[Chroma61509]");
}

// ─────────────────────────────────────────────
//  OUTPut Subsystem
//  Manual 9.5.2.4
// ─────────────────────────────────────────────

// OUTPut:RELay ON | OFF
void Chroma61509::setOutputRelay(bool on)
{
    sendCommandWithLog(
        QString("OUTPut:RELay %1").arg(on ? "ON" : "OFF"),
        "[Chroma61509]");
}

// OUTPut:COUPling AC | DC | ACDC
void Chroma61509::setOutputCoupling(OutputCoupling mode)
{
    QString param;
    switch (mode) {
    case OutputCoupling::AC:   param = "AC";   break;
    case OutputCoupling::DC:   param = "DC";   break;
    case OutputCoupling::ACDC: param = "ACDC"; break;
    }
    sendCommandWithLog(
        QString("OUTPut:COUPling %1").arg(param),
        "[Chroma61509]");
}

// OUTPut:MODE FIXED | LIST | PULSE | STEP | SYNTH | INTERHAR
void Chroma61509::setOutputMode(const QString& mode)
{
    sendCommandWithLog(
        QString("OUTPut:MODE %1").arg(mode),
        "[Chroma61509]");
}

// OUTPut:PROTection:CLEar
void Chroma61509::clearProtection()
{
    sendCommandWithLog("OUTPut:PROTection:CLEar", "[Chroma61509]");
}

// OUTPut:IMPedance:STATe ON | OFF
void Chroma61509::setImpedanceState(bool on)
{
    sendCommandWithLog(
        QString("OUTPut:IMPedance:STATe %1").arg(on ? "ON" : "OFF"),
        "[Chroma61509]");
}

// OUTPut:IMPedance:RESistor <NR2>  (0.00 ~ 1.00 Ω)
void Chroma61509::setImpedanceResistor(double ohm)
{
    sendCommandWithLog(
        QString("OUTPut:IMPedance:RESistor %1").arg(ohm, 0, 'f', 2),
        "[Chroma61509]");
}

// OUTPut:IMPedance:INDuction <NR2>  (0.00 ~ 2.00 mH)
void Chroma61509::setImpedanceInductor(double mH)
{
    sendCommandWithLog(
        QString("OUTPut:IMPedance:INDuction %1").arg(mH, 0, 'f', 2),
        "[Chroma61509]");
}

// OUTPut:SLEW:VOLTage:AC <NR2>  (0.000 ~ 1200.000 V/ms)
void Chroma61509::setSlewVoltageAC(double vPerMs)
{
    sendCommandWithLog(
        QString("OUTPut:SLEW:VOLTage:AC %1").arg(vPerMs, 0, 'f', 3),
        "[Chroma61509]");
}

// OUTPut:SLEW:VOLTage:DC <NR2>  (0.000 ~ 1200.000 V/ms)
void Chroma61509::setSlewVoltageDC(double vPerMs)
{
    sendCommandWithLog(
        QString("OUTPut:SLEW:VOLTage:DC %1").arg(vPerMs, 0, 'f', 3),
        "[Chroma61509]");
}

// OUTPut:SLEW:FREQuency <NR2>  (0.000 ~ 1600.000 Hz/ms)
void Chroma61509::setSlewFrequency(double hzPerMs)
{
    sendCommandWithLog(
        QString("OUTPut:SLEW:FREQuency %1").arg(hzPerMs, 0, 'f', 3),
        "[Chroma61509]");
}

// ─────────────────────────────────────────────
//  INSTrument Subsystem
//  Manual 9.5.2.2
// ─────────────────────────────────────────────

// INSTrument:NSELect 1 | 2 | 3  (選擇相位進行後續設定)
void Chroma61509::setPhaseSelect(int phase)
{
    if (phase < 1 || phase > 3) {
        qWarning() << "[Chroma61509] setPhaseSelect: invalid phase" << phase;
        return;
    }
    sendCommandWithLog(
        QString("INSTrument:NSELect %1").arg(phase),
        "[Chroma61509]");
}

// INSTrument:PHASe THREE | SINGLE
void Chroma61509::setInstrumentPhase(const QString& mode)
{
    sendCommandWithLog(
        QString("INSTrument:PHASe %1").arg(mode),
        "[Chroma61509]");
}

// ─────────────────────────────────────────────
//  MEASure Subsystem
//  Manual 9.5.2.3
// ─────────────────────────────────────────────

// MEASure[:SCALar]:VOLTage:AC?  → rms AC voltage
double Chroma61509::measureVoltage()
{
    return measureVoltageAC();
}

double Chroma61509::measureVoltageAC()
{
    double v = 0.0;
    queryDouble("MEASure:VOLTage:AC?", v);
    return v;
}

// MEASure[:SCALar]:VOLTage:DC?  → DC component
double Chroma61509::measureVoltageDC()
{
    double v = 0.0;
    queryDouble("MEASure:VOLTage:DC?", v);
    return v;
}

// MEASure[:SCALar]:VOLTage:ACDC?  → rms (AC+DC) voltage
double Chroma61509::measureVoltageACDC()
{
    double v = 0.0;
    queryDouble("MEASure:VOLTage:ACDC?", v);
    return v;
}

// MEASure[:SCALar]:CURRent:AC?  → rms AC current
double Chroma61509::measureCurrent()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:AC?", i);
    return i;
}

// MEASure[:SCALar]:CURRent:DC?  → DC current
double Chroma61509::measureCurrentDC()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:DC?", i);
    return i;
}

// MEASure[:SCALar]:CURRent:ACDC?  → rms (AC+DC) current
double Chroma61509::measureCurrentACDC()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:ACDC?", i);
    return i;
}

// MEASure[:SCALar]:CURRent:AMPLitude:MAXimum?  → peak current
double Chroma61509::measureCurrentPeak()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:AMPLitude:MAXimum?", i);
    return i;
}

// MEASure[:SCALar]:CURRent:CREStfactor?  → crest factor
double Chroma61509::measureCrestFactor()
{
    double cf = 0.0;
    queryDouble("MEASure:CURRent:CREStfactor?", cf);
    return cf;
}

// MEASure[:SCALar]:CURRent:INRush?  → inrush current
double Chroma61509::measureInrushCurrent()
{
    double i = 0.0;
    queryDouble("MEASure:CURRent:INRush?", i);
    return i;
}

// MEASure[:SCALar]:FREQuency?  → output frequency (Hz)
double Chroma61509::freQuency()
{
    double f = 0.0;
    queryDouble("MEASure:FREQuency?", f);
    return f;
}

// MEASure[:SCALar]:POWer:AC[:REAL]?  → true power (W)
double Chroma61509::realPower()
{
    double p = 0.0;
    queryDouble("MEASure:POWer:AC?", p);
    return p;
}

// MEASure[:SCALar]:POWer:AC:APParent?  → apparent power (VA)
double Chroma61509::apparentPower()
{
    double s = 0.0;
    queryDouble("MEASure:POWer:AC:APParent?", s);
    return s;
}

// MEASure[:SCALar]:POWer:AC:REACtive?  → reactive power (VAR)
double Chroma61509::reactivePower()
{
    double q = 0.0;
    queryDouble("MEASure:POWer:AC:REACtive?", q);
    return q;
}

// MEASure[:SCALar]:POWer:AC:PFACtor?  → power factor
double Chroma61509::powerPfactor()
{
    double pf = 0.0;
    queryDouble("MEASure:POWer:AC:PFACtor?", pf);
    return pf;
}

// MEASure[:SCALar]:POWer:AC:TOTal?  → total real power (3-phase, W)
double Chroma61509::measureTotalRealPower()
{
    double p = 0.0;
    queryDouble("MEASure:POWer:AC:TOTal?", p);
    return p;
}

// MEASure[:SCALar]:POWer:AC:TOTal:APParent?  → total apparent power (3-phase, VA)
double Chroma61509::measureTotalApparentPower()
{
    double s = 0.0;
    queryDouble("MEASure:POWer:AC:TOTal:APParent?", s);
    return s;
}

// MEASure[:SCALar]:LINE:V12?  → line voltage Φ1-Φ2
double Chroma61509::measureLineV12()
{
    double v = 0.0;
    queryDouble("MEASure:LINE:V12?", v);
    return v;
}

// MEASure[:SCALar]:LINE:V23?  → line voltage Φ2-Φ3
double Chroma61509::measureLineV23()
{
    double v = 0.0;
    queryDouble("MEASure:LINE:V23?", v);
    return v;
}

// MEASure[:SCALar]:LINE:V31?  → line voltage Φ3-Φ1
double Chroma61509::measureLineV31()
{
    double v = 0.0;
    queryDouble("MEASure:LINE:V31?", v);
    return v;
}
