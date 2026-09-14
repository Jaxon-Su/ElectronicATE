#include "instrumentoperationrunner.h"
#include <QFutureWatcher>
#include <QPromise>
#include <QThread>
#include <QThreadPool>
#include <memory>
#include <exception>
#include <utility>

void runInstrumentOperation(QObject* owner,
    std::function<InstrumentOperationResult()> work,
    std::function<void(const InstrumentOperationResult&)> completion)
{
    Q_ASSERT(owner && owner->thread() == QThread::currentThread());
    if (!owner) return;
    auto promise = std::make_shared<QPromise<InstrumentOperationResult>>();
    auto* watcher = new QFutureWatcher<InstrumentOperationResult>(owner);
    QObject::connect(watcher, &QFutureWatcher<InstrumentOperationResult>::finished,
        owner, [watcher, completion = std::move(completion)] {
            const auto result = watcher->result();
            watcher->deleteLater();
            if (completion) completion(result);
        });
    promise->start();
    watcher->setFuture(promise->future());
    QThreadPool::globalInstance()->start([promise, work = std::move(work)] {
        InstrumentOperationResult result;
        try { result = work(); }
        catch (const std::exception& error) { result = {false, QString::fromUtf8(error.what())}; }
        catch (...) { result = {false, QObject::tr("Unknown instrument operation error.")}; }
        promise->addResult(result);
        promise->finish();
    });
}
