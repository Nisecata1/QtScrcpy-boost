#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QByteArray>
#include <QProcess>
#include <QPointer>

class QAudioSink;
class QAudioOutput;
class QIODevice;
class QTcpSocket;
class AudioOutput : public QObject
{
    Q_OBJECT
public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput();

    bool start(const QString& serial, int port);
    void stop();
    void installonly(const QString& serial, int port);

private:
    bool runSndcpyProcess(const QString& serial, int port, bool wait = true);
    void startAudioOutput();
    void stopAudioOutput();
    void startRecvData(int port);
    void stopRecvData();

private:
    QPointer<QIODevice> m_outputDevice;
    QPointer<QTcpSocket> m_audioSocket;
    QProcess m_sndcpy;
    QByteArray m_buffer;
    bool m_running = false;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QAudioOutput* m_audioOutput = nullptr;
#else
    QAudioSink *m_audioSink = nullptr;
#endif
};

#endif // AUDIOOUTPUT_H
