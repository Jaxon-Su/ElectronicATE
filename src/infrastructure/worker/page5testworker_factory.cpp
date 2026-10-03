#include "oscilloscope.h"
#include "page5testworker.h"
#include "instrumentexecutor.h"
#include "dcinputoperation.h"
#include "communicationfactory.h"
#include "oscilloscopestrategyfactory.h"
#include "oscilloscopeconfiguration.h"
#include "oscilloscopeautoperiod.h"
namespace
{
InstrumentOperations hardwareOperations()
{
    InstrumentOperations operations;
    operations.input = [](const Page1Config &config, const InputRow &row, InputAction action) {
        return InstrumentExecutor::runInput(config, row, action);
    };
    operations.relay = &InstrumentExecutor::runRelay;
    operations.load = &InstrumentExecutor::runLoad;
    operations.dynamic = &InstrumentExecutor::runDynamicLoad;
    operations.dcGroup = [](const Page1Config &config, const DcGroup &group, InputAction action) {
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
    result.strategy = [](const QString &name, const TransientContext *context) {
        return std::unique_ptr<IOscilloscopeMeasureStrategy>(
            OscilloscopeStrategyFactory::create(name, context));
    };
    result.prepareConfiguration = [](const QString &model, int channels, const QVariantMap &values,
                                     OscilloscopeSettings &configuration, QString &error) {
        return prepareOscilloscopeSettings(values, model, channels, configuration, error);
    };
    result.autoPeriod = &adjustOscilloscopeAutoPeriod;
    return result;
}
} // namespace
Page5TestWorker::Page5TestWorker(QObject *parent) : Page5TestWorker(hardwareOperations(), parent) {}
Page5TestWorker::Page5TestWorker(InstrumentOperations operations, QObject *parent)
    : Page5TestWorker(productionDependencies(std::move(operations)), parent)
{
}
