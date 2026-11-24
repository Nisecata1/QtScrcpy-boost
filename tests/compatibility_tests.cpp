#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QtEndian>

#include <cstring>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include "adbprocess.h"
#include "controller.h"
#include "controlmsg.h"
#include "devicemsg.h"
#include "keymap.h"
#include "recorder.h"
#include "server.h"
#include "videobuffer.h"
#include "videosocket.h"

namespace {
const QString kTestSerial = QStringLiteral("compatibility-test-device");

void sendMouse(Controller &controller, QEvent::Type type, const QPointF &position = QPointF(100, 200),
               Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const bool pressed = type != QEvent::MouseButtonRelease;
    const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent event(type, position, position, button, pressed ? Qt::LeftButton : Qt::NoButton, modifiers);
    controller.mouseEvent(&event, QSize(1000, 2000), QSize(500, 1000));
    QCoreApplication::sendPostedEvents(&controller);
}

QByteArray buildDeviceInfoHeader(const QString &deviceName, quint32 codecId, quint32 width, quint32 height)
{
    QByteArray header(76, '\0');
    const QByteArray utf8Name = deviceName.toUtf8();
    const int nameCopyLen = qMin(63, utf8Name.size());
    std::memcpy(header.data(), utf8Name.constData(), static_cast<size_t>(nameCopyLen));
    qToBigEndian<quint32>(codecId, reinterpret_cast<uchar *>(header.data() + 64));
    qToBigEndian<quint32>(width, reinterpret_cast<uchar *>(header.data() + 68));
    qToBigEndian<quint32>(height, reinterpret_cast<uchar *>(header.data() + 72));
    return header;
}

AVPacket *makeH264ConfigPacket()
{
    // Minimal Annex-B SPS + PPS NAL units for MP4/MKV header creation.
    static const quint8 kSpsPps[] = {
        0x00, 0x00, 0x00, 0x01,
        0x67, 0x42, 0xC0, 0x1E, 0xD9, 0x00, 0xA0, 0x47, 0xFE, 0xC8,
        0x00, 0x00, 0x00, 0x01,
        0x68, 0xCE, 0x38, 0x80
    };
    AVPacket *pkt = av_packet_alloc();
    if (!pkt || av_new_packet(pkt, static_cast<int>(sizeof(kSpsPps))) != 0) {
        av_packet_free(&pkt);
        return nullptr;
    }
    std::memcpy(pkt->data, kSpsPps, sizeof(kSpsPps));
    pkt->pts = AV_NOPTS_VALUE;
    pkt->dts = AV_NOPTS_VALUE;
    pkt->flags = 0;
    return pkt;
}

AVPacket *makeH264MediaPacket(qint64 ptsUs, bool keyFrame)
{
    static const quint8 kSlice[] = {
        0x00, 0x00, 0x00, 0x01,
        0x65, 0x88, 0x84, 0x21, 0xA0, 0x01, 0x02, 0x03
    };
    AVPacket *pkt = av_packet_alloc();
    if (!pkt || av_new_packet(pkt, static_cast<int>(sizeof(kSlice))) != 0) {
        av_packet_free(&pkt);
        return nullptr;
    }
    std::memcpy(pkt->data, kSlice, sizeof(kSlice));
    pkt->pts = ptsUs;
    pkt->dts = ptsUs;
    pkt->flags = keyFrame ? AV_PKT_FLAG_KEY : 0;
    return pkt;
}

int runAdbProbe(const QStringList &arguments)
{
    qsc::AdbProcess::setAdbPath(arguments.at(2));
    const QString selected = qsc::AdbProcess::getAdbPath();
    if (!QDir::setCurrent(arguments.at(3)) || qsc::AdbProcess::getAdbPath() != selected) {
        return 1;
    }

    QTextStream output(stdout);
    output << (selected.isEmpty() ? QStringLiteral("<missing>") : selected) << '\n';
    if (selected.isEmpty()) {
        return 0;
    }

    // Execute the copied test helper, never a real ADB daemon or phone command.
    qsc::AdbProcess process;
    QSignalSpy results(&process, &qsc::AdbProcess::adbProcessResult);
    process.execute(kTestSerial, {QStringLiteral("--mock-adb")});
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 5000) {
        for (const auto &result : results) {
            const auto status = qvariant_cast<qsc::AdbProcess::ADB_EXEC_RESULT>(result.at(0));
            if (status == qsc::AdbProcess::AER_SUCCESS_EXEC) {
                output << process.getStdOut() << '\n';
                return 0;
            }
            if (status != qsc::AdbProcess::AER_SUCCESS_START) {
                return 2;
            }
        }
        QTest::qWait(10);
    }
    return 3;
}
} // namespace

class CompatibilityTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(!QFileInfo::exists(QCoreApplication::applicationDirPath() + "/config/userdata.ini"));
    }

    void adbSelection_data()
    {
        QTest::addColumn<QString>("scenario");
        for (const QString &scenario : {QStringLiteral("environment-first"), QStringLiteral("custom-fallback"),
                                       QStringLiteral("directory-is-not-adb"), QStringLiteral("beside-executable"),
                                       QStringLiteral("relative-path-with-spaces"), QStringLiteral("custom-only"),
                                       QStringLiteral("missing-all")}) {
            QTest::newRow(qPrintable(scenario)) << scenario;
        }
    }

    void adbSelection()
    {
        QFETCH(QString, scenario);
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        QDir root(fixture.path());
        QVERIFY(root.mkpath("application"));
        QVERIFY(root.mkpath("environment adb"));
        QVERIFY(root.mkpath("custom adb"));
        QVERIFY(root.mkpath("other working directory"));
#ifdef Q_OS_WIN
        const QString executableSuffix = QStringLiteral(".exe");
#else
        const QString executableSuffix;
#endif
        const QString helper = root.filePath("application/path_probe" + executableSuffix);
        const QString environmentAdb = root.filePath("environment adb/adb" + executableSuffix);
        const QString customAdb = root.filePath("custom adb/adb" + executableSuffix);
        const QString adjacentAdb = root.filePath("application/adb" + executableSuffix);
        const QString currentExecutable = QCoreApplication::applicationFilePath();
        QVERIFY(QFile::copy(currentExecutable, helper));

        QString environmentPath = root.filePath("missing/adb" + executableSuffix);
        QString configuredPath;
        QString expected;
        if (scenario != "missing-all") {
            QVERIFY(QFile::copy(currentExecutable, adjacentAdb));
            expected = adjacentAdb;
        }
        if (scenario == "environment-first" || scenario == "relative-path-with-spaces") {
            QVERIFY(QFile::copy(currentExecutable, environmentAdb));
            QVERIFY(QFile::copy(currentExecutable, customAdb));
            environmentPath = scenario == "relative-path-with-spaces" ? root.relativeFilePath(environmentAdb) : environmentAdb;
            configuredPath = customAdb;
            expected = environmentAdb;
        } else if (scenario == "custom-fallback" || scenario == "directory-is-not-adb" || scenario == "custom-only") {
            QVERIFY(QFile::copy(currentExecutable, customAdb));
            configuredPath = customAdb;
            expected = customAdb;
            if (scenario == "directory-is-not-adb") {
                environmentPath = root.filePath("environment adb");
            } else if (scenario == "custom-only") {
                environmentPath.clear();
            }
        }

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QTSCRCPY_ADB_PATH", environmentPath);
        QProcess child;
        child.setProcessEnvironment(environment);
        child.setWorkingDirectory(fixture.path());
        child.start(helper, {QStringLiteral("--adb-probe"), configuredPath, root.filePath("other working directory")});
        QVERIFY2(child.waitForStarted(5000), qPrintable(child.errorString()));
        QVERIFY2(child.waitForFinished(10000), qPrintable(child.errorString()));
        const QByteArray error = child.readAllStandardError();
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QVERIFY2(child.exitCode() == 0, error.constData());
        const QStringList lines = QString::fromUtf8(child.readAllStandardOutput()).trimmed().split('\n');
        QCOMPARE(lines.first().trimmed(), expected.isEmpty() ? QStringLiteral("<missing>") : QDir::cleanPath(expected));
        if (!expected.isEmpty()) {
            QCOMPARE(lines.size(), 2);
            QCOMPARE(lines.at(1).trimmed(), QStringLiteral("mock-adb-ok"));
        }
    }

    void adbPathCacheResetAndMultiLineDevicesOutput()
    {
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        QDir root(fixture.path());
        QVERIFY(root.mkpath("adb1"));
        QVERIFY(root.mkpath("adb2"));
#ifdef Q_OS_WIN
        const QString executableSuffix = QStringLiteral(".exe");
#else
        const QString executableSuffix;
#endif
        const QString adb1 = root.filePath("adb1/adb" + executableSuffix);
        const QString adb2 = root.filePath("adb2/adb" + executableSuffix);
        const QString currentExecutable = QCoreApplication::applicationFilePath();
        QVERIFY(QFile::copy(currentExecutable, adb1));
        QVERIFY(QFile::copy(currentExecutable, adb2));

        const QByteArray prevEnv = qgetenv("QTSCRCPY_ADB_PATH");
        qunsetenv("QTSCRCPY_ADB_PATH");

        qsc::AdbProcess::setAdbPath(adb1);
        QCOMPARE(qsc::AdbProcess::getAdbPath(), QDir::cleanPath(adb1));

        qsc::AdbProcess::setAdbPath(adb2);
        QCOMPARE(qsc::AdbProcess::getAdbPath(), QDir::cleanPath(adb2));

        qsc::AdbProcess process;
        QSignalSpy results(&process, &qsc::AdbProcess::adbProcessResult);
        process.execute(QString(), {QStringLiteral("--mock-adb-devices")});
        QTRY_VERIFY_WITH_TIMEOUT(!results.isEmpty(), 5000);
        QElapsedTimer elapsed;
        elapsed.start();
        bool finished = false;
        while (elapsed.elapsed() < 5000 && !finished) {
            for (const auto &entry : results) {
                const auto st = qvariant_cast<qsc::AdbProcess::ADB_EXEC_RESULT>(entry.at(0));
                if (st == qsc::AdbProcess::AER_SUCCESS_EXEC) {
                    finished = true;
                    break;
                }
            }
            if (!finished) {
                QTest::qWait(10);
            }
        }
        QVERIFY(finished);
        const QStringList serials = process.getDevicesSerialFromStdOut();
        QCOMPARE(serials, (QStringList{QStringLiteral("SERIAL_ONE"), QStringLiteral("192.168.1.25:5555")}));

        qsc::AdbProcess::setAdbPath(QString());
        if (!prevEnv.isEmpty()) {
            qputenv("QTSCRCPY_ADB_PATH", prevEnv);
        }
    }

    void mouseSequences_data()
    {
        QTest::addColumn<bool>("compatEnabled");
        QTest::addColumn<int>("holdMs");
        QTest::addColumn<QString>("scenario");
        for (int profile = 0; profile < 4; ++profile) {
            const bool enabled = profile != 0;
            const int hold = profile == 1 ? 0 : (profile == 3 ? 40 : 16);
            for (const QString &scenario : {QStringLiteral("single"), QStringLiteral("double"), QStringLiteral("rapid-triple"),
                                           QStringLiteral("drag"), QStringLiteral("double-then-drag")}) {
                const QByteArray name = QString("%1-compat%2-hold%3").arg(scenario).arg(enabled).arg(hold).toLatin1();
                QTest::newRow(name.constData()) << enabled << hold << scenario;
            }
        }
    }

    void mouseSequences()
    {
        QFETCH(bool, compatEnabled);
        QFETCH(int, holdMs);
        QFETCH(QString, scenario);
        QTemporaryDir configuration;
        QVERIFY(configuration.isValid());
        qputenv("QTSCRCPY_CONFIG_PATH", configuration.path().toLocal8Bit());
        QSettings settings(configuration.filePath("userdata.ini"), QSettings::IniFormat);
        settings.beginGroup(kTestSerial);
        settings.setValue("NormalMouseCompatEnabled", compatEnabled);
        settings.setValue("NormalMouseTapMinHoldMs", holdMs);
        settings.setValue("RemoteCursorEnabled", false);
        settings.endGroup();
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);

        QList<QByteArray> packets;
        Controller controller([&packets](const QByteArray &packet) -> qint64 {
            packets.append(packet);
            return packet.size();
        }, kTestSerial);

        QVector<int> expected{0, 1};
        sendMouse(controller, QEvent::MouseButtonPress);
        if (scenario == "drag") {
            sendMouse(controller, QEvent::MouseMove, QPointF(150, 250));
            expected = {0, 2, 1};
        }
        sendMouse(controller, QEvent::MouseButtonRelease);
        if (scenario == "double" || scenario == "rapid-triple" || scenario == "double-then-drag") {
            sendMouse(controller, QEvent::MouseButtonDblClick);
            expected << 0;
            if (scenario == "double-then-drag") {
                sendMouse(controller, QEvent::MouseMove, QPointF(150, 250));
                expected << 2;
            }
            sendMouse(controller, QEvent::MouseButtonRelease);
            expected << 1;
        }
        if (scenario == "rapid-triple") {
            sendMouse(controller, QEvent::MouseButtonDblClick);
            sendMouse(controller, QEvent::MouseButtonRelease);
            expected << 0 << 1;
        }
        QTRY_COMPARE_WITH_TIMEOUT(packets.size(), expected.size(), 1000);

        QVector<int> actions;
        for (const QByteArray &packet : packets) {
            QCOMPARE(packet.size(), 32);
            QCOMPARE(static_cast<quint8>(packet.at(0)), quint8(2));
            actions << static_cast<quint8>(packet.at(1));
            QCOMPARE(qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(packet.constData() + 2)), quint64(-2));
            QCOMPARE(qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(packet.constData() + 18)), quint16(1000));
            QCOMPARE(qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(packet.constData() + 20)), quint16(2000));
        }
        QCOMPARE(actions, expected);
        QTest::qWait(holdMs + 10);
        QCoreApplication::sendPostedEvents(&controller);
        QCOMPARE(packets.size(), expected.size());
    }

    void nonLeftDoubleClickIsIgnored()
    {
        QList<QByteArray> packets;
        Controller controller([&packets](const QByteArray &packet) -> qint64 {
            packets.append(packet);
            return packet.size();
        }, kTestSerial);
        QMouseEvent event(QEvent::MouseButtonDblClick, QPointF(100, 200), QPointF(100, 200),
                          Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        controller.mouseEvent(&event, QSize(1000, 2000), QSize(500, 1000));
        QCoreApplication::sendPostedEvents(&controller);
        QVERIFY(packets.isEmpty());
    }

    void normalTapHoldNotFlushedByMouseMoveAndCtrlPinch()
    {
        QTemporaryDir configuration;
        QVERIFY(configuration.isValid());
        qputenv("QTSCRCPY_CONFIG_PATH", configuration.path().toLocal8Bit());
        QSettings settings(configuration.filePath("userdata.ini"), QSettings::IniFormat);
        settings.beginGroup(kTestSerial);
        settings.setValue("NormalMouseCompatEnabled", true);
        settings.setValue("NormalMouseTapMinHoldMs", 35);
        settings.setValue("RemoteCursorEnabled", false);
        settings.endGroup();
        settings.sync();

        QList<QByteArray> packets;
        Controller controller([&packets](const QByteArray &packet) -> qint64 {
            packets.append(packet);
            return packet.size();
        }, kTestSerial);

        // Quick tap: MouseMove hover jitter right after UP must NOT prematurely flush min-hold UP.
        sendMouse(controller, QEvent::MouseButtonPress, QPointF(100, 200));
        sendMouse(controller, QEvent::MouseButtonRelease, QPointF(100, 200));
        QCOMPARE(packets.size(), 1);
        QMouseEvent hoverMove(QEvent::MouseMove, QPointF(102, 202), QPointF(102, 202),
                              Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        controller.mouseEvent(&hoverMove, QSize(1000, 2000), QSize(500, 1000));
        QCoreApplication::sendPostedEvents(&controller);
        QCOMPARE(packets.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(packets.size(), 2, 1000);
        QCOMPARE(static_cast<quint8>(packets.at(0).at(1)), quint8(0));
        QCOMPARE(static_cast<quint8>(packets.at(1).at(1)), quint8(1));

        packets.clear();

        // Ctrl + left-drag pinch-to-zoom should emit POINTER_ID_VIRTUAL_FINGER (-4) alongside primary touch (-2).
        sendMouse(controller, QEvent::MouseButtonPress, QPointF(100, 200), Qt::ControlModifier);
        sendMouse(controller, QEvent::MouseMove, QPointF(150, 250), Qt::ControlModifier);
        sendMouse(controller, QEvent::MouseButtonRelease, QPointF(150, 250), Qt::ControlModifier);
        QTRY_COMPARE_WITH_TIMEOUT(packets.size(), 6, 1000);

        int virtualFingerPackets = 0;
        for (const QByteArray &packet : packets) {
            const quint64 pointerId = qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(packet.constData() + 2));
            if (pointerId == static_cast<quint64>(-4)) {
                ++virtualFingerPackets;
            }
        }
        QCOMPARE(virtualFingerPackets, 3);
    }

    void controlMsgSerializationAndUtf8Truncation()
    {
        // 1. Empty clipboard should zero-init sequence = 0 and paste flag properly.
        {
            ControlMsg msg(ControlMsg::CMT_SET_CLIPBOARD);
            QString empty;
            msg.setSetClipboardMsgData(empty, false);
            const QByteArray data = msg.serializeData();
            QCOMPARE(data.size(), 14);
            QCOMPARE(static_cast<quint8>(data.at(0)), quint8(ControlMsg::CMT_SET_CLIPBOARD));
            QCOMPARE(qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(data.constData() + 1)), quint64(0));
            QCOMPARE(static_cast<quint8>(data.at(9)), quint8(0));
            QCOMPARE(qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + 10)), quint32(0));
        }

        // 2. Controller::requestDeviceClipboard should initialize GCCK_NONE (0).
        {
            QList<QByteArray> packets;
            Controller controller([&packets](const QByteArray &packet) -> qint64 {
                packets.append(packet);
                return packet.size();
            }, kTestSerial);
            controller.requestDeviceClipboard();
            QCoreApplication::sendPostedEvents(&controller);
            QCOMPARE(packets.size(), 1);
            QCOMPARE(packets.at(0).size(), 2);
            QCOMPARE(static_cast<quint8>(packets.at(0).at(0)), quint8(ControlMsg::CMT_GET_CLIPBOARD));
            QCOMPARE(static_cast<quint8>(packets.at(0).at(1)), quint8(ControlMsg::GCCK_NONE));
        }

        // 3. CJK text exceeding CONTROL_MSG_INJECT_TEXT_MAX_LENGTH bytes must truncate cleanly on UTF-8 boundaries.
        {
            ControlMsg msg(ControlMsg::CMT_INJECT_TEXT);
            QString longCjk(200, QChar(0x4E2D)); // 600 bytes UTF-8
            msg.setInjectTextMsgData(longCjk);
            const QByteArray data = msg.serializeData();
            const quint32 utf8Len = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + 1));
            QVERIFY(utf8Len <= CONTROL_MSG_INJECT_TEXT_MAX_LENGTH);
            QCOMPARE(utf8Len % 3, quint32(0));
            const QByteArray utf8Payload = data.mid(5, static_cast<int>(utf8Len));
            const QString decoded = QString::fromUtf8(utf8Payload);
            QVERIFY(!decoded.contains(QChar(0xFFFD)));
            QCOMPARE(decoded.toUtf8(), utf8Payload);
        }
    }

    void deviceMsgDeserialization()
    {
        // DMT_GET_CLIPBOARD
        {
            DeviceMsg msg;
            QByteArray raw;
            raw.append(static_cast<char>(DeviceMsg::DMT_GET_CLIPBOARD));
            const QByteArray hello = QByteArray("hello-clip");
            uchar lenBuf[4];
            qToBigEndian<quint32>(static_cast<quint32>(hello.size()), lenBuf);
            raw.append(reinterpret_cast<const char *>(lenBuf), 4);
            raw.append(hello);
            QCOMPARE(msg.deserialize(raw), 5 + hello.size());
            QCOMPARE(msg.type(), DeviceMsg::DMT_GET_CLIPBOARD);
            QString out;
            msg.getClipboardMsgData(out);
            QCOMPARE(out, QStringLiteral("hello-clip"));
        }

        // DMT_ACK_CLIPBOARD (exact 9 bytes) and partial packet (5 bytes -> 0)
        {
            DeviceMsg partialMsg;
            QByteArray partial(5, '\0');
            partial[0] = static_cast<char>(DeviceMsg::DMT_ACK_CLIPBOARD);
            QCOMPARE(partialMsg.deserialize(partial), 0);

            DeviceMsg ackMsg;
            QByteArray ack(9, '\0');
            ack[0] = static_cast<char>(DeviceMsg::DMT_ACK_CLIPBOARD);
            QCOMPARE(ackMsg.deserialize(ack), 9);
            QCOMPARE(ackMsg.type(), DeviceMsg::DMT_ACK_CLIPBOARD);
        }

        // DMT_UHID_OUTPUT
        {
            DeviceMsg uhidMsg;
            QByteArray raw(5, '\0');
            raw[0] = static_cast<char>(DeviceMsg::DMT_UHID_OUTPUT);
            qToBigEndian<quint16>(1, reinterpret_cast<uchar *>(raw.data() + 1));
            qToBigEndian<quint16>(3, reinterpret_cast<uchar *>(raw.data() + 3));
            QCOMPARE(uhidMsg.deserialize(raw), 0);
            raw.append(QByteArray("abc"));
            QCOMPARE(uhidMsg.deserialize(raw), 8);
            QCOMPARE(uhidMsg.type(), DeviceMsg::DMT_UHID_OUTPUT);
        }

        // Oversized clipboard length must return -1 without allocating or crashing
        {
            DeviceMsg oversized;
            QByteArray raw(5, '\0');
            raw[0] = static_cast<char>(DeviceMsg::DMT_GET_CLIPBOARD);
            qToBigEndian<quint32>(static_cast<quint32>(DEVICE_MSG_TEXT_MAX_LENGTH) + 100u,
                                  reinterpret_cast<uchar *>(raw.data() + 1));
            QCOMPARE(oversized.deserialize(raw), -1);
        }
    }

    void recorderLifecycleAndConfigOnlyStop()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // (a) Config + 2 media packets
        {
            const QString mp4Path = tempDir.filePath(QStringLiteral("valid_recording.MP4"));
            Recorder recorder(mp4Path);
            recorder.setFrameSize(QSize(720, 1280));
            QVERIFY2(recorder.open(), qPrintable(recorder.lastError()));
            QVERIFY2(recorder.startRecorder(), qPrintable(recorder.lastError()));

            AVPacket *cfg = makeH264ConfigPacket();
            AVPacket *f1 = makeH264MediaPacket(1000, true);
            AVPacket *f2 = makeH264MediaPacket(34333, false);
            QVERIFY(cfg && f1 && f2);
            QVERIFY(recorder.push(cfg));
            QVERIFY(recorder.push(f1));
            QVERIFY(recorder.push(f2));
            av_packet_free(&cfg);
            av_packet_free(&f1);
            av_packet_free(&f2);

            recorder.stopRecorder();
            QVERIFY(recorder.wait(5000));
            recorder.close();
            QFileInfo info(mp4Path);
            QVERIFY(info.exists());
            QVERIFY(info.size() > 0);
        }

        // (b) Config-only immediate stop (regression for double-free + m_mutex deadlock + AV_NOPTS_VALUE)
        {
            const QString mp4ConfigOnlyPath = tempDir.filePath(QStringLiteral("config_only.mp4"));
            Recorder recorder(mp4ConfigOnlyPath);
            recorder.setFrameSize(QSize(720, 1280));
            QVERIFY2(recorder.open(), qPrintable(recorder.lastError()));
            QVERIFY2(recorder.startRecorder(), qPrintable(recorder.lastError()));

            AVPacket *cfg = makeH264ConfigPacket();
            QVERIFY(cfg);
            QVERIFY(recorder.push(cfg));
            av_packet_free(&cfg);

            QTest::qWait(15);
            recorder.stopRecorder();
            QVERIFY(recorder.wait(5000));
            recorder.close();
        }
    }

    void videoBufferEmptyFrameDoesNotDeadlock()
    {
        VideoBuffer vb;
        QVERIFY(vb.init());

        bool invoked = false;
        vb.peekRenderedFrame([&invoked](int, int, uint8_t *) {
            invoked = true;
        });
        QVERIFY(!invoked);

        // Verify m_mutex was released even on empty frame by locking from another thread.
        QThread worker;
        bool acquiredFromOtherThread = false;
        QObject ctx;
        ctx.moveToThread(&worker);
        QObject::connect(&worker, &QThread::started, &ctx, [&]() {
            vb.lock();
            acquiredFromOtherThread = true;
            vb.unLock();
            worker.quit();
        }, Qt::DirectConnection);
        worker.start();
        QVERIFY(worker.wait(2000));
        QVERIFY(acquiredFromOtherThread);
        vb.deInit();
    }

    void keyMapReloadAndValidation()
    {
        KeyMap keyMap;

        // 1. Valid script with Key_A click node
        const QString script1 = QStringLiteral(R"({
            "switchKey": "Key_QuoteLeft",
            "mouseMoveMap": {
                "startPos": {"x": 0.5, "y": 0.5},
                "speedRatio": 2.0
            },
            "keyMapNodes": [
                {
                    "type": "KMT_CLICK",
                    "key": "Key_A",
                    "pos": {"x": 0.2, "y": 0.3},
                    "switchMap": false
                }
            ]
        })");
        keyMap.loadKeyMap(script1);
        QVERIFY(keyMap.isValidMouseMoveMap());
        QCOMPARE(keyMap.getMouseMoveMap().data.mouseMove.smallEyes.type, KeyMap::AT_INVALID);
        QCOMPARE(keyMap.getMouseMoveMap().data.mouseMove.smallEyes.key, static_cast<int>(Qt::Key_unknown));
        QCOMPARE(keyMap.getKeyMapNodeKey(Qt::Key_A).type, KeyMap::KMT_CLICK);

        // 2. Reload script without Key_A and with speedRatioY < 0.001 (must reset state and reject mouseMoveMap)
        const QString script2 = QStringLiteral(R"({
            "switchKey": "Key_Tab",
            "mouseMoveMap": {
                "startPos": {"x": 0.5, "y": 0.5},
                "speedRatioX": 1.0,
                "speedRatioY": 0.0005
            },
            "keyMapNodes": []
        })");
        keyMap.loadKeyMap(script2);
        QVERIFY(!keyMap.isValidMouseMoveMap());
        QCOMPARE(keyMap.getKeyMapNodeKey(Qt::Key_A).type, KeyMap::KMT_INVALID);
    }

    void serverReadInfoExact76ByteHeader()
    {
        QTcpServer serverSocket;
        QVERIFY(serverSocket.listen(QHostAddress::LocalHost, 0));
        const quint16 port = serverSocket.serverPort();

        Server server;

        // Subcase 1: exact 76-byte single packet (previously blocked and had commented-out width)
        {
            VideoSocket client;
            client.connectToHost(QHostAddress::LocalHost, port);
            QVERIFY(client.waitForConnected(2000));
            QVERIFY(serverSocket.waitForNewConnection(2000));
            QTcpSocket *peer = serverSocket.nextPendingConnection();
            QVERIFY(peer);

            const QByteArray header = buildDeviceInfoHeader(QStringLiteral("PixelTest333"), 0x68323634U, 1080, 2400);
            QCOMPARE(header.size(), 76);
            peer->write(header);
            peer->flush();

            QString name;
            QSize size;
            QVERIFY(server.readInfo(&client, name, size));
            QCOMPARE(name, QStringLiteral("PixelTest333"));
            QCOMPARE(size, QSize(1080, 2400));
            peer->close();
            peer->deleteLater();
        }

        // Subcase 2: fragmented header (40 bytes then 36 bytes) + trailing H.264 bytes left unread
        {
            QThread readerThread;
            QString name;
            QSize size;
            bool readOk = false;
            QByteArray trailingBytes;

            QObject *worker = new QObject();
            worker->moveToThread(&readerThread);
            QObject::connect(&readerThread, &QThread::started, worker, [&]() {
                VideoSocket client;
                client.connectToHost(QHostAddress::LocalHost, port);
                if (client.waitForConnected(2000)) {
                    readOk = server.readInfo(&client, name, size);
                    trailingBytes = client.readAll();
                }
                readerThread.quit();
            });
            readerThread.start();

            QVERIFY(serverSocket.waitForNewConnection(2000));
            QTcpSocket *peer = serverSocket.nextPendingConnection();
            QVERIFY(peer);

            const QByteArray header = buildDeviceInfoHeader(QStringLiteral("SplitHeader"), 0x68323634U, 1920, 1080);
            peer->write(header.left(40));
            peer->flush();
            QTest::qWait(30);
            peer->write(header.mid(40));
            peer->write("ABCD"); // stream payload must stay in socket buffer
            peer->flush();

            QVERIFY(readerThread.wait(3000));
            delete worker;

            QVERIFY(readOk);
            QCOMPARE(name, QStringLiteral("SplitHeader"));
            QCOMPARE(size, QSize(1920, 1080));
            QCOMPARE(trailingBytes, QByteArray("ABCD"));
            peer->close();
            peer->deleteLater();
        }

        // Subcase 3: truncated header followed by disconnect aborts cleanly before timeout
        {
            VideoSocket client;
            client.connectToHost(QHostAddress::LocalHost, port);
            QVERIFY(client.waitForConnected(2000));
            QVERIFY(serverSocket.waitForNewConnection(2000));
            QTcpSocket *peer = serverSocket.nextPendingConnection();
            QVERIFY(peer);

            peer->write(QByteArray(20, 'x'));
            peer->flush();
            peer->disconnectFromHost();
            QTest::qWait(20);

            QElapsedTimer timer;
            timer.start();
            QString name;
            QSize size;
            QVERIFY(!server.readInfo(&client, name, size));
            QVERIFY(timer.elapsed() < 2000);
            peer->deleteLater();
        }
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.contains("--mock-adb")) {
        QTextStream(stdout) << "mock-adb-ok\n";
        return 0;
    }
    if (arguments.contains("--mock-adb-devices")) {
        QTextStream(stdout) << "List of devices attached\r\nSERIAL_ONE\tdevice\r\n192.168.1.25:5555\tdevice\r\nOFFLINE_DEV\toffline\r\n";
        return 0;
    }
    if (arguments.size() == 4 && arguments.at(1) == "--adb-probe") {
        return runAdbProbe(arguments);
    }
    CompatibilityTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "compatibility_tests.moc"
