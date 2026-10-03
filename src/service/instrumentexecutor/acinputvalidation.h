#pragma once
#include "parameterparser.h"
#include <cmath>

inline QString validateAcParameters(const QString &model, const ParameterParser::InputParameters &p)
{
    if (!QStringList{"61505", "61509", "6530", "DE-A3000AB"}.contains(model))
        return "Unsupported AC source model";
    if (!p.valid || !std::isfinite(p.voltage) || !std::isfinite(p.frequency) || !std::isfinite(p.phase) ||
        p.voltage < 0 || p.frequency <= 0 || p.phase < 0 || p.phase > 360)
        return "Invalid AC voltage, frequency or phase";
    if (p.isThreePhase() && model != "61509") return "3phase requires Chroma 61509";
    // Same verified Chroma limits used by Brown Out. No guessed Delta ratings.
    if (model != "DE-A3000AB") {
        const double phaseVoltage = p.isThreePhase() ? p.voltage / std::sqrt(3.0) : p.voltage;
        if (phaseVoltage > (model == "61509" ? 350 : 300) || p.frequency < 15 ||
            p.frequency > (model == "61505" ? 1000 : 2000))
            return "AC voltage/frequency exceeds the supported model range";
    }
    return {};
}
