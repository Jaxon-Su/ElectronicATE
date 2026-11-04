#include "chroma6415_6420_6430.h"
#include <stdexcept>

Chroma6415_6420_6430::Chroma6415_6420_6430(const QString &model, ICommunication *comm)
    : ChromaScpiACSource(model, ACSourceFamily::Chroma6415_6420_6430, comm) {}

bool Chroma6415_6420_6430::prepareOutput(bool threePhase, double volts)
{
    if (threePhase) return false;
    requireRange(volts, 0, 300, "6415/6420/6430 support 0–300 V single-phase");
    const QString external = textReply("VOLTage:EPRogram?");
    if (external != "0" && external.compare("OFF", Qt::CaseInsensitive) != 0)
        throw std::runtime_error("Disable external voltage programming before using the AC condition");
    // Use a fixed range for Brown Out; do not switch the range of a running output.
    if (!outputEnabled()) command("VOLTage:RANGe 300");
    else if (volts > scalar("VOLTage:RANGe?")) throw std::runtime_error("Turn output off before changing voltage range");
    return true;
}

void Chroma6415_6420_6430::setFrequency(double hz)
{
    requireRange(hz, 45, 500, "6415/6420/6430 remote FREQuency range is 45–500 Hz");
    command(QString("FREQuency %1").arg(hz, 0, 'f', 1));
    checkInstrumentError();
}

void Chroma6415_6420_6430::setPhaseOn(double degrees)
{
    // Zero denotes no phase request; the remote command tree has no start-angle command.
    if (degrees != 0) throw std::invalid_argument("Programmable start angle unavailable on 6415/6420/6430");
}

void Chroma6415_6420_6430::setPowerOn()
{
    checkInstrumentError();
    try {
        command("OUTPut ON");
        if (!outputEnabled()) throw std::runtime_error("6415/6420/6430 output did not turn on");
    } catch (...) {
        setPowerOff();
        throw;
    }
}

void Chroma6415_6420_6430::setPowerOff() { command("OUTPut OFF"); requireOutputOff("OUTPut?"); }
double Chroma6415_6420_6430::apparentPower() { return measureVoltage() * measureCurrent(); }
double Chroma6415_6420_6430::reactivePower()
{ throw std::runtime_error("6415/6420/6430 do not report reactive power"); }
