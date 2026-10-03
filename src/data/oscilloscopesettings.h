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
