#pragma once
#include "instrumentoperationresult.h"
#include <functional>

class QObject;

// Called on owner's thread. Work owns its inputs; completion runs only while
// owner is alive, on owner's thread. Work does not receive the owner pointer.
void runInstrumentOperation(QObject* owner,
    std::function<InstrumentOperationResult()> work,
    std::function<void(const InstrumentOperationResult&)> completion);
