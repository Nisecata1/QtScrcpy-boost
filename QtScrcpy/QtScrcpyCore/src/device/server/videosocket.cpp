#include <QCoreApplication>
#include <QDebug>
#include <QThread>

#include "videosocket.h"

VideoSocket::VideoSocket(QObject *parent) : QTcpSocket(parent)
{
}

VideoSocket::~VideoSocket()
{
}

void VideoSocket::requestStop()
{
    m_stopped.store(true, std::memory_order_release);
}

void VideoSocket::resetStop()
{
    m_stopped.store(false, std::memory_order_release);
}

qint32 VideoSocket::subThreadRecvData(quint8 *buf, qint32 bufSize)
{
    if (!buf || bufSize <= 0) {
        return 0;
    }
    // this function cant call in main thread
    Q_ASSERT(QCoreApplication::instance()->thread() != QThread::currentThread());

    while (bytesAvailable() < bufSize) {
        if (m_stopped.load(std::memory_order_acquire)) {
            return 0;
        }
        if (!waitForReadyRead(200)) {
            if (m_stopped.load(std::memory_order_acquire)) {
                return 0;
            }
            if (state() != QAbstractSocket::ConnectedState && bytesAvailable() < bufSize) {
                return 0;
            }
        }
    }

    if (m_stopped.load(std::memory_order_acquire)) {
        return 0;
    }

    // recv data
    return static_cast<qint32>(read(reinterpret_cast<char *>(buf), bufSize));
}
