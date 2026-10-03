#pragma once
#include "imeasurementstrategy.h"
#include "measurementstrategyrequest.h"
#include <functional>
#include <memory>

using ScopeAlgorithmFactory = std::function<std::unique_ptr<IOscilloscopeMeasureStrategy>(
    const QString &, const TransientContext *)>;
using MeasurementStrategyFactory = std::function<std::unique_ptr<IMeasurementStrategy>(
    const MeasurementStrategyRequest &)>;

// The composition root supplies the existing scope algorithm factory.
// Brown Out and scope algorithms are then exposed through the same execution contract.
MeasurementStrategyFactory makeMeasurementStrategyFactory(ScopeAlgorithmFactory scopeAlgorithms);
