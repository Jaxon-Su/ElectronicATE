#include "waveformstatistics.h"
#include <algorithm>
#include <cmath>

namespace Waveform {
bool stopped(const Cancellation &cancel) { return cancel && cancel->load(); }

Result summarize(Trace trace, const Cancellation &cancel)
{
    if (stopped(cancel)) return {{}, {}, true};
    if (trace.samples.isEmpty() || trace.samples.size() > MaxSamples ||
        trace.samples.size() % trace.valuesPerPoint() != 0 ||
        !std::isfinite(trace.start) || !std::isfinite(trace.interval) || trace.interval <= 0 ||
        !std::isfinite(trace.pointInterval()) ||
        !std::isfinite(trace.end()) || !std::isfinite(trace.end() - trace.start) ||
        (trace.pointCount() > 1 && trace.timeAt(trace.valuesPerPoint()) <= trace.start))
        return {{}, QStringLiteral("Invalid sample count or time axis.")};
    Statistics stats;
    stats.minimum = stats.maximum = trace.samples.first();
    long double mean = 0;
    for (qsizetype i = 0; i < trace.samples.size(); ++i) {
        if ((i & 16383) == 0 && stopped(cancel)) return {{}, {}, true};
        const double y = trace.samples[i];
        if (!std::isfinite(y)) return {{}, QStringLiteral("Waveform contains a non-finite value.")};
        if (trace.minMaxPairs && (i % 2 == 1) && trace.samples[i-1] > y)
            return {{}, QStringLiteral("Invalid Min/Max pair ordering.")};
        stats.minimum = std::min(stats.minimum, y);
        stats.maximum = std::max(stats.maximum, y);
        mean += static_cast<long double>(y) / trace.samples.size();
    }
    const double scale = std::max(std::abs(stats.minimum), std::abs(stats.maximum));
    long double squares = 0;
    if (scale > 0) {
        for (qsizetype i = 0; i < trace.samples.size(); ++i) {
            if ((i & 16383) == 0 && stopped(cancel)) return {{}, {}, true};
            const long double normalized = trace.samples[i] / scale;
            squares += normalized * normalized / trace.samples.size();
        }
    }
    stats.mean = double(mean);
    stats.rms = double(std::sqrt(squares) * scale);
    if (!std::isfinite(stats.mean) || !std::isfinite(stats.rms))
        return {{}, QStringLiteral("Waveform statistics exceed the numeric range.")};
    trace.statistics = stats;
    return {std::move(trace), {}};
}

} // namespace Waveform
