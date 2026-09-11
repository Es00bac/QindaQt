// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/night_light/night_light_config_port.h>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUuid>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusMetaType>

#include <memory>
#include <QtTest>

using namespace QindaQt::Services::NightLight;

namespace {

NightLightSettings exampleSettings()
{
    NightLightSettings settings;
    settings.output.active = true;
    settings.output.mode = Mode::DarkLight;
    settings.output.dayTemperatureKelvin = 6500;
    settings.output.nightTemperatureKelvin = 3400;
    settings.schedule.source = ScheduleSource::Location;
    settings.schedule.automaticLocation = false;
    settings.schedule.latitudeDegrees = 52.5;
    settings.schedule.longitudeDegrees = 13.25;
    settings.schedule.sunriseStart = QTime(6, 30, 0);
    settings.schedule.sunsetStart = QTime(19, 45, 0);
    settings.schedule.transitionSeconds = 2400;
    return settings;
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(content.toUtf8()) == content.toUtf8().size();
}

QDBusConnection offlineBus()
{
    return QDBusConnection(QStringLiteral("none"));
}

// Private dbus-daemon: announcement rows never touch a real session bus.
struct PrivateBus {
    QProcess process;
    QString address;
    QString name;

    bool start()
    {
        process.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
        process.setArguments({ QStringLiteral("--session"),
                               QStringLiteral("--nofork"),
                               QStringLiteral("--nopidfile"),
                               QStringLiteral("--print-address=1") });
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("night-light-config-port-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        return QDBusConnection::connectToBus(address, name).isConnected();
    }

    QDBusConnection connection() const { return QDBusConnection(name); }

    ~PrivateBus()
    {
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }
};

} // namespace

// Records ConfigChanged announcements per object path, decoded the way
// KConfigWatcher decodes them.
class ConfigChangeRecorder final : public QObject {
    Q_OBJECT

public:
    QHash<QString, QList<QHash<QString, QByteArrayList>>> byPath;

public Q_SLOTS:
    void record(const QDBusMessage &message)
    {
        if (message.arguments().size() != 1) {
            return;
        }
        byPath[message.path()].append(
            qdbus_cast<QHash<QString, QByteArrayList>>(message.arguments().at(0)));
    }
};

class NightLightConfigPortTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void absentFilesReadAsDefaults();
    void writeThenReadRoundTrip();
    void unchangedWriteTouchesNoFile();
    void unrelatedGroupsAndKeysSurvive();
    void hostileStoredValuesFailClosedAndConverge();
    void invalidWriteIsRefused();
    void externalChangeIsReportedAndSelfWritesAreNot();
    void writesAreAnnouncedToConfigWatchers();

private Q_SLOTS:
    // Each test function gets fresh files: QSettings-style state leaking
    // between test functions would turn write-outcome assertions into
    // order-dependent guesses.
    void init()
    {
        m_directory = std::make_unique<QTemporaryDir>();
        QVERIFY(m_directory->isValid());
    }

private:
    QString kwinPath() const
    {
        return m_directory->filePath(QStringLiteral("kwinrc"));
    }
    QString knightPath() const
    {
        return m_directory->filePath(QStringLiteral("knighttimerc"));
    }

    std::unique_ptr<QTemporaryDir> m_directory;
};

void NightLightConfigPortTests::absentFilesReadAsDefaults()
{
    QtConfigNightLightPort port(kwinPath(), knightPath(), offlineBus());
    const NightLightConfigPort::ReadResult result = port.read();
    QCOMPARE(result.outcome, NightLightConfigPort::ReadOutcome::Absent);
    QCOMPARE(result.values, NightLightSettings{});
    QVERIFY(result.diagnostic.isEmpty());
}

