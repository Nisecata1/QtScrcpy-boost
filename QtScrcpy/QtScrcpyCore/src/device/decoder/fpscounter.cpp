#include <QDebug>
#include <QTimerEvent>

#include "fpscounter.h"

FpsCounter::FpsCounter(QObject *parent) : QObject(parent) {}

FpsCounter::~FpsCounter() {}

void FpsCounter::start()
{
    resetCounter();
    startCounterTimer();
}

void FpsCounter::stop()
{
    stopCounterTimer();
    resetCounter();
}

bool FpsCounter::isStarted()
{
    return m_counterTimer.load(std::memory_order_acquire) != 0;
}

void FpsCounter::addRenderedFrame()
{
    m_rendered.fetch_add(1, std::memory_order_relaxed);
}

void FpsCounter::addSkippedFrame()
{
    m_skipped.fetch_add(1, std::memory_order_relaxed);
}

void FpsCounter::timerEvent(QTimerEvent *event)
{
    if (event && m_counterTimer.load(std::memory_order_acquire) == event->timerId()) {
        m_curRendered = m_rendered.exchange(0, std::memory_order_relaxed);
        m_curSkipped = m_skipped.exchange(0, std::memory_order_relaxed);
        emit updateFPS(m_curRendered);
        //qInfo("FPS:%d Discard:%d", m_curRendered, m_skipped);
    }
}

void FpsCounter::startCounterTimer()
{
    stopCounterTimer();
    m_counterTimer.store(startTimer(1000), std::memory_order_release);
}

void FpsCounter::stopCounterTimer()
{
    const qint32 timerId = m_counterTimer.exchange(0, std::memory_order_acq_rel);
    if (timerId != 0) {
        killTimer(timerId);
    }
}

void FpsCounter::resetCounter()
{
    m_rendered.store(0, std::memory_order_relaxed);
    m_skipped.store(0, std::memory_order_relaxed);
}
