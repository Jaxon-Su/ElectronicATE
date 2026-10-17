#pragma once
#include "instrumentsetupconfig.h"
#include <optional>
#include <utility>

// Owner-thread state: coalesce pending settings while one snapshot is in flight.
// The caller supplies external blockers such as an active capture.
class PendingConfigUpdate {
  public:
    void enqueue(const InstrumentSetupConfig& config) { m_pending = config; }
    bool hasPending() const { return m_pending.has_value(); }
    bool isRunning() const { return m_running; }

    std::optional<InstrumentSetupConfig> tryStart(bool blocked)
    {
        if (blocked || m_running || !m_pending)
            return std::nullopt;
        auto snapshot = std::move(m_pending);
        m_pending.reset();
        m_running = true;
        return snapshot;
    }

    // Call for both success and failure. New settings survive either outcome.
    void complete() { m_running = false; }

  private:
    std::optional<InstrumentSetupConfig> m_pending;
    bool m_running = false;
};
