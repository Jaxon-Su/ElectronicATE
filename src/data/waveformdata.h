#pragma once

#include <QString>
#include <QVector>

namespace Waveform {

constexpr qsizetype MaxSamples = 10000000;

struct Statistics {
    double minimum = 0;
    double maximum = 0;
    double mean = 0;
    double rms = 0;
};

struct Trace {
    QString sourcePath;
    QString label;
    QString unit;
    double start = 0;
    double interval = 1;
    QVector<double> samples;
    Statistics statistics;
    // Min/Max records retain both bounds; their intra-interval timing is unknown.
    bool minMaxPairs = false;

    qsizetype valuesPerPoint() const { return minMaxPairs ? 2 : 1; }
    qsizetype pointCount() const { return samples.size() / valuesPerPoint(); }
    double pointInterval() const { return interval * valuesPerPoint(); }
    double timeAt(qsizetype index) const { return start + double(index / valuesPerPoint()) * pointInterval(); }
    double end() const { return samples.isEmpty() ? start : timeAt(samples.size() - 1); }
};

struct Result {
    Trace trace;
    QString error;
    bool cancelled = false;
    bool ok() const { return error.isEmpty() && !cancelled; }
};

} // namespace Waveform
