#pragma once
#include "oscilloscopesettings.h"
#include <QAtomicInt>

class IScopeConfiguration
{
  public:
    virtual ~IScopeConfiguration() = default;
    // Settings are decoded and validated before execution. Apply in driver order,
    // checking cancellation between commands and stopping on the first failure.
    virtual bool applySettings(const OscilloscopeSettings &settings, QAtomicInt &stop, QString &error) = 0;
};
