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

bool MSOSeries456::setEdgeTriggerChannel(int channel, QString &error)
{
    if (channel < 1 || channel > getTotalChannel())
        return unsupportedSetting("setEdgeTriggerChannel: invalid channel", error);
    // 4/5/6 MSO programmer manual 077130526, p.1758.
    return writeConfigurationCommand(QString("TRIGger:A:EDGE:SOUrce CH%1").arg(channel), error);
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

bool MSOSeries456::setChannelDisplay(int channel, bool enabled, QString &error)
{
    if (channel < 1 || channel > getTotalChannel())
        return unsupportedSetting("setChannelDisplay: invalid channel", error);
    // 4/5/6 MSO programmer manual 077130526, p.528: display mode only.
    return writeConfigurationCommand(QString("DISplay:GLObal:CH%1:STATE %2")
                                         .arg(channel).arg(enabled ? "ON" : "OFF"), error);
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

bool MSOSeries456::readVerticalRange(int channel, ScopeVerticalRange &range, QString &error)
{
    range = {};
    error.clear();
    if (channel < 1 || channel > getTotalChannel())
        return unsupportedSetting("readVerticalRange: invalid channel", error);
    auto read = [&](const QString &command, double &value) {
        QString reply;
        if (!queryConfiguration(command, reply, error)) return false;
        bool ok = false;
        value = reply.simplified().section(' ', -1).toDouble(&ok);
        if (!ok || !std::isfinite(value) || std::abs(value) >= 1e20) {
            error = "Invalid vertical range reply: " + command + " " + reply;
            return false;
        }
        return true;
    };
    ScopeVerticalRange actual;
    if (!read(QString("CH%1:SCAle?").arg(channel), actual.scale) || actual.scale <= 0 ||
        !read(QString("CH%1:OFFSet?").arg(channel), actual.center)) {
        if (error.isEmpty()) error = "Invalid vertical scale";
        return false;
    }
    // 4/5/6 MSO Help: ten vertical acquisition divisions. Position is display-only.
    // Invert reverses both waveform samples and the reported Offset; do not negate twice.
    actual.divisions = 10;
    range = actual;
    return true;
}
