#pragma once
#include "instrumentoperations.h"
#include "oscilloscopemanager.h"

// Manual control additionally owns instrument discovery and connection lifecycle.
struct Page3Operations : InstrumentOperations {
    std::function<OscilloscopeManager::OscMap(const Page1Config &)> connectScopes;
};
