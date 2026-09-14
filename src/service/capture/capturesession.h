#pragma once
#include <functional>
#include <mutex>

class CaptureLease;

// Shared lifetime boundary between the ViewModel and work that can outlive it.
class CaptureSession final {
public:
    bool isBusy() const;
    // Permanently reject new leases. Run cleanup once after the last lease exits,
    // on the closing thread if idle or the releasing thread if work is active.
    void closeWhenIdle(std::function<void()> cleanup);

private:
    friend class CaptureLease;
    bool acquire();
    void release() noexcept;
    static void runCleanup(std::function<void()> cleanup) noexcept;
    mutable std::mutex m_mutex;
    bool m_busy = false;
    bool m_closed = false;
    std::function<void()> m_cleanup;
};
