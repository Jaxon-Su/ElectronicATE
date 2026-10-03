#pragma once
#include <QObject>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <functional>
#include <memory>
#include <stdexcept>

// Serial admission on the owning thread. Work owns its lease until I/O returns;
// destroying the runner discards delivery without destroying an in-use instrument.
class ScopeOperationRunner : public QObject
{
  public:
    explicit ScopeOperationRunner(QObject *parent = nullptr) : QObject(parent) {}
    bool isBusy() const { return m_busy; }

    template <class Result, class Work, class Completion>
    bool submit(std::shared_ptr<void> lease, Work work, Completion complete)
    {
        if (m_busy || !lease)
            return false;
        m_busy = true;
        auto *watcher = new QFutureWatcher<Result>(this);
        connect(watcher, &QFutureWatcher<Result>::finished, this,
                [this, watcher, complete = std::move(complete)] {
                    const auto result = watcher->result();
                    watcher->deleteLater();
                    m_busy = false;
                    complete(result);
                });
        watcher->setFuture(QtConcurrent::run([lease = std::move(lease), work = std::move(work)] {
            try {
                return work();
            } catch (const std::exception &exception) {
                Result result;
                result.error = QString::fromUtf8(exception.what());
                return result;
            } catch (...) {
                Result result;
                result.error = "Unexpected oscilloscope error";
                return result;
            }
        }));
        return true;
    }

  private:
    bool m_busy = false;
};
