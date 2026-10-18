#pragma once
#include "instrumentoperations.h"
#include "oscilloscopemanager.h"

// Manual control additionally owns instrument discovery and connection lifecycle.
struct ManualControlOperations : InstrumentOperations {
    std::function<OscilloscopeManager::OscMap(const InstrumentSetupConfig &)> connectScopes;
};
