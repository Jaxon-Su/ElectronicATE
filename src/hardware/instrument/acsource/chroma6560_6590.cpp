#include "chroma6560_6590.h"
#include <exception>
#include <stdexcept>

Chroma6560_6590::Chroma6560_6590(const QString &model, ICommunication *comm)
    : ChromaScpiACSource(model, ACSourceFamily::Chroma6560_6590, comm) {}

bool Chroma6560_6590::prepareOutput(bool threePhase, double phaseVoltage)
{
    m_running = m_prepared = false;
    if (threePhase && model() != "6590") return false;
    requireRange(phaseVoltage, 0, 300, "6560/6590 standard output supports 0–300 V per phase");
    // Manual §6.6.2: NPHase switches physical relays. Do not infer live topology on a fresh connection.
    if (outputEnabled()) throw std::runtime_error("Turn output off before configuring 6560/6590 phase mode or range");
    checkInstrumentError();
    if (model() == "6560") {
        command("NPHase PARALLEL"); // 1phase = parallel L–N wiring (fig. 2-6); never infer SERIES.
    } else {
        command(threePhase ? "NPHase THREE" : "NPHase SINGLE");
    }
    checkInstrumentError(); // No documented NPHase? query; do not proceed past a rejected relay switch.
    if (threePhase) {
        command("INSTrument:COUPle ALL"); // One voltage setting for all phases.
        command("INSTrument:NSELect 1");  // Scalar readbacks refer to phase 1.
        if (textReply("INSTrument:COUPle?").compare("ALL", Qt::CaseInsensitive) != 0 ||
            scalar("INSTrument:NSELect?") != 1)
            throw std::runtime_error("6590 did not confirm coupled voltage programming and phase 1 readback");
    }
    command("RANGe HIGH"); // No AUTO range on these two models.
    checkInstrumentError();
    if (textReply("RANGe?").compare("HIGH", Qt::CaseInsensitive) != 0)
        throw std::runtime_error("6560/6590 did not confirm the requested HIGH voltage range");
    m_prepared = true;
    return true;
}

void Chroma6560_6590::setVoltageWhileOn(double volts)
{
    requireRange(volts, 0, 300, "Live AC voltage must be 0–300 V per phase");
    if (!m_prepared || !m_running || !outputEnabled()) throw std::runtime_error("Live voltage change requires an enabled 6560/6590 output");
    command(QString("V %1").arg(volts, 0, 'f', 1)); // Immediate update, not a new OUTPut ON.
    checkInstrumentError();
}

void Chroma6560_6590::setFrequency(double hz)
{
    requireRange(hz, 45, 1000, "6560/6590 frequency range is 45–1000 Hz");
    command(QString("FREQuency %1").arg(hz, 0, 'f', 2));
}

void Chroma6560_6590::setPhaseOn(double degrees)
{
    requireRange(degrees, 0, 360, "Invalid 6560/6590 phase angle");
    const int hundredths = qRound(degrees * 100) % 36000;
    command(QString("TPHase %1").arg(hundredths / 100.0, 0, 'f', 2));
    command("TPHase:SYNC PHAS");
    checkInstrumentError();
}

void Chroma6560_6590::setPowerOn()
{
    if (!m_prepared) throw std::runtime_error("Configure 6560/6590 output mode before enabling it");
    checkInstrumentError();
    try {
        command("ORELay ON");
        checkInstrumentError();
        command("OUTPut ON");
        if (!outputEnabled()) throw std::runtime_error("6560/6590 output did not turn on");
        m_running = true;
    } catch (...) {
        setPowerOff(); // Also open the relay if the enable or readback failed.
        throw;
    }
}

void Chroma6560_6590::setPowerOff()
{
    m_running = m_prepared = false;
    std::exception_ptr failure;
    try { command("OUTPut OFF"); } catch (...) { failure = std::current_exception(); }
    try { command("ORELay OFF"); } catch (...) { if (!failure) failure = std::current_exception(); }
    requireOutputOff("OUTPut?");
    if (failure) std::rethrow_exception(failure);
    checkInstrumentError();
}

double Chroma6560_6590::apparentPower() { return scalar("MEASure:POWer:AC:APParent?"); }
double Chroma6560_6590::reactivePower() { return scalar("MEASure:POWer:AC:REACtive?"); }
