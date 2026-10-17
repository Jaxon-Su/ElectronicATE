#include "manualcontrolviewmodel.h"
#include "instrumentexecutor.h"
#include "dcinputoperation.h"
#include "communicationfactory.h"

namespace {
ManualControlOperations hardwareOperations()
{
    ManualControlOperations operations;
    operations.dcGroup = [](const InstrumentSetupConfig& config, const DcGroup& rows, InputAction action) {
        return runDcGroup(config, rows, action, &CommunicationFactory::create);
    };
    operations.dcInput = [](const InstrumentSetupConfig& config, int source, const DcRow& row, InputAction action) {
        return runDcInput(config, source, row, action, &CommunicationFactory::create);
    };
    operations.input = [](const InstrumentSetupConfig& config, const InputRow& row, InputAction action) {
        return InstrumentExecutor::runInput(config, row, action);
    };
    operations.relay = &InstrumentExecutor::runRelay;
    operations.load = &InstrumentExecutor::runLoad;
    operations.dynamic = &InstrumentExecutor::runDynamicLoad;
    operations.connectScopes = &OscilloscopeManager::buildFromConfig;
    return operations;
}
} // namespace

ManualControlViewModel::ManualControlViewModel(ManualControlModel* model, QObject* parent)
    : ManualControlViewModel(model, hardwareOperations(), parent)
{
}
