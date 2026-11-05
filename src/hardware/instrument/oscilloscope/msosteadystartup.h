#pragma once
#include "oscilloscope.h"
#include <QAtomicInt>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

// Tektronix MSO protocol sequence. Kept with the driver, not the test-run workflow.
inline bool prepareMsoSteadyAcquisition(Oscilloscope &scope, QAtomicInt &stop, QString &error, bool startContinuous = true, bool preserveEdge = false)
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
    if (!startContinuous && !write("ACQuire:STATE STOP", "Scope STOP failed"))
        return false;
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
    if (channel < 1 || channel > scope.getTotalChannel())
    {
        error = "Steady startup requires an enabled analog trigger channel";
        return false;
    }
    const bool enabled = scope.isChannelEnabled(channel);
    const QString stateError = scope.lastError();
    if (!stateError.isEmpty())
    {
        error = QString("Steady startup CH%1 display STATE query failed: %2").arg(channel).arg(stateError);
        return false;
    }
    if (!enabled)
    {
        error = QString("Steady startup requires enabled CH%1").arg(channel);
        return false;
    }
    const double level = scope.getChannelScale(channel) * 0.04;
    if (!std::isfinite(level) || level <= 0 || level >= 1e19)
    {
        error = "Invalid channel scale for positive startup trigger level";
        return false;
    }
    QStringList commands{"TRIGger:A:TYPe EDGE"};
    if (!preserveEdge)
        commands << "TRIGger:A:EDGE:SLOpe RISE"
                 << QString("TRIGger:A:LEVel:CH%1 %2").arg(channel).arg(level, 0, 'g', 16);
    commands << "ACQuire:STOPAfter RUNSTop";
    for (const auto &command : commands)
        if (!write(command, "Steady startup failed"))
            return false;
    return !startContinuous || write("ACQuire:STATE RUN", "Steady startup failed");
}
