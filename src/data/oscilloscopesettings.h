#pragma once
#include <QString>
#include <QVector>
#include <optional>

struct OscilloscopeSettings {
    struct Horizontal {
        std::optional<QString> mode, adjustment;
        std::optional<double> secondsPerDivision, positionPercent, sampleRate, recordLength;
    } horizontal;
    struct Acquisition {
        std::optional<QString> mode;
        std::optional<bool> fast;
    } acquisition;
    struct Channel {
        int index = 0;
        bool enabled = false;
        std::optional<double> terminationOhms, bandwidthHz, scale, positionDivisions;
        bool fullBandwidth = false;
        std::optional<QString> coupling;
    };
    QVector<Channel> channels;
    struct Trigger {
        std::optional<QString> source, slope;
    } trigger;
};

// A fresh snapshot after all settings have been applied. Never reuse a previous
// run's values when a query fails; the driver publishes only a complete snapshot.
struct OscilloscopeConfigurationResult {
    struct Horizontal {
        QString mode, adjustment;
        double secondsPerDivision = 0, sampleRate = 0, recordLength = 0, divisions = 0;
    };
    std::optional<Horizontal> horizontal;
};
