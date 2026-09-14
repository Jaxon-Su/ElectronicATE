#pragma once
#include "oscilloscopemanager.h"
#include "page1config.h"
#include <functional>

using OscilloscopeCreator = std::function<std::shared_ptr<Oscilloscope>(
    const QString& modelName, const QString& resource)>;

// Connection policy is independent of concrete communication/device factories.
OscilloscopeManager::OscMap buildConnectedOscilloscopes(
    const Page1Config& config, const OscilloscopeCreator& create);
