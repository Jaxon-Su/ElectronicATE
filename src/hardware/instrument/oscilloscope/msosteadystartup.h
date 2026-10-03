#pragma once
#include "oscilloscope.h"
#include <QAtomicInt>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

// Tektronix MSO protocol sequence. Kept with the driver, not the test-run workflow.
inline bool prepareMsoSteadyAcquisition(Oscilloscope &scope, QAtomicInt &stop, QString &error)
{
    error.clear();
    auto write = [&](const QString &command, const QString &phase)
    {
        if (stop.loadAcquire())
        {
            error = "Stopped by user";
            return false;
        }
        QString detail;
        if (!scope.writeConfigurationCommand(command, detail))
        {
            error = phase + ": " + detail;
            return false;
        }
        return true;
    };
    if (!write("TRIGger:A:MODe AUTO", "Trigger AUTO failed"))
        return false;
    if (stop.loadAcquire())
    {
        error = "Stopped by user";
        return false;
    }
    const auto source = QRegularExpression("(?:^|[ :])CH(\\d+)$", QRegularExpression::CaseInsensitiveOption)
                            .match(scope.getTriggerSource().trimmed());
    const int channel = source.hasMatch() ? source.captured(1).toInt() : -1;
    if (channel < 1 || channel > scope.getTotalChannel() || !scope.isChannelEnabled(channel))
    {
        error = "Steady startup requires an enabled analog trigger channel";
        return false;
    }
    const double level = scope.getChannelScale(channel) * 0.04;
    if (!std::isfinite(level) || level <= 0 || level >= 1e19)
    {
        error = "Invalid channel scale for positive startup trigger level";
        return false;
    }
    for (const auto &command :
         QStringList{"TRIGger:A:TYPe EDGE", "TRIGger:A:EDGE:SLOpe RISE",
                     QString("TRIGger:A:LEVel:CH%1 %2").arg(channel).arg(level, 0, 'g', 16),
                     "ACQuire:STOPAfter RUNSTop", "ACQuire:STATE RUN"})
        if (!write(command, "Steady startup failed"))
            return false;
    return true;
}
