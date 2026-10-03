#include "msoseries456.h"
#include <QRegularExpression>
#include <cmath>

ScopeAcquisitionState MSOSeries456::acquisitionState()
{
    const auto state = getAcquisitionState().simplified().toUpper().section(' ', -1);
    if (state == "0" || state == "STOP" || state == "OFF")
        return ScopeAcquisitionState::Completed;
    if (state == "1" || state == "RUN" || state == "ON")
        return ScopeAcquisitionState::Running;
    if (m_lastError.isEmpty())
        m_lastError = "Invalid acquisition state response: " + state;
    return ScopeAcquisitionState::Invalid;
}

ScopeTriggerState MSOSeries456::triggerState()
{
    const auto state = getTriggerState().simplified().toUpper().section(' ', -1);
    if (state == "ARMED")
        return ScopeTriggerState::Arming;
    if (state == "READY")
        return ScopeTriggerState::Ready;
    if (state == "TRIGGER")
        return ScopeTriggerState::Triggered;
    if (state == "AUTO")
        return ScopeTriggerState::Automatic;
    if (state == "SAVE")
        return ScopeTriggerState::Saving;
    if (m_lastError.isEmpty())
        m_lastError = "Invalid trigger state response: " + state;
    return ScopeTriggerState::Invalid;
}

int MSOSeries456::triggerChannel()
{
    const auto source = getTriggerSource().trimmed();
    const auto match =
        QRegularExpression("(?:^|[ :])CH(\\d+)$", QRegularExpression::CaseInsensitiveOption).match(source);
    const int channel = match.hasMatch() ? match.captured(1).toInt() : 0;
    if (channel >= 1 && channel <= getTotalChannel())
        return channel;
    if (m_lastError.isEmpty())
        m_lastError = "Trigger source is not an analog channel: " + source;
    return 0;
}

double MSOSeries456::readMeasurement(int channel, ScopeMeasurement measurement)
{
    if (channel < 1 || channel > getTotalChannel())
        return unavailable("readMeasurement: invalid channel", std::numeric_limits<double>::quiet_NaN());
    switch (measurement) {
    case ScopeMeasurement::Maximum:
        return measureSignalPeak(channel, "MAXimum");
    case ScopeMeasurement::Minimum:
        return measureSignalPeak(channel, "MINimum");
    case ScopeMeasurement::Rms:
        return measureSignalPeak(channel, "RMS");
    case ScopeMeasurement::Mean:
        return measureSignalPeak(channel, "MEAN");
    }
    return unavailable("readMeasurement: invalid measurement", std::numeric_limits<double>::quiet_NaN());
}

bool MSOSeries456::stopAcquisition(QString &error)
{
    return writeConfigurationCommand("ACQuire:STATE STOP", error);
}

bool MSOSeries456::selectEdgeTrigger(QString &error)
{
    return writeConfigurationCommand("TRIGger:A:TYPe EDGE", error);
}

bool MSOSeries456::setEdgeSlope(ScopeTriggerSlope slope, QString &error)
{
    switch (slope) {
    case ScopeTriggerSlope::Rise:
        return writeConfigurationCommand("TRIGger:A:EDGE:SLOpe RISE", error);
    case ScopeTriggerSlope::Fall:
        return writeConfigurationCommand("TRIGger:A:EDGE:SLOpe FALL", error);
    }
    return unsupportedSetting("setEdgeSlope: invalid slope", error);
}

bool MSOSeries456::setTriggerMode(ScopeTriggerMode mode, QString &error)
{
    switch (mode) {
    case ScopeTriggerMode::Auto:
        return writeConfigurationCommand("TRIGger:A:MODe AUTO", error);
    case ScopeTriggerMode::Normal:
        return writeConfigurationCommand("TRIGger:A:MODe NORMAL", error);
    }
    return unsupportedSetting("setTriggerMode: invalid mode", error);
}

bool MSOSeries456::setChannelTriggerLevel(int channel, double level, QString &error)
{
    if (channel < 1 || channel > getTotalChannel() || !std::isfinite(level))
        return unsupportedSetting("setChannelTriggerLevel: invalid channel or level", error);
    return writeConfigurationCommand(QString("TRIGger:A:LEVel:CH%1 %2").arg(channel).arg(level, 0, 'g', 16),
                                     error);
}

bool MSOSeries456::setVerticalScale(int channel, double scale, QString &error)
{
    if (channel < 1 || channel > getTotalChannel() || !std::isfinite(scale) || scale <= 0)
        return unsupportedSetting("setVerticalScale: invalid channel or scale", error);
    return writeConfigurationCommand(QString("CH%1:SCAle %2").arg(channel).arg(scale, 0, 'g', 16), error);
}
