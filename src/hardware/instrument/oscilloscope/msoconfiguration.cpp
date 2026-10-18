#include "msoconfiguration.h"
#include "msoseries456.h"
#include <cmath>

namespace {
// This readback belongs only to Write Oscilloscope. Capture query/transfer paths
// deliberately remain separate. No RUN, STOP, acquisition wait, or retry here.
bool readHorizontalSettings(MSOSeries456 &scope, QAtomicInt &stop,
                            OscilloscopeConfigurationResult::Horizontal &actual, QString &error)
{
    auto query = [&](const QString &command, QString &value) {
        if (stop.loadAcquire()) {
            error = "Stopped by user";
            return false;
        }
        if (!scope.queryConfiguration(command, value, error)) {
            error = "Horizontal readback " + command + ": " + error;
            return false;
        }
        value = value.trimmed();
        QString header = command.chopped(1);
        if (value.startsWith(':')) value.remove(0, 1);
        if (value.startsWith(header + ' ', Qt::CaseInsensitive))
            value = value.mid(header.size()).trimmed();
        if (stop.loadAcquire()) {
            error = "Stopped by user";
            return false;
        }
        return true;
    };
    auto status = [&] {
        QString value;
        if (!query("*ESR?", value)) return false;
        bool ok = false;
        const int esr = value.toInt(&ok);
        if (!ok || esr < 0 || esr > 255 || (esr & 0x3c)) {
            error = "Horizontal readback: invalid/error ESR=" + value;
            return false;
        }
        return true;
    };
    auto number = [&](const QString &command, double &result, bool integer = false) {
        QString value;
        if (!query(command, value)) return false;
        bool ok = false;
        const double parsed = value.toDouble(&ok);
        if (!ok || !std::isfinite(parsed) || parsed <= 0 || parsed >= 1e30 ||
            (integer && (std::floor(parsed) != parsed || parsed > 9007199254740991.0))) {
            error = "Horizontal readback: invalid " + command + " response: " + value;
            return false;
        }
        result = parsed;
        return true;
    };
    if (!status() || !query("HORizontal:MODe?", actual.mode)) return false;
    actual.mode = actual.mode.toUpper();
    if (actual.mode != "AUTO" && actual.mode != "MANUAL") {
        error = "Horizontal readback: invalid mode: " + actual.mode;
        return false;
    }
    if (actual.mode == "MANUAL") {
        if (!query("HORizontal:MODe:MANual:CONFigure?", actual.adjustment)) return false;
        actual.adjustment = actual.adjustment.toUpper();
        if (actual.adjustment != "HORIZONTALSCALE" && actual.adjustment != "RECORDLENGTH") {
            error = "Horizontal readback: invalid adjustment: " + actual.adjustment;
            return false;
        }
    }
    return number("HORizontal:SCAle?", actual.secondsPerDivision) &&
           number("HORizontal:MODe:SAMPLERate?", actual.sampleRate) &&
           number("HORizontal:MODe:RECOrdlength?", actual.recordLength, true) &&
           number("HORizontal:DIVisions?", actual.divisions) && status();
}
}

QStringList compileMsoConfiguration(const OscilloscopeSettings &settings)
{
    QStringList commands;
    auto text = [&](const QString &command, const std::optional<QString> &value) {
        if (value)
            commands << command + *value;
    };
    auto numeric = [&](const QString &command, const std::optional<double> &value) {
        if (value)
            commands << command + QString::number(*value, 'g', 15);
    };
    // Acquisition/channel changes can constrain the horizontal settings. Apply
    // them first, then establish the requested pair, then read back everything.
    text("ACQuire:MODe ", settings.acquisition.mode);
    if (settings.acquisition.fast)
        commands << QString("ACQuire:FASTAcq:STATE %1").arg(*settings.acquisition.fast ? "ON" : "OFF");
    for (const auto &c : settings.channels) {
        commands << QString("DISplay:GLObal:CH%1:STATE ").arg(c.index) + (c.enabled ? "ON" : "OFF");
        if (!c.enabled)
            continue;
        const auto prefix = QString("CH%1:").arg(c.index);
        numeric(prefix + "TERmination ", c.terminationOhms);
        text(prefix + "COUPling ", c.coupling);
        if (c.fullBandwidth)
            commands << prefix + "BANdwidth FULl";
        else
            numeric(prefix + "BANdwidth ", c.bandwidthHz);
        numeric(prefix + "SCAle ", c.scale);
        numeric(prefix + "POSition ", c.positionDivisions);
    }
    const auto &h = settings.horizontal;
    text("HORizontal:MODe ", h.mode);
    text("HORizontal:MODe:MANual:CONFigure ", h.adjustment);
    numeric("HORizontal:MODe:SAMPLERate ", h.sampleRate);
    // Time priority: SR then time; Sample Rate priority: SR then RL. Never write
    // all three, since the last write would invalidate an earlier selection.
    if (h.adjustment == std::optional<QString>("HORIZONTALSCALE"))
        numeric("HORizontal:MODe:RECOrdlength ", h.recordLength);
    else
        numeric("HORizontal:SCAle ", h.secondsPerDivision);
    numeric("HORizontal:POSition ", h.positionPercent);
    if (settings.trigger.source || settings.trigger.slope)
        commands << "TRIGger:A:TYPe EDGE";
    text("TRIGger:A:EDGE:SOUrce ", settings.trigger.source);
    text("TRIGger:A:EDGE:SLOpe ", settings.trigger.slope);
    return commands;
}

bool MSOSeries456::applySettings(const OscilloscopeSettings &settings, QAtomicInt &stop, QString &error,
                                 OscilloscopeConfigurationResult *result)
{
    error.clear();
    if (result) *result = {};
    if (stop.loadAcquire()) {
        error = "Stopped by user";
        return false;
    }
    const auto commands = compileMsoConfiguration(settings);
    for (const auto &command : commands) {
        if (stop.loadAcquire()) {
            error = "Stopped by user";
            return false;
        }
        if (!writeConfigurationCommand(command, error))
            return false;
    }
    if (stop.loadAcquire()) {
        error = "Stopped by user";
        return false;
    }
    const auto &h = settings.horizontal;
    if (h.mode || h.secondsPerDivision || h.sampleRate || h.recordLength || h.adjustment) {
        OscilloscopeConfigurationResult::Horizontal actual;
        if (!readHorizontalSettings(*this, stop, actual, error)) return false;
        if (result) result->horizontal = actual;
    }
    return true;
}