void NightLightConfigPortTests::writeThenReadRoundTrip()
{
    QtConfigNightLightPort port(kwinPath(), knightPath(), offlineBus());
    const NightLightSettings settings = exampleSettings();
    const NightLightConfigPort::WriteResult write = port.write(settings);
    QCOMPARE(write.outcome, NightLightConfigPort::WriteOutcome::Applied);

    const NightLightConfigPort::ReadResult read = port.read();
    QCOMPARE(read.outcome, NightLightConfigPort::ReadOutcome::Loaded);
    QCOMPARE(read.values, settings);

    // The persisted shapes are exactly the KConfig dialect: group names,
    // choice-name enums, "HH:mm:ss" times, plain integers. Values that equal
    // the schema defaults stay unwritten: a fresh profile must not grow
    // redundant keys.
    const QString kwin = readFile(kwinPath());
    QVERIFY(kwin.contains(QLatin1String("[NightColor]")));
    QVERIFY(kwin.contains(QLatin1String("Active=true")));
    QVERIFY(kwin.contains(QLatin1String("NightTemperature=3400")));
    QVERIFY(!kwin.contains(QLatin1String("Mode=")));
    QVERIFY(!kwin.contains(QLatin1String("DayTemperature=")));

    const QString knight = readFile(knightPath());
    // Source=Location equals the schema default: the group stays unwritten.
    QVERIFY(!knight.contains(QLatin1String("Source=")));
    QVERIFY(knight.contains(QLatin1String("[Location]")));
    QVERIFY(knight.contains(QLatin1String("Automatic=false")));
    QVERIFY(knight.contains(QLatin1String("Latitude=52.5")));
    QVERIFY(knight.contains(QLatin1String("Longitude=13.25")));
    QVERIFY(knight.contains(QLatin1String("[Times]")));
    QVERIFY(knight.contains(QLatin1String("SunriseStart=06:30:00")));
    QVERIFY(knight.contains(QLatin1String("SunsetStart=19:45:00")));
    QVERIFY(knight.contains(QLatin1String("TransitionDuration=2400")));

    // A second draft changes every output value; the file converges and the
    // read-back still equals the draft.
    NightLightSettings changed = settings;
    changed.schedule.source = ScheduleSource::Times;
    changed.output.mode = Mode::Constant;
    changed.output.dayTemperatureKelvin = 6000;
    changed.output.nightTemperatureKelvin = 4500;
    changed.output.active = false;
    QCOMPARE(port.write(changed).outcome,
             NightLightConfigPort::WriteOutcome::Applied);
    const QString kwinAfter = readFile(kwinPath());
    QVERIFY(kwinAfter.contains(QLatin1String("Active=false")));
    QVERIFY(kwinAfter.contains(QLatin1String("Mode=Constant")));
    QVERIFY(kwinAfter.contains(QLatin1String("DayTemperature=6000")));
    QVERIFY(kwinAfter.contains(QLatin1String("NightTemperature=4500")));
    const QString knightAfter = readFile(knightPath());
    QVERIFY(knightAfter.contains(QLatin1String("[General]")));
    QVERIFY(knightAfter.contains(QLatin1String("Source=Times")));
    QCOMPARE(port.read().values, changed);
}

void NightLightConfigPortTests::unchangedWriteTouchesNoFile()
{
    QtConfigNightLightPort port(kwinPath(), knightPath(), offlineBus());
    QCOMPARE(port.write(exampleSettings()).outcome,
             NightLightConfigPort::WriteOutcome::Applied);
    const QString kwinBefore = readFile(kwinPath());
    const QString knightBefore = readFile(knightPath());

    const NightLightConfigPort::WriteResult write =
        port.write(exampleSettings());
    QCOMPARE(write.outcome, NightLightConfigPort::WriteOutcome::Unchanged);
    QVERIFY(write.diagnostic.isEmpty());
    QCOMPARE(readFile(kwinPath()), kwinBefore);
    QCOMPARE(readFile(knightPath()), knightBefore);
}

void NightLightConfigPortTests::unrelatedGroupsAndKeysSurvive()
{
    writeFile(kwinPath(), QStringLiteral(
                              "[NightColor]\n"
                              "Active=false\n"
                              "NightTemperature=4500\n"
                              "[Windows]\n"
                              "Placement=Smart\n"));
    writeFile(knightPath(), QStringLiteral(
                                "[General]\n"
                                "Source=Times\n"
                                "[Vendor]\n"
                                "Keep=yes\n"));

    NightLightSettings settings = exampleSettings();
    settings.output.mode = Mode::Constant;
    settings.schedule.source = ScheduleSource::Times;
    QtConfigNightLightPort port(kwinPath(), knightPath(), offlineBus());
    QCOMPARE(port.write(settings).outcome,
             NightLightConfigPort::WriteOutcome::Applied);

    const QString kwin = readFile(kwinPath());
    QVERIFY(kwin.contains(QLatin1String("[Windows]")));
    QVERIFY(kwin.contains(QLatin1String("Placement=Smart")));
    QVERIFY(kwin.contains(QLatin1String("Mode=Constant")));
    const QString knight = readFile(knightPath());
    QVERIFY(knight.contains(QLatin1String("[Vendor]")));
    QVERIFY(knight.contains(QLatin1String("Keep=yes")));
    QCOMPARE(port.read().values, settings);
}

