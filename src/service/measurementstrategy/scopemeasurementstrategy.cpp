#include "scopemeasurementstrategy.h"
#include <stdexcept>
#include <utility>

ScopeMeasurementStrategy::ScopeMeasurementStrategy(
    std::unique_ptr<IOscilloscopeMeasureStrategy> algorithm, IScopeMeasurement &scope)
    : m_algorithm(std::move(algorithm)), m_scope(scope)
{
    if (!m_algorithm) throw std::invalid_argument("Missing oscilloscope strategy");
}

QString ScopeMeasurementStrategy::name() const { return m_algorithm->name(); }

MeasurementResult ScopeMeasurementStrategy::execute(QAtomicInt &stop)
{
    const auto values = m_algorithm->execute(&m_scope, stop);
    MeasurementResult result;
    result.success = values.success;
    result.cleanupFailed = values.cleanupFailed;
    result.error = values.errorMessage;
    result.channels.reserve(values.channels.size());
    for (const auto &channel : values.channels)
        result.channels.append({channel.channel, {}, channel.maxVal, channel.minVal,
                                channel.rms, channel.mean});
    // Regular algorithms do not return units. Keep them unknown; no added scope queries.
    return result;
}
