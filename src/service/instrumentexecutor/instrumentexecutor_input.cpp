#include "instrumentexecutor.h"

#include "acsource.h"
#include "chroma61509.h"
#include "parameterparser.h"
#include "resourcecleaner.h"

#include <QDebug>
#include <cmath>
#include <QScopeGuard>

namespace {

bool executeACSourceAction(
    ACSource* source,
    InputAction action,
    const ParameterParser::InputParameters& params)
{
    auto configureInputSource = [source, &params]() -> bool {
        double voltage = params.voltage;

        if (auto* chroma61509 = dynamic_cast<Chroma61509*>(source)) {
            if (params.isThreePhase()) {
                chroma61509->setInstrumentPhase("THREE");
                chroma61509->setPhaseP12(120.0);
                chroma61509->setPhaseP13(240.0);
                voltage = params.voltage / std::sqrt(3.0);
                qDebug() << "[executeACSourceAction] Chroma61509 3phase line voltage"
                         << params.voltage << "V -> phase voltage" << voltage << "V";
            } else {
                chroma61509->setInstrumentPhase("SINGLE");
            }
        } else if (params.isThreePhase()) {
            qWarning() << "[executeACSourceAction] 3phase input selected but AC source does not support phase-mode SCPI:"
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
        source->setPowerOn();
        break;
    case InputAction::Change:
        if (!configureInputSource())
            return false;
        break;
    case InputAction::PowerOff:
        source->setVoltage(0);
        source->setPowerOff();
        break;
    }

    return true;
}

InstrumentExecutor::Result runInputWithParams(
    const Page1Config& cfg,
    const ParameterParser::InputParameters& params,
    InputAction action)
{
    if (!params.valid)
        return { false, "Input parameter parse failed" };

    auto createResult = InstrumentCreator::createACSource(cfg, nullptr);
    if (!createResult.success || !createResult.source)
        return { false, "AC Source creation failed" };

    const auto cleanup = qScopeGuard([&] { ResourceCleaner::cleanupACSource(createResult.source, createResult.comm); });

    if (!executeACSourceAction(createResult.source, action, params)) {
        return { false, "3phase input is supported only for Chroma 61509" };
    }

    return {};
}

} // namespace

InstrumentExecutor::Result
InstrumentExecutor::runInput(
    const Page1Config& cfg,
    const QString&     inputText,
    InputAction        action)
{
    try {
        auto params = ParameterParser::parseInput(inputText);
        return runInputWithParams(cfg, params, action);

    } catch (const std::exception& ex) {
        return { false, QString("[runInput] Exception: %1").arg(ex.what()) };
    }
}

InstrumentExecutor::Result
InstrumentExecutor::runInput(
    const Page1Config& cfg,
    const InputRow&    inputRow,
    InputAction        action)
{
    try {
        auto params = ParameterParser::parseInputRow(inputRow);
        return runInputWithParams(cfg, params, action);

    } catch (const std::exception& ex) {
        return { false, QString("[runInput] Exception: %1").arg(ex.what()) };
    }
}
