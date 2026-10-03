#include "chroma62000.h"
#include <QMutexLocker>
#include <cmath>
#include <stdexcept>

Chroma62000::Chroma62000(const QString& subModel, ICommunication* comm) : DCSource(comm)
{
    const auto selected = Chroma62000Spec::forModel(subModel);
    if (!selected)
        throw std::invalid_argument(QString("Unsupported Chroma 62000H model: %1").arg(subModel).toStdString());
    m_spec = *selected;
}
void Chroma62000::fail(const QString& message)
{
    m_lastError = model() + ": " + message;
    throw std::runtime_error(m_lastError.toStdString());
}
void Chroma62000::validate(double value, double maximum, const QString& quantity)
{
    if (!std::isfinite(value) || value < 0 || value > maximum)
        fail(QString("%1 must be finite and within 0..%2").arg(quantity).arg(maximum));
}
void Chroma62000::send(const QString& command)
{
    QMutexLocker lock(&m_commMutex);
    const auto bytes = (command + '\n').toUtf8();
    if (write(bytes) != bytes.size()) fail("Incomplete command write: " + command);
    m_lastError.clear();
}
double Chroma62000::measure(const QString& command)
{
    QMutexLocker lock(&m_commMutex);
    const auto bytes = (command + '\n').toUtf8();
    if (write(bytes) != bytes.size()) fail("Incomplete query write: " + command);
    QByteArray response;
    if (read(response, 256) <= 0) fail("Measurement read failed: " + command);
    bool ok = false;
    const double value = QString::fromLatin1(response).trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(value)) fail("Invalid measurement response: " + command);
    m_lastError.clear();
    return value;
}
// Manual 5.6.2.3: SOURCE subsystem. V * current limit is not requested power.
void Chroma62000::setVoltage(double voltage)
{
    validate(voltage, m_spec.maxVoltage, "Voltage (V)");
    send("SOUR:VOLT " + QString::number(voltage, 'g', 15));
}
void Chroma62000::setCurrent(double current)
{
    validate(current, m_spec.maxCurrent, "Current (A)");
    send("SOUR:CURR " + QString::number(current, 'g', 15));
}
// Manual 5.6.2.2: CONFIGURE subsystem.
void Chroma62000::setPowerOn() { send("CONF:OUTP ON"); }
void Chroma62000::setPowerOff()
{
    send("CONF:OUTP OFF");
    try { requireOutputOff("CONF:OUTP?"); }
    catch (const std::exception& error) { fail(QString::fromUtf8(error.what())); }
}
// Manual 5.6.2.5: output measurements, not programmed setpoints.
double Chroma62000::measureVoltage() { return measure("MEAS:VOLT?"); }
double Chroma62000::measureCurrent() { return measure("MEAS:CURR?"); }
double Chroma62000::measurePower() { return measure("MEAS:POW?"); }
