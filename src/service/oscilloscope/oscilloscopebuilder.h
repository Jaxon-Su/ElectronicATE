#pragma once
#include "oscilloscopemanager.h"
#include "instrumentsetupconfig.h"
#include <functional>

using OscilloscopeCreator =
    std::function<std::shared_ptr<Oscilloscope>(const QString& modelName, const CommunicationConfig& communication)>;

// Connection policy is independent of concrete communication/device factories.
OscilloscopeManager::OscMap buildConnectedOscilloscopes(const InstrumentSetupConfig& config,
                                                        const OscilloscopeCreator& create);
