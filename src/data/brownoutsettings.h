#pragma once
#include <QVariantMap>
#include "inputselection.h"
#include "captureoptions.h"
#include <QStringList>
#include <cmath>
#include <stdexcept>

namespace BrownOut {
enum class Mode { FixedFall, Track };
enum class RampSource { AC, DC };
inline constexpr double minimumInputVoltage = 0;
inline constexpr int shutdownTimeoutMs = 120000;
struct NumberRule {
    const char *key;
    double initial, minimum, maximum;
    bool integer = false;
};
inline constexpr NumberRule rules[] = {
    {"voltage_step", 0.5, 0.001, 1000},
    {"step_interval_ms", 1000, 100, 60000, true}, {"delay_ms", 5000, 0, 3600000, true},
    {"trigger_channel", 4, 1, 8, true},
    {"trigger_level", 8, -10000, 10000}, {"monitor_output", 1, 1, 100, true},
    {"shutdown_voltage", 0.5, 0, 1000}, {"shutdown_confirm_ms", 500, 100, 60000, true},
    {"tail_ms", 1000, 0, 60000, true}, {"data_timeout_ms", 10000, 1000, 60000, true},
    {"dc_source", 1, 1, 3, true}
};
inline bool validField(const QString &key, const QVariant &value)
{
    if (key == "ramp_source")
        return value.metaType().id() == QMetaType::QString && QStringList{"AC", "DC"}.contains(value.toString());
    if (key == "ignore_advanced")
        return value.metaType().id() == QMetaType::Bool;
    if (key == "mode" || key == "direction")
        return value.metaType().id() == QMetaType::QString &&
            (key == "mode" ? QStringList{"FIXED_FALL", "TRACK"} : QStringList{"RISE", "FALL"})
                .contains(value.toString());
    for (const auto &rule : rules) {
        if (key != rule.key)
            continue;
        const int type = value.metaType().id();
        if (type != QMetaType::Double && type != QMetaType::Int && type != QMetaType::LongLong &&
            type != QMetaType::UInt && type != QMetaType::ULongLong && type != QMetaType::Float)
            return false;
        const double n = value.toDouble();
        return std::isfinite(n) && n >= rule.minimum && n <= rule.maximum &&
               (!rule.integer || std::floor(n) == n);
    }
    return false;
}
inline QVariantMap defaults()
{
    QVariantMap result{{"mode", "FIXED_FALL"}, {"direction", "FALL"}, {"ignore_advanced", true}, {"ramp_source", "AC"}};
    for (const auto &rule : rules)
        result[rule.key] = rule.integer ? QVariant(int(rule.initial)) : QVariant(rule.initial);
    return result;
}
struct Settings {
    InputSelection inputs;
    bool captureEnabled = false;
    CaptureOptions captureSettings;
    int load = -1;
    RampSource rampSource;
    Mode mode;
    bool rising;
    // Internal limits are not task settings and cannot be overridden by UI/XML.
    double minimumVin = minimumInputVoltage;
    int timeoutMs = shutdownTimeoutMs;
    double voltageStep, triggerLevel, shutdownVoltage;
    int stepIntervalMs, settleMs, triggerChannel, monitorOutput, confirmMs, tailMs,
        dataTimeoutMs, dcSource;
    explicit Settings(const QVariantMap &draft) : inputs(draft)
    {
        if (draft.contains("capture_enabled") && draft.value("capture_enabled").metaType().id() != QMetaType::Bool)
            throw std::invalid_argument("Brown Out: invalid capture_enabled");
        captureEnabled = draft.value("capture_enabled", false).toBool();
        if (captureEnabled) captureSettings = CaptureOptions(draft.value("capture_settings").toMap(), true);
        const auto builtIn = defaults();
        auto v = builtIn;
        for (auto it = draft.begin(); it != draft.end(); ++it) {
            if (v.contains(it.key())) {
                if (!validField(it.key(), it.value()))
                    throw std::invalid_argument(("Brown Out: invalid " + it.key()).toStdString());
                v[it.key()] = it.value();
            }
        }
        load = InputSelection::index(draft, "load_index");
        rampSource = v["ramp_source"] == "AC" ? RampSource::AC : RampSource::DC;
        if (v["ignore_advanced"].toBool()) {
            for (const char *key : {"data_timeout_ms", "shutdown_confirm_ms", "tail_ms"})
                v[key] = builtIn[key];
        }
        mode = v["mode"] == "TRACK" ? Mode::Track : Mode::FixedFall;
        rising = mode == Mode::Track && v["direction"] == "RISE";
        voltageStep = v["voltage_step"].toDouble();
        triggerLevel = v["trigger_level"].toDouble(); shutdownVoltage = v["shutdown_voltage"].toDouble();
        stepIntervalMs = v["step_interval_ms"].toInt(); settleMs = v["delay_ms"].toInt();
        triggerChannel = v["trigger_channel"].toInt();
        monitorOutput = v["monitor_output"].toInt(); confirmMs = v["shutdown_confirm_ms"].toInt();
        tailMs = v["tail_ms"].toInt(); dataTimeoutMs = v["data_timeout_ms"].toInt();
        dcSource = v["dc_source"].toInt();
        if (confirmMs + tailMs >= timeoutMs)
            throw std::invalid_argument("Brown Out: timeout must exceed shutdown confirmation and tail time");
    }
};
}
