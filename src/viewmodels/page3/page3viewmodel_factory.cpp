#include "page3viewmodel.h"
#include "instrumentexecutor.h"
#include "dcinputoperation.h"
#include "communicationfactory.h"

namespace {
Page3Operations hardwareOperations()
{
    Page3Operations operations;
    operations.dcGroup = [](const Page1Config& config, const DcGroup& rows, InputAction action) {
        return runDcGroup(config, rows, action, &CommunicationFactory::create);
    };
    operations.dcInput = [](const Page1Config& config, int source, const DcRow& row, InputAction action) {
        return runDcInput(config, source, row, action, &CommunicationFactory::create);
    };
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
