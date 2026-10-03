#pragma once
#include "measurementresult.h"
#include <QAtomicInt>

// Dependencies are bound at construction; the caller keeps borrowed capabilities
// alive until execution (including cancellation and cleanup) returns.
class IMeasurementStrategy {
  public:
    virtual ~IMeasurementStrategy() = default;
    virtual QString name() const = 0;
    virtual MeasurementResult execute(QAtomicInt &stop) = 0;
};
