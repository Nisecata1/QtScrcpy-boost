#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QtEndian>

#include "adbprocess.h"
#include "controller.h"

namespace {
const QString kTestSerial = QStringLiteral("compatibility-test-device");

void sendMouse(Controller &controller, QEvent::Type type, const QPointF &position = QPointF(100, 200))
{
    const bool pressed = type != QEvent::MouseButtonRelease;
    const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent event(type, position, position, button, pressed ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    controller.mouseEvent(&event, QSize(1000, 2000), QSize(500, 1000));
    QCoreApplication::sendPostedEvents(&controller);
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
};

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.contains("--mock-adb")) {
        QTextStream(stdout) << "mock-adb-ok\n";
        return 0;
    }
    if (arguments.size() == 4 && arguments.at(1) == "--adb-probe") {
        return runAdbProbe(arguments);
    }
    CompatibilityTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "compatibility_tests.moc"
