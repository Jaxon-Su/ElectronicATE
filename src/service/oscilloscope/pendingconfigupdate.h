#pragma once
#include "page1config.h"
#include <optional>
#include <utility>

// Owner-thread state: coalesce pending settings while one snapshot is in flight.
// The caller supplies external blockers such as an active capture.
class PendingConfigUpdate {
public:
    void enqueue(const Page1Config& config) { m_pending = config; }
    bool hasPending() const { return m_pending.has_value(); }
    bool isRunning() const { return m_running; }

    std::optional<Page1Config> tryStart(bool blocked)
    {
        if (blocked || m_running || !m_pending) return std::nullopt;
        auto snapshot = std::move(m_pending);
        m_pending.reset();
        m_running = true;
        return snapshot;
    }

    // Call for both success and failure. New settings survive either outcome.
    void complete() { m_running = false; }

private:
    std::optional<Page1Config> m_pending;
    bool m_running = false;
};
