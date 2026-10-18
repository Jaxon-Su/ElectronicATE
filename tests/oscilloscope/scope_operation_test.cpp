#include "scopeoperationrunner.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QSemaphore>
#include <iostream>
#include <stdexcept>
#include <cmath>
struct Result { QString error; int value = 0; };
void require(bool ok, const char *error) { if (!ok) throw std::runtime_error(error); }
class UnsupportedScope : public Oscilloscope {
public:
    QString model() const override { return "unsupported"; }
    QString vendor() const override { return "offline"; }
    QByteArray captureScreenshot(const QString &, const QString &) override { return {}; }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        UnsupportedScope scope;
        bool failed=false;try { scope.run(); } catch(const std::logic_error &) { failed=true; }
        require(failed,"unsupported Run silently succeeded");
        failed=false;try { scope.isClipping(1); } catch(const std::logic_error &) { failed=true; }
        require(failed && !scope.waitForOperationComplete(1),"unsupported measurement silently succeeded");
        require(std::isnan(scope.getTriggerLevel()) && scope.lastError().contains("unsupported", Qt::CaseInsensitive), "unsupported trigger level disguised as zero");
        require(std::isnan(scope.getChannelScale(1)) && std::isnan(scope.getTimebase()), "unsupported scale disguised as zero");
        require(std::isnan(scope.getHorizontalPosition()) && std::isnan(scope.getChannelPosition(1)), "unsupported position disguised as zero");
        require(!scope.isRunning() && !scope.lastError().isEmpty(), "unsupported state query disguised as stopped");
        ScopeOperationRunner runner;
        auto lease=std::make_shared<int>(1);
        bool delivered=false;QEventLoop loop;
        auto complete=[&](const Result &result) { require(result.error.contains("failed"),"error lost");delivered=true;loop.quit(); };
        require(!runner.submit<Result>({},[]{return Result{};},complete),"missing lease accepted");
        require(runner.submit<Result>(lease,[]()->Result { throw std::runtime_error("failed"); },complete),"first command rejected");
        require(!runner.submit<Result>(lease,[]{return Result{};},complete),"overlapping I/O accepted");
        QTimer::singleShot(2000,&loop,&QEventLoop::quit);loop.exec();
        require(delivered && !runner.isBusy(),"completion did not release busy state");
        auto pendingLease=std::make_shared<int>(2);std::weak_ptr<int> weak=pendingLease;
        QSemaphore started,finish;bool stale=false;
        auto *pending=new ScopeOperationRunner;
        require(pending->submit<Result>(pendingLease,[&]{started.release();finish.acquire();return Result{};},[&](const Result &){stale=true;}),"pending command rejected");
        require(started.tryAcquire(1,2000),"work did not start");
        pendingLease.reset();delete pending;
        require(!weak.expired(),"instrument lease released during I/O");
        finish.release();
        for(int i=0;i<200 && !weak.expired();++i){QCoreApplication::processEvents();QThread::msleep(1);}
        QCoreApplication::processEvents();
        require(weak.expired() && !stale,"destroyed owner received completion or leaked lease");
        return 0;
    }catch(const std::exception &exception){std::cerr<<exception.what();return 1;}
}
