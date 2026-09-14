#pragma once

#include "capturesession.h"
#include <memory>
#include <utility>

// One lease spans synchronous channel queries, file selection and async work.
// Copies of the shared handle keep the session busy until the final owner exits.
class CaptureLease final {
public:
    static std::shared_ptr<CaptureLease> tryAcquire(std::shared_ptr<CaptureSession> session)
    {
        if (!session) return {};
        // Allocate before acquiring so allocation failure cannot strand the session.
        auto lease = std::shared_ptr<CaptureLease>(new CaptureLease(std::move(session)));
        lease->m_acquired = lease->m_session->acquire();
        return lease->m_acquired ? lease : std::shared_ptr<CaptureLease>{};
    }

    ~CaptureLease()
    {
        if (m_acquired) m_session->release();
    }

    CaptureLease(const CaptureLease&) = delete;
    CaptureLease& operator=(const CaptureLease&) = delete;

private:
    explicit CaptureLease(std::shared_ptr<CaptureSession> session) : m_session(std::move(session)) {}
    std::shared_ptr<CaptureSession> m_session;
    bool m_acquired = false;
};
