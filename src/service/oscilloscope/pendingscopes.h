#pragma once
#include "oscilloscopemanager.h"
#include <utility>

// Own connections returned by a worker until a live consumer adopts them.
// Shared through QFuture; only the completion handler calls take().
class PendingScopes {
public:
    explicit PendingScopes(OscilloscopeManager::OscMap scopes) : m_scopes(std::move(scopes)) {}
    PendingScopes(const PendingScopes&) = delete;
    PendingScopes& operator=(const PendingScopes&) = delete;
    ~PendingScopes()
    {
        if (!m_scopes.isEmpty()) OscilloscopeManager::disconnectAll(m_scopes);
    }
    OscilloscopeManager::OscMap take() { return std::exchange(m_scopes, {}); }
private:
    OscilloscopeManager::OscMap m_scopes;
};
