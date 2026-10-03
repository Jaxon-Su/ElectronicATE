#pragma once
#include "oscilloscopesettings.h"
#include <QStringList>

// MSO protocol mapping; callers outside the driver use IScopeConfiguration.
QStringList compileMsoConfiguration(const OscilloscopeSettings &settings);
