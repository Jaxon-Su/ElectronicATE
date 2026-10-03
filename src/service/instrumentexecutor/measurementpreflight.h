#pragma once
#include "page5taskpayload.h"
#include "inputselection.h"

// Throws a descriptive configuration error without opening any instrument.
void validateMeasurementConditions(const Page5ExecutionContext &, InputSelection, int load, bool dynamic);
