#pragma once
#include "instrumentoperationrunner.h"
#include <QObject>
#include <QPointer>
#include <QThread>
#include <deque>

// Owner-thread FIFO. Pending work is discarded on destruction; active work
// keeps its captured inputs and finishes without delivering a stale callback.
class InstrumentOperationQueue : public QObject {
public:
    using Work = std::function<InstrumentOperationResult()>;
    using Completion = std::function<void(const InstrumentOperationResult&)>;

    void submit(Work work, Completion completion)
    {
        Q_ASSERT(thread() == QThread::currentThread());
        if (m_closed) return;
        m_pending.push_back({std::move(work), std::move(completion)});
        startNext();
    }

    void close()
    {
        Q_ASSERT(thread() == QThread::currentThread());
        m_closed = true;
        m_pending.clear();
    }

private:
    struct Entry { Work work; Completion completion; };
    std::deque<Entry> m_pending;
    bool m_running = false;
    bool m_closed = false;

    void startNext()
    {
        if (m_closed || m_running || m_pending.empty()) return;
        m_running = true;
        auto entry = std::move(m_pending.front());
        m_pending.pop_front();
        runInstrumentOperation(this, std::move(entry.work),
            [this, completion = std::move(entry.completion)](const InstrumentOperationResult& result) {
                QPointer<InstrumentOperationQueue> alive(this);
                if (m_closed) return;
                // Keep busy through callbacks, including reentrant submissions.
                if (completion) completion(result);
                if (!alive) return;
                m_running = false;
                startNext();
            });
    }
};
