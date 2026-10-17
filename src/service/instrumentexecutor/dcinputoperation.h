#pragma once
#include "instrumentsetupconfig.h"
#include "testconditionsconfig.h"
#include "instrumentactions.h"
#include "instrumentoperationresult.h"
#include <functional>
class ICommunication;

// Source index is zero-based, matching TestConditions/ManualControl tabs. Factory returns ownership.
InstrumentOperationResult
runDcInput(const InstrumentSetupConfig& config, int source, const DcRow& row, InputAction action,
           const std::function<ICommunication*(const QString&)>& createCommunication);

InstrumentOperationResult
runDcGroup(const InstrumentSetupConfig& config, const DcGroup& rows, InputAction action,
           const std::function<ICommunication*(const QString&)>& createCommunication);
