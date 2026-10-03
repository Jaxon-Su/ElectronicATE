#include "msoseries456.h"
#include <limits>

ScopeCaptureSource MSOSeries456::captureSource()
{
    const auto name = getTriggerSource().simplified().section(' ', -1).toUpper();
    bool ok = false;
    const int channel = name.startsWith("CH") ? name.mid(2).toInt(&ok) : 0;
    return {name, ok ? channel : 0};
}

QString MSOSeries456::captureSlope()
{
    return getTriggerSlope().simplified().section(' ', -1);
}

bool MSOSeries456::readChannelOffset(int channel, double &offset, QString &error)
{
    offset = std::numeric_limits<double>::quiet_NaN();
    QString response;
    if (!queryConfiguration(QString("CH%1:OFFSet?").arg(channel), response, error))
        return false;
    bool ok = false;
    const double value = response.simplified().section(' ', -1).toDouble(&ok);
    if (ok)
        offset = value;
    // An unavailable number stays NaN; the caller validates the complete metadata.
    return true;
}
