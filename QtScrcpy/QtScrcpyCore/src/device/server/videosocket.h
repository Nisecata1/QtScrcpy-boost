#ifndef VIDEOSOCKET_H
#define VIDEOSOCKET_H

#include <atomic>
#include <QTcpSocket>

class VideoSocket : public QTcpSocket
{
    Q_OBJECT
public:
    explicit VideoSocket(QObject *parent = nullptr);
    virtual ~VideoSocket();

    qint32 subThreadRecvData(quint8 *buf, qint32 bufSize);
    void requestStop();
    void resetStop();

private:
    std::atomic_bool m_stopped{false};
};

#endif // VIDEOSOCKET_H
