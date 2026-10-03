#pragma once
#include <QString>
#include "transientstrategy.h"
class IOscilloscopeMeasureStrategy;
class OscilloscopeStrategyFactory {
  public:
    static IOscilloscopeMeasureStrategy* create(const QString& taskName,
                                                const TransientContext* context = nullptr);
};
