#pragma once
#include "imeasurementstrategy.h"
#include "../oscilloscopestrategy/ioscilloscopemeasurestrategy.h"
#include <memory>

// Binds the existing steady/transient algorithm to its measurement capability.
class ScopeMeasurementStrategy final : public IMeasurementStrategy {
  public:
    ScopeMeasurementStrategy(std::unique_ptr<IOscilloscopeMeasureStrategy> algorithm,
                             IScopeMeasurement &scope);
    QString name() const override;
    MeasurementResult execute(QAtomicInt &stop) override;

  private:
    std::unique_ptr<IOscilloscopeMeasureStrategy> m_algorithm;
    IScopeMeasurement &m_scope;
};
