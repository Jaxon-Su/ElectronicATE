#pragma once
#include <QStringList>
#include <cmath>
#include <stdexcept>

namespace Chroma63600Commands {
enum class SlopeMode { Static, Dynamic, Both };
inline QString build(int channel, SlopeMode mode, double rise, double fall, double von)
{
    if (channel < 1 || channel > 10 || !std::isfinite(rise) || rise <= 0 ||
        !std::isfinite(fall) || fall <= 0 || !std::isfinite(von) || von < 0)
        throw std::invalid_argument("Invalid 63600 channel, slew rate or Von");
    if (mode != SlopeMode::Static && mode != SlopeMode::Dynamic && mode != SlopeMode::Both)
        throw std::invalid_argument("Invalid 63600 slope mode");
    QStringList commands{QString("CHAN %1").arg(channel)};
    auto appendSlopes = [&](const QString &group) {
        commands << QString("CURR:%1:RISE %2").arg(group, QString::number(rise, 'g', 12));
        commands << QString("CURR:%1:FALL %2").arg(group, QString::number(fall, 'g', 12));
    };
    if (mode != SlopeMode::Dynamic) appendSlopes("STAT");
    if (mode != SlopeMode::Static) appendSlopes("DYN");
    commands << QString("CONF:VOLT:ON %1").arg(QString::number(von, 'g', 12));
    return commands.join('\n');
}
} // namespace Chroma63600Commands
