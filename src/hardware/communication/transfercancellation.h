#pragma once
#include <QAtomicInt>

// A transfer borrows the caller's stop flag only on its executing thread.
// Writes and cleanup commands deliberately remain usable after cancellation.
class TransferCancellation final
{
  public:
    explicit TransferCancellation(const QAtomicInt &stop) : m_previous(s_stop) { s_stop = &stop; }
    ~TransferCancellation() { s_stop = m_previous; }
    TransferCancellation(const TransferCancellation &) = delete;
    TransferCancellation &operator=(const TransferCancellation &) = delete;
    static bool requested() { return s_stop && s_stop->loadAcquire(); }

  private:
    const QAtomicInt *m_previous;
    inline static thread_local const QAtomicInt *s_stop = nullptr;
};
