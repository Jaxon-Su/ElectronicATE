#pragma once
#include "manualcontrolviewmodel.h"
#include <QEventLoop>
#include <QTimer>

// Exercise the public queue/debounce path, without calling private implementation slots.
inline void startQueuedConfiguration(ManualControlViewModel *vm)
{
    QEventLoop loop;
    QObject::connect(vm, &ManualControlViewModel::controlActiveChanged, &loop,
                     [&loop](bool active) { if (active) loop.quit(); });
    QObject::connect(vm, &QObject::destroyed, &loop, &QEventLoop::quit);
    QTimer::singleShot(650, &loop, &QEventLoop::quit);
    loop.exec();
}
