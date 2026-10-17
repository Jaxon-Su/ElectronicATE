#pragma once
#include "../brownout/ibrownoutio.h"
#include "brownoutsettings.h"
#include "../operations/instrumentoperations.h"
#include "testsequencetaskpayload.h"
#include "icommunication.h"

// Pure snapshot validation. The returned factory opens transports only when invoked
// on the Brown Out controller thread, and owns them until output cleanup finishes.
BrownOutIoFactory prepareBrownOutIo(const TestSequenceExecutionContext &, const BrownOut::Settings &,
                                   const InstrumentOperations &,
                                   std::function<ICommunication *(const QString &)> createCommunication);
