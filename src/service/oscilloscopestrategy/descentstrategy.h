#pragma once
#include "transientstrategy.h"

class DescentStrategy final : public IOscilloscopeMeasureStrategy {
public:
    DescentStrategy(bool shortTest, TransientContext context)
        : m_shortTest(shortTest), m_context(std::move(context)) {}
    QString name() const override;
    OscMeasureResult execute(IScopeMeasurement *scope, QAtomicInt &stop) override;
private:
    bool m_shortTest;
    TransientContext m_context;
};
