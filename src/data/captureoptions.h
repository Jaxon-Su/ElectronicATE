#pragma once
#include "tasksettingrules.h"
#include <QVariantMap>
#include <QStringList>
#include <QSet>
#include <cmath>
#include <stdexcept>

struct CaptureOptions {
    QString directory;
    QStringList formats;
    int channel = TaskSettingRules::captureChannel.initial;

    CaptureOptions() = default;
    explicit CaptureOptions(const QVariantMap &values, bool defaultFormats = false)
        : directory(values.value("directory").toString().trimmed()),
          formats(values.value("formats", defaultFormats ? QStringList{"WFM", "PNG"} : QStringList{})
                      .toStringList())
    {
        QSet<QString> seen;
        if (formats.isEmpty())
            throw std::invalid_argument("Capture: select at least one format");
        for (const auto &format : formats) {
            if (!QStringList{"PNG", "CSV", "AllCSV", "WFM", "AllWFM"}.contains(format) ||
                seen.contains(format))
                throw std::invalid_argument("Capture: invalid or duplicate format");
            seen.insert(format);
        }
        if (values.contains("channel")) {
            const auto value = values.value("channel");
            const auto type = value.metaType().id();
            if (type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong &&
                type != QMetaType::ULongLong && type != QMetaType::Double && type != QMetaType::Float)
                throw std::invalid_argument("Capture: channel must be numeric");
            const double number = value.toDouble();
            if (!TaskSettingRules::captureChannel.accepts(number))
                throw std::invalid_argument("Capture: channel must be 0 or CH1-CH8");
            channel = static_cast<int>(number);
        }
    }
};
