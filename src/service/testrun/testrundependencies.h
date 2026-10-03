#pragma once
#include "../operations/instrumentoperations.h"
#include "../measurementstrategy/measurementstrategyfactory.h"
#include "oscilloscopesettings.h"
#include "../brownout/ibrownoutio.h"
#include "brownoutsettings.h"
#include "page5taskpayload.h"
#include <QVariantMap>
#include <memory>
class IScopeAutoPeriod;

// Synchronous operations; the worker determines the execution thread.
struct TestRunDependencies {
    InstrumentOperations instruments;
    MeasurementStrategyFactory strategy;
    std::function<void(const Page5ExecutionContext &, InputSelection, int, bool)> validateMeasurement;
    std::function<bool(const QString &, int, const QVariantMap &, OscilloscopeSettings &, QString &)> prepareConfiguration;
    std::function<bool(IScopeAutoPeriod *, QAtomicInt &, QString &, const OscCaptureObserver &)> autoPeriod;
    std::function<BrownOutIoFactory(const Page5ExecutionContext &, const BrownOut::Settings &)> prepareBrownOut;
};
