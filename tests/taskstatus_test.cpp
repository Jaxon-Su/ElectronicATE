#include "taskstatus.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <iostream>

class StatusSource : public QObject {
    Q_OBJECT
signals:
    void statusChanged(int row, TaskStatus status);
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    qRegisterMetaType<TaskStatus>("TaskStatus");
    QThread thread;
    auto* source = new StatusSource;
    source->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, source, &QObject::deleteLater);
    QEventLoop loop;
    int received = 0;
    bool valid = true;
    const TaskStatus expected[] = {TaskStatus::Idle, TaskStatus::Running, TaskStatus::Pass, TaskStatus::Fail};
    QObject::connect(source, &StatusSource::statusChanged, &loop,
        [&](int row, TaskStatus status) {
            valid = valid && QThread::currentThread() == app.thread()
                    && row == received && received < 4 && status == expected[received];
            if (++received == 4) loop.quit();
        }, Qt::QueuedConnection);
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    thread.start();
    QMetaObject::invokeMethod(source, [source, &expected] {
        for (int i = 0; i < 4; ++i) emit source->statusChanged(i, expected[i]);
    }, Qt::QueuedConnection);
    loop.exec();
    thread.quit();
    thread.wait();
    if (!valid || received != 4) {
        std::cerr << "Task status queued delivery failed\n";
        return 1;
    }
    std::cout << "PASS: all task states delivered across threads using QtCore only\n";
    return 0;
}

#include "taskstatus_test.moc"
