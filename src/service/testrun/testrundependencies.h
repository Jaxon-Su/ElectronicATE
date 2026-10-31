#pragma once
#include "../operations/instrumentoperations.h"
#include "../measurementstrategy/measurementstrategyfactory.h"
#include "oscilloscopesettings.h"
#include "../brownout/ibrownoutio.h"
#include "brownoutsettings.h"
#include "testsequencetaskpayload.h"
#include <QVariantMap>
#include <memory>
class IScopeAutoPeriod;

// Synchronous operations; the worker determines the execution thread.
struct TestRunDependencies {
    InstrumentOperations instruments;
    MeasurementStrategyFactory strategy;
    std::function<void(const TestSequenceExecutionContext &, InputSelection, int, bool)> validateMeasurement;
    std::function<bool(const QString &, int, const QVariantMap &, OscilloscopeSettings &, QString &)> prepareConfiguration;
    std::function<bool(IScopeAutoPeriod *, QAtomicInt &, QString &, const OscCaptureObserver &,
                       const VerticalScaleOptions &, const std::function<void(const QString &)> &)> autoPeriod;
    std::function<BrownOutIoFactory(const TestSequenceExecutionContext &, const BrownOut::Settings &)> prepareBrownOut;
};