void NightLightConfigPortTests::hostileStoredValuesFailClosedAndConverge()
{
    // Out-of-bounds temperature.
    writeFile(kwinPath(), QStringLiteral(
                              "[NightColor]\n"
                              "Active=true\n"
                              "Mode=DarkLight\n"
                              "NightTemperature=20000\n"));
    writeFile(knightPath(), QStringLiteral(
                                "[General]\n"
                                "Source=Location\n"
                                "[Location]\n"
                                "Automatic=true\n"));
    QtConfigNightLightPort port(kwinPath(), knightPath(), offlineBus());
    NightLightConfigPort::ReadResult read = port.read();
    QCOMPARE(read.outcome, NightLightConfigPort::ReadOutcome::Failed);
    QVERIFY(!read.diagnostic.isEmpty());

    // The write replaces the whole owned key set with validated values, so a
    // hostile file converges instead of keeping poisoned entries behind a
    // partial merge.
    QCOMPARE(port.write(exampleSettings()).outcome,
             NightLightConfigPort::WriteOutcome::Applied);
    read = port.read();
    QCOMPARE(read.outcome, NightLightConfigPort::ReadOutcome::Loaded);
    QCOMPARE(read.values, exampleSettings());
    QVERIFY(readFile(kwinPath()).contains(QLatin1String("NightTemperature=3400")));

    // A foreign enum token fails the same way (pre-6.6 kwinrc leftovers).
    writeFile(kwinPath(), QStringLiteral(
                              "[NightColor]\n"
                              "Active=true\n"
                              "Mode=Automatic\n"
                              "NightTemperature=3400\n"));
    read = port.read();
    QCOMPARE(read.outcome, NightLightConfigPort::ReadOutcome::Failed);

    // A legacy "hhmm" time is not a "HH:mm:ss" time: fail closed.
    writeFile(knightPath(), QStringLiteral(
                                "[General]\n"
                                "Source=Times\n"
                                "[Times]\n"
                                "SunriseStart=0630\n"
                                "SunsetStart=1945\n"
                                "TransitionDuration=2400\n"));
    read = port.read();
    QCOMPARE(read.outcome, NightLightConfigPort::ReadOutcome::Failed);
}

void NightLightConfigPortTests::invalidWriteIsRefused()
{
    writeFile(kwinPath(), QStringLiteral("[NightColor]\nActive=true\n"));
    const QString kwinBefore = readFile(kwinPath());

    NightLightSettings hostile = exampleSettings();
    hostile.output.nightTemperatureKelvin = 999;
    QtConfigNightLightPort port(kwinPath(), knightPath(), offlineBus());
    const NightLightConfigPort::WriteResult write = port.write(hostile);
    QCOMPARE(write.outcome, NightLightConfigPort::WriteOutcome::Failed);
    QVERIFY(!write.diagnostic.isEmpty());
    QCOMPARE(readFile(kwinPath()), kwinBefore);
    QVERIFY(!QFileInfo::exists(knightPath()));
}

void NightLightConfigPortTests::externalChangeIsReportedAndSelfWritesAreNot()
{
    QtConfigNightLightPort port(kwinPath(), knightPath(), offlineBus());
    QSignalSpy external(&port, &NightLightConfigPort::changedExternally);

    // The port's own write is an echo, never an external change.
    QCOMPARE(port.write(exampleSettings()).outcome,
             NightLightConfigPort::WriteOutcome::Applied);
    QTest::qWait(300);
    QCOMPARE(external.count(), 0);

    // An outside writer with a different value is external intent.
    writeFile(knightPath(), QStringLiteral(
                                "[General]\n"
                                "Source=Times\n"
                                "[Times]\n"
                                "SunriseStart=07:00:00\n"));
    QTRY_VERIFY_WITH_TIMEOUT(external.count() >= 1, 5000);

    QTest::qWait(200);
    const qsizetype observed = external.count();
    QTest::qWait(300);
    QCOMPARE(external.count(), observed);
}

void NightLightConfigPortTests::writesAreAnnouncedToConfigWatchers()
{
    qDBusRegisterMetaType<QByteArrayList>();
    qDBusRegisterMetaType<QHash<QString, QByteArrayList>>();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString listenerName = QStringLiteral("night-light-watcher");
    QDBusConnection listener = QDBusConnection::connectToBus(bus.address, listenerName);
    ConfigChangeRecorder recorder;
    QVERIFY(listener.connect(QString(), QString(),
                             QStringLiteral("org.kde.kconfig.notify"),
                             QStringLiteral("ConfigChanged"), &recorder,
                             SLOT(record(QDBusMessage))));

    QtConfigNightLightPort port(kwinPath(), knightPath(), bus.connection());
    QCOMPARE(port.write(exampleSettings()).outcome,
             NightLightConfigPort::WriteOutcome::Applied);
    QTRY_COMPARE(recorder.byPath.value(QStringLiteral("/kwinrc")).size(), 1);
    const QByteArrayList kwinKeys = recorder.byPath.value(QStringLiteral("/kwinrc"))
                                        .first()
                                        .value(QStringLiteral("NightColor"));
    QVERIFY(kwinKeys.contains("Active"));
    QVERIFY(kwinKeys.contains("NightTemperature"));
    QVERIFY(QFileInfo::exists(knightPath()));
    QTRY_COMPARE(recorder.byPath.value(QStringLiteral("/knighttimerc")).size(), 1);

    // Negative control: an unchanged write touches no file and announces nothing.
    QCOMPARE(port.write(exampleSettings()).outcome,
             NightLightConfigPort::WriteOutcome::Unchanged);
    QTest::qWait(200);
    QCOMPARE(recorder.byPath.value(QStringLiteral("/kwinrc")).size(), 1);
    QDBusConnection::disconnectFromBus(listenerName);
}

QTEST_MAIN(NightLightConfigPortTests)
#include "tst_night_light_config_port.moc"
