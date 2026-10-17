#include "instrumentexecutor.h"

#include "acsource.h"
#include "chroma61509.h"
#include "parameterparser.h"
#include "acinputconfiguration.h"
#include "acinputvalidation.h"
#include "resourcecleaner.h"

#include <QDebug>
#include <cmath>
#include <QScopeGuard>

namespace
{

bool executeACSourceAction(ACSource *source, InputAction action,
                           const ParameterParser::InputParameters &params,
                           InstrumentExecutor::Result &result)
{
    auto configureInputSource = [source, &params] { return configureACInput(source, params); };

    switch (action) {
    case InputAction::PowerOn:
        if (!configureInputSource())
            return false;
        result.outputUnchanged = false;
        source->setPowerOn();
        result.confirmedOutput = true;
        break;
    case InputAction::Change:
        if (!configureInputSource())
            return false;
        break;
    case InputAction::PowerOff: {
        std::exception_ptr settingError;
        try {
            source->setVoltage(0);
        } catch (...) {
            settingError = std::current_exception();
        }
        result.outputUnchanged = false;
        source->setPowerOff(); // Still attempt OFF if setting zero volts failed.
        result.confirmedOutput = false;
        if (settingError)
            std::rethrow_exception(settingError);
    } break;
    }

    return true;
}

InstrumentExecutor::Result
runInputWithParams(const InstrumentSetupConfig &cfg, const ParameterParser::InputParameters &params, InputAction action)
{
    if (action != InputAction::PowerOff && !params.valid)
        return {false, "Input parameter parse failed", true};

    InstrumentExecutor::Result result{true, {}, true};
    bool observedEnabled = false;
    try {
        auto createResult = InstrumentCreator::createACSource(cfg);
        if (!createResult.success || !createResult.source())
            return {false, createResult.errorMessage.isEmpty() ? "AC Source creation failed"
                                                               : createResult.errorMessage, true};

        if (action != InputAction::PowerOff) {
            const auto error = validateAcParameters(createResult.source()->model(), params);
            if (!error.isEmpty()) return {false, error, true};
            observedEnabled = createResult.source()->outputEnabled();
        }
        if (!executeACSourceAction(createResult.source(), action, params, result))
            return {false, "3phase input is supported only for Chroma 61509", true};
    } catch (const std::exception &ex) {
        result.success = false;
        result.errorMessage = QString("[runInput] Exception: %1").arg(ex.what());
    } catch (...) {
        result.success = false;
        result.errorMessage = "[runInput] Unknown exception";
    }
    if (result.outputUnchanged && observedEnabled)
        result.confirmedOutput = true;
    return result;
}

} // namespace

InstrumentExecutor::Result InstrumentExecutor::runInput(const InstrumentSetupConfig &cfg, const QString &inputText,
                                                        InputAction action)
{
    try {
        if (action == InputAction::PowerOff) return runInputWithParams(cfg, {}, action);
        auto params = ParameterParser::parseInput(inputText);
        return runInputWithParams(cfg, params, action);

    } catch (const std::exception &ex) {
        return {false, QString("[runInput] Exception: %1").arg(ex.what()), true};
    }
}

InstrumentExecutor::Result InstrumentExecutor::runInput(const InstrumentSetupConfig &cfg, const InputRow &inputRow,
                                                        InputAction action)
{
    try {
        if (action == InputAction::PowerOff) return runInputWithParams(cfg, {}, action);
        auto params = ParameterParser::parseInputRow(inputRow);
        return runInputWithParams(cfg, params, action);

    } catch (const std::exception &ex) {
        return {false, QString("[runInput] Exception: %1").arg(ex.what()), true};
    }
}
