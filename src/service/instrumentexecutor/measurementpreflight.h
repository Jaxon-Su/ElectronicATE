#pragma once
#include "testsequencetaskpayload.h"
#include "inputselection.h"

// Throws a descriptive configuration error without opening any instrument.
void validateMeasurementConditions(const TestSequenceExecutionContext &, InputSelection, int load, bool dynamic);
