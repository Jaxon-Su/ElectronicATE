#include "oscilloscope.h"
#include "testrunservicefactory.h"
#include "instrumentexecutor.h"
#include "dcinputoperation.h"
#include "communicationfactory.h"
#include "oscilloscopestrategyfactory.h"
#include "oscilloscopeconfiguration.h"
#include "oscilloscopeautoperiod.h"
#include "brownoutio.h"
#include "measurementpreflight.h"
namespace
{
InstrumentOperations hardwareOperations()
{
    InstrumentOperations operations;
    operations.input = [](const InstrumentSetupConfig &config, const InputRow &row, InputAction action) {
        return InstrumentExecutor::runInput(config, row, action);
    };
    operations.relay = &InstrumentExecutor::runRelay;
    operations.load = &InstrumentExecutor::runLoad;
    operations.dynamic = &InstrumentExecutor::runDynamicLoad;
    operations.dcGroup = [](const InstrumentSetupConfig &config, const DcGroup &group, InputAction action) {
        return runDcGroup(config, group, action, &CommunicationFactory::create);
    };
    return operations;
}
} // namespace

namespace
{
TestRunDependencies productionDependencies(InstrumentOperations instruments)
{
    TestRunDependencies result;
    result.instruments = std::move(instruments);
    result.validateMeasurement = &validateMeasurementConditions;
    result.strategy = makeMeasurementStrategyFactory([](const QString &name, const TransientContext *context) {
        return std::unique_ptr<IOscilloscopeMeasureStrategy>(
            OscilloscopeStrategyFactory::create(name, context));
    });
    result.prepareConfiguration = [](const QString &model, int channels, const QVariantMap &values,
                                     OscilloscopeSettings &configuration, QString &error) {
        return prepareOscilloscopeSettings(values, model, channels, configuration, error);
    };
    result.autoPeriod = &adjustOscilloscopeAutoPeriod;
    result.prepareBrownOut = [operations = result.instruments](const TestSequenceExecutionContext &context,
                                                               const BrownOut::Settings &settings) {
        return prepareBrownOutIo(context, settings, operations, &CommunicationFactory::create);
    };
    return result;
}
} // namespace
std::unique_ptr<TestRunService> makeTestRunService()
{
    return makeTestRunService(hardwareOperations());
}

std::unique_ptr<TestRunService> makeTestRunService(InstrumentOperations operations)
{
    return std::make_unique<TestRunService>(productionDependencies(std::move(operations)));
}
