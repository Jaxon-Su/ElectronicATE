#include "chromascpiacsource.h"
#include <cmath>
#include <stdexcept>

ChromaScpiACSource::ChromaScpiACSource(const QString &model, ACSourceFamily family, ICommunication *comm)
    : ACSource(comm), m_model(model)
{
    if (acSourceFamily(model) != family) throw std::invalid_argument(("Unsupported AC model: " + model).toStdString());
}

void ChromaScpiACSource::requireRange(double value, double low, double high, const char *setting)
{
    if (!std::isfinite(value) || value < low || value > high) throw std::invalid_argument(setting);
}

// LF is a valid program-message terminator for both GPIB and RS-232.
void ChromaScpiACSource::command(const QString &value) { sendCommandWithLog(value + '\n', "[Chroma" + model() + "]"); }

QString ChromaScpiACSource::textReply(const QString &query)
{
    QString reply;
    if (!queryString(query + '\n', reply)) throw std::runtime_error(lastError().toStdString());
    if (reply.isEmpty() || reply.contains('\r') || reply.contains('\n') || reply.contains(QChar::Null))
        throw std::runtime_error(("Invalid SCPI text reply to " + query).toStdString());
    return reply;
}

double ChromaScpiACSource::scalar(const QString &query)
{
    double result;
    QString error;
    if (!queryScalarDouble(query + '\n', result, error)) throw std::runtime_error(error.toStdString());
    return result;
}

void ChromaScpiACSource::checkInstrumentError()
{
    // 6400 returns a numeric code and message; 6560/6590 documents a textual error string.
    const QString reply = textReply("SYSTem:ERRor?");
    bool numeric = false;
    const int code = reply.section(',', 0, 0).trimmed().toInt(&numeric);
    if ((numeric && code == 0) || reply.trimmed().compare("No error", Qt::CaseInsensitive) == 0) return;
    throw std::runtime_error(QString("Chroma %1 SYSTem:ERRor?: %2").arg(model(), reply).toStdString());
}

void ChromaScpiACSource::setVoltage(double volts)
{
    requireRange(volts, 0, 300, "AC programmed voltage must be 0–300 V RMS");
    command(QString("VOLTage %1").arg(volts, 0, 'f', 1));
    // A configured voltage limit or range can cause the instrument to accept a lower value.
    if (std::abs(scalar("VOLTage?") - volts) > 0.051)
        throw std::runtime_error("AC source did not accept the requested voltage");
}

bool ChromaScpiACSource::readVoltageSetting(double &volts, QString &error)
{ return queryScalarDouble("VOLTage?\n", volts, error); }

void ChromaScpiACSource::setPhaseOff(double)
{ throw std::invalid_argument(("No programmable output-off phase on Chroma " + model()).toStdString()); }

double ChromaScpiACSource::measureVoltage() { return scalar("MEASure:VOLTage:AC?"); }
double ChromaScpiACSource::measureCurrent() { return scalar("MEASure:CURRent:AC?"); }
double ChromaScpiACSource::realPower() { return scalar("MEASure:POWer:AC?"); }
double ChromaScpiACSource::powerPfactor() { return scalar("MEASure:POWer:AC:PFACtor?"); }
double ChromaScpiACSource::freQuency() { return scalar("MEASure:FREQuency?"); }
