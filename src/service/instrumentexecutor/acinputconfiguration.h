#pragma once
#include "acsource.h"
#include "chroma61507_61508_61509.h"
#include "parameterparser.h"
#include <cmath>

inline bool configureACInput(ACSource *source, const ParameterParser::InputParameters &params)
{
    double voltage = params.voltage;
    if (auto *chroma = dynamic_cast<Chroma61507_61508_61509 *>(source)) {
        if (params.isThreePhase()) {
            chroma->setInstrumentPhase("THREE");
            chroma->setPhaseP12(120.0);
            chroma->setPhaseP13(240.0);
            voltage /= std::sqrt(3.0);
        } else {
            chroma->setInstrumentPhase("SINGLE");
        }
    } else {
        if (params.isThreePhase()) voltage /= std::sqrt(3.0);
        if (!source->prepareOutput(params.isThreePhase(), voltage)) return false;
    }
    source->setVoltage(voltage);
    source->setFrequency(params.frequency);
    source->setPhaseOn(params.phase);
    return true;
}
