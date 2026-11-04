#pragma once
#include "acsourcefamily.h"
#include "parameterparser.h"
#include <cmath>

inline QString validateAcParameters(const QString &model, const ParameterParser::InputParameters &p)
{
    const auto family = acSourceFamily(model);
    if (family == ACSourceFamily::Unknown)
        return "Unsupported AC source model";
    if (!p.valid || !std::isfinite(p.voltage) || !std::isfinite(p.frequency) || !std::isfinite(p.phase) ||
        p.voltage < 0 || p.frequency <= 0 || p.phase < 0 || p.phase > 360)
        return "Invalid AC voltage, frequency or phase";
    const bool is6150x = family == ACSourceFamily::Chroma61507_61508_61509;
    if (p.isThreePhase() && !is6150x && model != "6590")
        return "3phase requires Chroma 61507 / 61508 / 61509 / 6590";
    const bool is6400 = family == ACSourceFamily::Chroma6415_6420_6430;
    const bool is6560or6590 = family == ACSourceFamily::Chroma6560_6590;
    if (is6400 && p.phase != 0)
        return "6415 / 6420 / 6430 do not support a programmable start angle; use 0 for no phase request";
    // Same verified Chroma limits used by Brown Out. No guessed Delta ratings.
    if (family != ACSourceFamily::DeltaA3000) {
        const double phaseVoltage = p.isThreePhase() ? p.voltage / std::sqrt(3.0) : p.voltage;
        const double minHz = (is6400 || is6560or6590) ? 45 : 15;
        // 6400 manual §6.6 limits the remote FREQ command to 500 Hz.
        const double maxHz = is6400 ? 500 : (is6560or6590 || family == ACSourceFamily::Chroma61505) ? 1000 : 2000;
        if (phaseVoltage > (is6150x ? 350 : 300) || p.frequency < minHz || p.frequency > maxHz)
            return "AC voltage/frequency exceeds the supported model range";
    }
    return {};
}
