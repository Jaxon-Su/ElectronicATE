#include "instrumentexecutor.h"

#include "acsource.h"
#include "chroma61509.h"
#include "parameterparser.h"
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
    auto configureInputSource = [source, &params]() -> bool {
        double voltage = params.voltage;

        if (auto *chroma61509 = dynamic_cast<Chroma61509 *>(source)) {
            if (params.isThreePhase()) {
                chroma61509->setInstrumentPhase("THREE");
                chroma61509->setPhaseP12(120.0);
                chroma61509->setPhaseP13(240.0);
                voltage = params.voltage / std::sqrt(3.0);
                qDebug() << "[executeACSourceAction] Chroma61509 3phase line voltage" << params.voltage
                         << "V -> phase voltage" << voltage << "V";
            } else {
                chroma61509->setInstrumentPhase("SINGLE");
            }
        } else if (params.isThreePhase()) {
            qWarning() << "[executeACSourceAction] 3phase input selected but AC source does not support "
                          "phase-mode SCPI:"
                       << source->model();
            return false;
        }

        source->setVoltage(voltage);
        source->setFrequency(params.frequency);
        source->setPhaseOn(params.phase);
        return true;
    };

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
runInputWithParams(const Page1Config &cfg, const ParameterParser::InputParameters &params, InputAction action)
{
    if (!params.valid)
        return {false, "Input parameter parse failed", true};

    InstrumentExecutor::Result result{true, {}, true};
    try {
        auto createResult = InstrumentCreator::createACSource(cfg);
        if (!createResult.success || !createResult.source())
            return {false, createResult.errorMessage.isEmpty() ? "AC Source creation failed"
                                                               : createResult.errorMessage, true};

        if (!executeACSourceAction(createResult.source(), action, params, result))
            return {false, "3phase input is supported only for Chroma 61509", true};
    } catch (const std::exception &ex) {
        result.success = false;
        result.errorMessage = QString("[runInput] Exception: %1").arg(ex.what());
    } catch (...) {
        result.success = false;
        result.errorMessage = "[runInput] Unknown exception";
    }
    return result;
}

} // namespace

InstrumentExecutor::Result InstrumentExecutor::runInput(const Page1Config &cfg, const QString &inputText,
                                                        InputAction action)
{
    try {
        auto params = ParameterParser::parseInput(inputText);
        return runInputWithParams(cfg, params, action);

    } catch (const std::exception &ex) {
        return {false, QString("[runInput] Exception: %1").arg(ex.what()), true};
    }
}

InstrumentExecutor::Result InstrumentExecutor::runInput(const Page1Config &cfg, const InputRow &inputRow,
                                                        InputAction action)
{
    try {
        auto params = ParameterParser::parseInputRow(inputRow);
        return runInputWithParams(cfg, params, action);

    } catch (const std::exception &ex) {
        return {false, QString("[runInput] Exception: %1").arg(ex.what()), true};
    }
}
