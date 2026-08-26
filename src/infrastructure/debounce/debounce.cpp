#include "debounce.h"
#include <QTimer>

Debounce::Debounce(int delayMs, QObject* parent)
    : QObject(parent), m_delayMs(delayMs)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &Debounce::fired);
}

void Debounce::schedule()
{
    m_timer->start(m_delayMs);
}

void Debounce::cancel()
{
    m_timer->stop();
}

bool Debounce::isPending() const
{
    return m_timer->isActive();
}
