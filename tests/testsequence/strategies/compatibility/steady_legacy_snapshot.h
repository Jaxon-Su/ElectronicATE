#pragma once
#include "ioscilloscopemeasurestrategy.h"

class LegacySteadyStateSearchStrategy : public IOscilloscopeMeasureStrategy
{
  public:
    explicit LegacySteadyStateSearchStrategy(int timeoutMs = 10000, int phaseTimeoutMs = 900000,
                                   int maxIterations = 100, const QString &target = "BOTH");
    OscMeasureResult execute(IScopeMeasurement *scope, QAtomicInt &stopFlag) override;
    QString name() const override;

  private:
    QString m_target;
    int m_timeoutMs;
    int m_phaseTimeoutMs;
    int m_maxIterations;
};
