#include "oscilloscopeconfiguration.h"
#include "oscilloscopesettingrules.h"
#include <stdexcept>

OscilloscopeSettings decodeOscilloscopeSettings(const QVariantMap &cfg, const QString &model,
                                                int channelCount)
{
    if (model != "MSOSeries456")
        throw std::invalid_argument(("Unsupported oscilloscope configuration model: " + model).toStdString());
    OscilloscopeSettings result;
    auto value = [&](const QString &key) -> std::optional<QVariant> {
        if (!cfg.contains(key))
            return {};
        auto parsed = OscilloscopeSettingRules::decode(key, QJsonValue::fromVariant(cfg[key]));
        if (!parsed)
            throw std::invalid_argument(("Invalid oscilloscope setting: " + key).toStdString());
        return parsed;
    };
    auto flag = [&](const QString &key, bool fallback = false) {
        const auto parsed = value(key);
        return parsed ? parsed->toBool() : fallback;
    };
    auto numeric = [&](const QString &key) -> std::optional<double> {
        const auto parsed = value(key);
        return parsed ? std::optional<double>(parsed->toDouble()) : std::nullopt;
    };
    auto token = [&](const QString &key) -> std::optional<QString> {
        const auto parsed = value(key);
        return parsed ? std::optional<QString>(parsed->toString()) : std::nullopt;
    };
    const bool basic = !flag("ignore_basic");
    if (basic && !flag("ignore_horizontal")) {
        auto &h = result.horizontal;
        h.mode = token("horz_mode");
        h.secondsPerDivision = numeric("timescale");
        h.positionPercent = numeric("horz_pos");
        if (h.mode == std::optional<QString>("MANUAL")) {
            h.adjustment = token("horz_config");
            h.sampleRate = numeric("sample_rate");
            h.recordLength = numeric("record_length");
        }
    }
    if (basic && !flag("ignore_acquire")) {
        result.acquisition.mode = token("acq_mode");
        if (cfg.contains("fastacq"))
            result.acquisition.fast = flag("fastacq");
    }
    if (!flag("ignore_channels")) {
        for (int channel = 1; channel <= channelCount; ++channel) {
            const auto prefix = QString("ch%1_").arg(channel);
            if (!cfg.contains(prefix + "enabled"))
                continue;
            OscilloscopeSettings::Channel c;
            c.index = channel;
            c.enabled = flag(prefix + "enabled");
            if (c.enabled) {
                if (auto termination = token(prefix + "termination"))
                    c.terminationOhms = *termination == "50Ω" ? 50 : 1000000;
                c.coupling = token(prefix + "coupling");
                if (auto bandwidth = value(prefix + "bandwidth")) {
                    c.fullBandwidth = bandwidth->toString() == "FULL";
                    if (!c.fullBandwidth)
                        c.bandwidthHz = bandwidth->toDouble();
                }
                c.scale = numeric(prefix + "scale");
                c.positionDivisions = numeric(prefix + "position");
            }
            result.channels << c;
        }
    }
    if (!flag("ignore_trigger")) {
        result.trigger.source = token("trig_source");
        if (result.trigger.source && result.trigger.source->startsWith("CH") &&
            result.trigger.source->mid(2).toInt() > channelCount)
            throw std::invalid_argument("Trigger channel is unavailable on this oscilloscope");
        const auto slope = token("trig_edge");
        if (slope)
            result.trigger.slope = (*slope == "RISING" || *slope == "RISE")    ? "RISE"
                                   : (*slope == "FALLING" || *slope == "FALL") ? "FALL"
                                                                               : "EITHER";
    }
    return result;
}

bool prepareOscilloscopeSettings(const QVariantMap &config, const QString &model, int channels,
                                 OscilloscopeSettings &settings, QString &error)
{
    settings = {};
    error.clear();
    try {
        settings = decodeOscilloscopeSettings(config, model, channels);
        return true;
    } catch (const std::exception &ex) {
        error = QString::fromUtf8(ex.what());
        return false;
    }
}
