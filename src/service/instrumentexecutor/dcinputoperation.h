#pragma once
#include "page1config.h"
#include "page2config.h"
#include "instrumentactions.h"
#include "instrumentoperationresult.h"
#include <functional>
class ICommunication;

// Source index is zero-based, matching Page2/Page3 tabs. Factory returns ownership.
InstrumentOperationResult
runDcInput(const Page1Config& config, int source, const DcRow& row, InputAction action,
           const std::function<ICommunication*(const QString&)>& createCommunication);

InstrumentOperationResult
runDcGroup(const Page1Config& config, const DcGroup& rows, InputAction action,
           const std::function<ICommunication*(const QString&)>& createCommunication);
