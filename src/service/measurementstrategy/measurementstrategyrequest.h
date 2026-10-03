#pragma once
#include "../oscilloscopestrategy/transientstrategy.h"
#include "../brownout/ibrownoutio.h"
#include "brownoutsettings.h"
#include <variant>

class IScopeBrownOut;

// Each request carries only the capabilities/settings required by that family.
// Creating a strategy must not open transports or operate hardware.
struct ScopeMeasurementRequest {
    QString taskName;
    IScopeMeasurement &scope;
    TransientContext context;
    OscCaptureObserver capture;
};

struct BrownOutMeasurementRequest {
    BrownOut::Settings settings;
    IScopeBrownOut &scope;
    BrownOutIoFactory createIo;
    OscCaptureObserver capture;
};

using MeasurementStrategyRequest = std::variant<ScopeMeasurementRequest, BrownOutMeasurementRequest>;
