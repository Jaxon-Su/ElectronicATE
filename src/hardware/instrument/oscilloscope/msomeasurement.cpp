#include "msoseries456.h"
#include "oscilloscopesettingrules.h"
#include <QRegularExpression>
#include <cmath>

bool MSOSeries456::beginAutoReference(int &timeoutMs, QString &error)
{
    m_referenceActive = m_referenceStopping = false;
    m_referenceFrozenAfter = 0;
    m_referenceRequired = 1;
    timeoutMs = 0;
    error.clear();
    auto token = [&](const QString &command, QString &value) {
        if (!queryConfiguration(command, value, error)) return false;
        value = value.simplified().section(' ', -1).toUpper();
        return true;
    };
    auto number = [&](const QString &command, double &value) {
        QString reply;
        if (!token(command,reply)) return false;
        bool ok = false; value = reply.toDouble(&ok);
        if (ok && std::isfinite(value) && value > 0 && value < 1e20) return true;
        error = "Invalid AUTO reference setting: " + command + " " + reply;
        return false;
    };
    double scale = 0, divisions = 0;
    QString frames, mode;
    if (!number("HORizontal:SCAle?",scale) || !number("HORizontal:DIVisions?",divisions) ||
        !token("HORizontal:FASTframe:STATE?",frames) || !token("ACQuire:MODe?",mode)) return false;
    if (frames != "0" && frames != "OFF") {
        error = "AUTO reference requires FastFrame disabled"; return false;
    }
    if (mode == "AVERAGE") {
        double count = 0;
        if (!number("ACQuire:NUMAVg?",count)) return false;
        if (count < 2 || count > 10240 || std::floor(count) != count) {
            error = "Invalid AUTO reference average count"; return false;
        }
        m_referenceRequired = quint64(count);
    } else if (mode != "SAMPLE" && mode != "PEAKDETECT" && mode != "HIRES" && mode != "ENVELOPE") {
        error = "Invalid AUTO reference acquisition mode: " + mode; return false;
    }
    const double required = std::max(10000.0, 2 * scale * divisions * 1000 * m_referenceRequired + 5000);
    if (!std::isfinite(required) || required > 60000) {
        error = "AUTO reference window exceeds 60 s budget; reduce time/div or average count"; return false;
    }
    timeoutMs = int(std::ceil(required));
    // RUN resets NUMACQ (077130526 pp.245-248). No Single/edge is required for DC.
    for (const auto &command : {"ACQuire:STATE STOP", "TRIGger:A:MODe AUTO",
                                "ACQuire:STOPAfter RUNSTop", "ACQuire:STATE RUN"})
        if (!writeConfigurationCommand(command,error)) return false;
    QString actualMode, stopAfter;
    if (!token("TRIGger:A:MODe?",actualMode) || !token("ACQuire:STOPAfter?",stopAfter)) return false;
    if (actualMode != "AUTO" || stopAfter != "RUNSTOP") {
        error = "AUTO reference readback mismatch: mode=" + actualMode + "; stop-after=" + stopAfter;
        m_lastError = error; return false;
    }
    m_referenceActive = true;
    return true;
}

ScopeReferenceStatus MSOSeries456::pollAutoReference(QString &state, QString &error)
{
    error.clear();
    auto fail = [&](const QString &message) {
        error = message; m_lastError = error; return ScopeReferenceStatus::Error;
    };
    if (!m_referenceActive) return fail("AUTO reference was not started");
    QString mode, stopAfter, counter;
    if (!queryConfiguration("TRIGger:A:MODe?",mode,error) ||
        !queryConfiguration("ACQuire:STOPAfter?",stopAfter,error)) return ScopeReferenceStatus::Error;
    mode = mode.simplified().section(' ', -1).toUpper();
    stopAfter = stopAfter.simplified().section(' ', -1).toUpper();
    auto acquisition = acquisitionState();
    if (acquisition == ScopeAcquisitionState::Invalid) return fail(m_lastError);
    const auto trigger = triggerState();
    if (trigger == ScopeTriggerState::Invalid) return fail(m_lastError);
    auto readCount = [&](quint64 &count) {
        if (!queryConfiguration("ACQuire:NUMACq?",counter,error)) return false;
        const auto value = counter.simplified().section(' ', -1);
        bool ok = false; count = value.toULongLong(&ok);
        if (!ok || !QRegularExpression("^[0-9]+$").match(value).hasMatch()) {
            error = "Invalid AUTO reference acquisition count: " + counter; return false;
        }
        return true;
    };
    quint64 count = 0;
    if (!readCount(count)) return fail(error);
    const QString triggerName = QStringList{"INVALID","ARMED","READY","TRIGGER","AUTO","SAVE"}.value(int(trigger));
    auto describe = [&] {
        state = QString("mode=%1 stop-after=%2 acquisition=%3 trigger=%4 count=%5/%6")
            .arg(mode,stopAfter,acquisition == ScopeAcquisitionState::Completed ? "STOP" : "RUN",triggerName)
            .arg(count).arg(m_referenceRequired);
    };
    describe();
    if (mode != "AUTO" || stopAfter != "RUNSTOP") return fail("AUTO reference readback mismatch: " + state);
    if (count < m_referenceRequired) {
        if (acquisition == ScopeAcquisitionState::Completed || m_referenceStopping)
            return fail("AUTO reference stopped without a new complete record: " + state);
        return ScopeReferenceStatus::Pending;
    }
    if (!m_referenceStopping) {
        m_referenceFrozenAfter = count;
        if (!writeConfigurationCommand("ACQuire:STATE STOP",error)) return ScopeReferenceStatus::Error;
        m_referenceStopping = true;
    }
    acquisition = acquisitionState();
    if (acquisition == ScopeAcquisitionState::Invalid) return fail(m_lastError);
    if (acquisition != ScopeAcquisitionState::Completed) return ScopeReferenceStatus::Pending;
    if (!readCount(count)) return fail(error);
    describe();
    if (count < m_referenceFrozenAfter) return fail("AUTO reference counter reset while freezing: " + state);
    m_referenceActive = false;
    return ScopeReferenceStatus::Completed;
}

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
        !read(QString("CH%1:OFFSet?").arg(channel), actual.center) ||
        !read(QString("CH%1:POSition?").arg(channel), actual.position)) {
        if (error.isEmpty()) error = "Invalid vertical scale";
        return false;
    }
    // 4/5/6 MSO Help: ten vertical acquisition divisions. Position is display-only.
    // Invert reverses both waveform samples and the reported Offset; do not negate twice.
    actual.divisions = 10;
    actual.minimumPosition = OscilloscopeSettingRules::positionMinimum;
    actual.maximumPosition = OscilloscopeSettingRules::positionMaximum;
    if (actual.position < actual.minimumPosition || actual.position > actual.maximumPosition) {
        error = "Invalid vertical position";
        return false;
    }
    range = actual;
    return true;
}

bool MSOSeries456::setVerticalPosition(int channel, double position, QString &error)
{
    if (channel < 1 || channel > getTotalChannel() || !std::isfinite(position) ||
        position < OscilloscopeSettingRules::positionMinimum || position > OscilloscopeSettingRules::positionMaximum)
        return unsupportedSetting("setVerticalPosition: invalid channel or position", error);
    return writeConfigurationCommand(QString("CH%1:POSition %2").arg(channel).arg(position, 0, 'g', 16), error);
}
