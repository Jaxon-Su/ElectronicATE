#pragma once
#include "testrunservice.h"
#include <memory>

// Production wiring stays outside the service and the headless execution target.
std::unique_ptr<TestRunService> makeTestRunService();
std::unique_ptr<TestRunService> makeTestRunService(InstrumentOperations operations);
