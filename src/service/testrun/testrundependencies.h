#pragma once
#include "../operations/instrumentoperations.h"
#include "../oscilloscopestrategy/ioscilloscopemeasurestrategy.h"
#include "oscilloscopesettings.h"
#include <QVariantMap>
#include <memory>
struct TransientContext;
class IScopeAutoPeriod;

// Synchronous operations; the worker determines the execution thread.
struct TestRunDependencies {
    InstrumentOperations instruments;
    std::function<std::unique_ptr<IOscilloscopeMeasureStrategy>(const QString &, const TransientContext *)>
        strategy;
    std::function<bool(const QString &, int, const QVariantMap &, OscilloscopeSettings &, QString &)> prepareConfiguration;
    std::function<bool(IScopeAutoPeriod *, QAtomicInt &, QString &)> autoPeriod;
};
