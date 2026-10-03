#pragma once
#include "transientstrategy.h"
#include <utility>
class TurnOnThenShortStrategy final : public IOscilloscopeMeasureStrategy
{
  public:
    explicit TurnOnThenShortStrategy(TransientContext context) : m_context(std::move(context)) {}
    OscMeasureResult execute(IScopeMeasurement *scope, QAtomicInt &stopFlag) override;
    QString name() const override;

  private:
    TransientContext m_context;
};
