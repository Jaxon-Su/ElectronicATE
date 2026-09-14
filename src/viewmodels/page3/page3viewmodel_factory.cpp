#include "page3viewmodel.h"
#include "instrumentexecutor.h"

namespace {
Page3Operations hardwareOperations()
{
    Page3Operations operations;
    operations.input = [](const Page1Config& config, const InputRow& row, InputAction action) {
        return InstrumentExecutor::runInput(config, row, action);
    };
    operations.relay = &InstrumentExecutor::runRelay;
    operations.load = &InstrumentExecutor::runLoad;
    operations.dynamic = &InstrumentExecutor::runDyLoad;
    operations.connectScopes = &OscilloscopeManager::buildFromConfig;
    return operations;
}
}

Page3ViewModel::Page3ViewModel(Page3Model* model, QObject* parent)
    : Page3ViewModel(model, hardwareOperations(), parent)
{}
