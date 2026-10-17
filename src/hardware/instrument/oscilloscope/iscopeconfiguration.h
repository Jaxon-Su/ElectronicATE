#pragma once
#include "oscilloscopesettings.h"
#include <QAtomicInt>

class IScopeConfiguration
{
  public:
    virtual ~IScopeConfiguration() = default;
    // Settings are decoded and validated before execution. Apply in driver order,
    // checking cancellation between commands and stopping on the first failure.
    // Horizontal writes also require a valid readback, even when result is null.
    virtual bool applySettings(const OscilloscopeSettings &settings, QAtomicInt &stop, QString &error,
                               OscilloscopeConfigurationResult *result = nullptr) = 0;
};
