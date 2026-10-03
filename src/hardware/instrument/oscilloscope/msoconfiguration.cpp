#include "msoconfiguration.h"
#include "msoseries456.h"

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
    const auto &h = settings.horizontal;
    text("HORizontal:MODe ", h.mode);
    numeric("HORizontal:SCAle ", h.secondsPerDivision);
    numeric("HORizontal:POSition ", h.positionPercent);
    text("HORizontal:MODe:MANual:CONFigure ", h.adjustment);
    numeric("HORizontal:MODe:SAMPLERate ", h.sampleRate);
    numeric("HORizontal:MODe:RECOrdlength ", h.recordLength);
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
    if (settings.trigger.source || settings.trigger.slope)
        commands << "TRIGger:A:TYPe EDGE";
    text("TRIGger:A:EDGE:SOUrce ", settings.trigger.source);
    text("TRIGger:A:EDGE:SLOpe ", settings.trigger.slope);
    return commands;
}

bool MSOSeries456::applySettings(const OscilloscopeSettings &settings, QAtomicInt &stop, QString &error)
{
    error.clear();
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
    return true;
}
