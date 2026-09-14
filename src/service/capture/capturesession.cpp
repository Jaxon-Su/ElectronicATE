#include "capturesession.h"
#include <QDebug>
#include <utility>

bool CaptureSession::isBusy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_busy;
}

bool CaptureSession::acquire()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_closed || m_busy) return false;
    m_busy = true;
    return true;
}

void CaptureSession::closeWhenIdle(std::function<void()> cleanup)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_closed) return;
        m_closed = true;
        if (m_busy) {
            m_cleanup = std::move(cleanup);
            return;
        }
    }
    runCleanup(std::move(cleanup));
}

void CaptureSession::release() noexcept
{
    std::function<void()> cleanup;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_busy = false;
        cleanup = std::move(m_cleanup);
    }
    runCleanup(std::move(cleanup));
}

void CaptureSession::runCleanup(std::function<void()> cleanup) noexcept
{
    if (!cleanup) return;
    try { cleanup(); }
    catch (...) { qWarning() << "[CaptureSession] deferred cleanup failed"; }
}
