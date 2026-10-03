#pragma once
#include "oscilloscopesettings.h"
#include <QVariantMap>

// Validate all active fields before writing anything. Ignored groups are not parsed.
OscilloscopeSettings decodeOscilloscopeSettings(const QVariantMap &config, const QString &model,
                                                int channels);
bool prepareOscilloscopeSettings(const QVariantMap &config, const QString &model, int channelCount,
                                 OscilloscopeSettings &settings, QString &error);
