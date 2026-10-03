#pragma once
#include "transientstrategy.h"
#include <utility>
class TurnOffStrategy final : public IOscilloscopeMeasureStrategy
{
  public:
    explicit TurnOffStrategy(TransientContext context) : m_context(std::move(context)) {}
    OscMeasureResult execute(IScopeMeasurement *scope, QAtomicInt &stopFlag) override;
    QString name() const override;

  private:
    TransientContext m_context;
};
