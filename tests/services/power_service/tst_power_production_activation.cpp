// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_logind_service.h"
#include "support/fake_ppd_service.h"
#include "support/fake_upower_service.h"

#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/power_protocol/power_dbus.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest>

#include <csignal>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

bool isExactServiceProcess(const pid_t pid)
{
    const QString expected =
        QFileInfo(QStringLiteral(QINDAQT_POWER_SERVICE_EXECUTABLE)).canonicalFilePath();
    return !expected.isEmpty()
        && QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).canonicalFilePath()
               == expected;
}

void terminateExactServiceProcess(const pid_t pid)
{
    if (!isExactServiceProcess(pid)) {
        return;
    }
    ::kill(pid, SIGTERM);
    for (int attempt = 0; attempt < 100 && isExactServiceProcess(pid); ++attempt) {
        QTest::qSleep(20);
    }
    if (isExactServiceProcess(pid)) {
        ::kill(pid, SIGKILL);
        for (int attempt = 0; attempt < 100 && isExactServiceProcess(pid); ++attempt) {
            QTest::qSleep(20);
        }
    }
}

// One pinned-address private bus. The pinned address is exported to activated
// children as DBUS_SYSTEM_BUS_ADDRESS, so the production adapters inside the
// activated service reach only this bus and its fake services — never the
// host system bus. AGENT-GUARD: rows must never inherit an ambient host
// DBUS_SYSTEM_BUS_ADDRESS; the pinned value overwrites it in the daemon env.
class ProductionBus final
{
public:
    bool start(const QString &extraExecArguments)
    {
        if (!root.isValid()) {
            return false;
        }
        const QString serviceDir = root.filePath(QStringLiteral("share/dbus-1/services"));
        runtimeDir = root.filePath(QStringLiteral("runtime"));
        backlightRoot = root.filePath(QStringLiteral("backlight"));
        if (!QDir().mkpath(serviceDir) || !QDir().mkpath(runtimeDir)
            || !QDir().mkpath(backlightRoot + QStringLiteral("/panel"))) {
            return false;
        }
        QFile descriptor(serviceDir + QStringLiteral("/org.qindaqt.Power1.service"));
        if (!descriptor.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        descriptor.write("[D-BUS Service]\nName=org.qindaqt.Power1\nExec=");
        descriptor.write(QByteArray(QINDAQT_POWER_SERVICE_EXECUTABLE));
        QString effectiveArguments = extraExecArguments;
        if (effectiveArguments.contains(QStringLiteral("--upstream=production"))) {
            effectiveArguments += QStringLiteral(" --backlight-root=%1").arg(backlightRoot);
        }
        if (!effectiveArguments.isEmpty()) {
            descriptor.write(" ");
            descriptor.write(effectiveArguments.toUtf8());
        }
        descriptor.write("\n");
        descriptor.close();

        address = QStringLiteral("unix:abstract=qindaqt-pb2-%1")
                      .arg(QUuid::createUuid().toString(QUuid::Id128));
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("XDG_DATA_DIRS"),
                           root.filePath(QStringLiteral("share")));
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDir);
        environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), address);
        environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), address);
        daemon.setProcessEnvironment(environment);
        daemon.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
        daemon.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                             QStringLiteral("--nopidfile"),
                             QStringLiteral("--address=%1").arg(address),
                             QStringLiteral("--print-address=1")});
        daemon.start();
        if (!daemon.waitForStarted(5000) || !daemon.waitForReadyRead(5000)) {
            return false;
        }
        return QString::fromUtf8(daemon.readLine()).trimmed().startsWith(
            address + QStringLiteral(",guid="));
    }

    ~ProductionBus()
    {
        if (servicePid > 0) {
            terminateExactServiceProcess(servicePid);
        }
        if (!connectionName.isEmpty()) {
            QDBusConnection::disconnectFromBus(connectionName);
        }
        for (const QString &open : openedConnections) {
            QDBusConnection::disconnectFromBus(open);
        }
        if (daemon.state() == QProcess::NotRunning) {
            return;
        }
        daemon.terminate();
        if (!daemon.waitForFinished(2000)) {
            daemon.kill();
            daemon.waitForFinished();
        }
    }

    QDBusConnection openConnection(const QString &suffix)
    {
        const QString name = QStringLiteral("qindaqt-production-fake-%1-%2")
                                 .arg(suffix, QUuid::createUuid().toString(
                                                  QUuid::Id128));
        const QDBusConnection opened = QDBusConnection::connectToBus(address, name);
        if (opened.isConnected()) {
            openedConnections.push_back(name);
        }
        return opened;
    }

    QTemporaryDir root{QStringLiteral(QINDAQT_TEST_SCRATCH_DIR)
                        + QStringLiteral("/production-XXXXXX")};
    QString runtimeDir;
    QString backlightRoot;
    QString address;
    QProcess daemon;
    QString connectionName;
    QDBusConnection connection{QStringLiteral("invalid")};
    QStringList openedConnections;
    pid_t servicePid = 0;
};

void writeBacklightFixture(const QString &root)
{
    auto write = [root](const QString &name, const QByteArray &content) {
        QFile file(root + QLatin1Char('/') + name);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }
        file.write(content);
        file.close();
    };
    write(QStringLiteral("panel/type"), QByteArrayLiteral("firmware\n"));
    write(QStringLiteral("panel/max_brightness"), QByteArrayLiteral("255\n"));
    write(QStringLiteral("panel/brightness"), QByteArrayLiteral("128\n"));
    write(QStringLiteral("panel/actual_brightness"), QByteArrayLiteral("129\n"));
}

FakeUpowerService::DeviceSpec productionBattery()
{
    QVariantMap properties;
    properties.insert(QStringLiteral("Type"), QVariant(uint(2)));
    properties.insert(QStringLiteral("IsPresent"), QVariant(true));
    properties.insert(QStringLiteral("State"), QVariant(uint(2)));
    properties.insert(QStringLiteral("Percentage"), QVariant(42.5));
    properties.insert(QStringLiteral("BatteryLevel"), QVariant(uint(1)));
    properties.insert(QStringLiteral("TimeToEmpty"), QVariant(qint64(3'600)));
    return {QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0"),
            properties};
}

void activateAndWaitForOwner(const QDBusConnection &connection, pid_t *ownerPid)
{
    *ownerPid = 0;
    pid_t resolved = 0;
    QDBusMessage start = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("StartServiceByName"));
    start.setArguments({QString::fromLatin1(kServiceName), quint32(0)});
    QDBusPendingCallWatcher activation(connection.asyncCall(start));
    QSignalSpy activated(&activation, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE_WITH_TIMEOUT(activated.size(), 1, 5000);
    const QDBusPendingReply<quint32> activationReply = activation;
    if (activationReply.isError()) {
        return;
    }
    QDBusMessage pidCall = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("GetConnectionUnixProcessID"));
    pidCall.setArguments({QString::fromLatin1(kServiceName)});
    const QDBusMessage pidReply = connection.call(pidCall);
    if (pidReply.type() == QDBusMessage::ReplyMessage
        && !pidReply.arguments().isEmpty()) {
        const pid_t pid = static_cast<pid_t>(pidReply.arguments().constFirst().toUInt());
        if (isExactServiceProcess(pid)) {
            resolved = pid;
        }
    }
    *ownerPid = resolved;
}

} // namespace

class PowerProductionActivationTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void productionModePublishesFakeUpstreamTruth();
    void productionModeWithoutFakesStaysHonest();
    void defaultModeKeepsUnavailableTruth();
    void invalidModeExitsFailClosed();
    void packagedDescriptorSelectsProduction();
};

void PowerProductionActivationTests::productionModePublishesFakeUpstreamTruth()
{
    registerDBusTypes();
    ProductionBus bus;
    QVERIFY(bus.start(QStringLiteral("--upstream=production")));
    writeBacklightFixture(bus.backlightRoot);
    bus.connectionName =
        QStringLiteral("qindaqt-production-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    bus.connection = QDBusConnection::connectToBus(bus.address, bus.connectionName);
    QVERIFY(bus.connection.isConnected());

    const QDBusConnection upowerConnection = bus.openConnection(QStringLiteral("upower"));
    FakeUpowerService upower(upowerConnection);
    QVERIFY(upower.registerService());
    upower.setDevices({productionBattery()});
    upower.setOnBattery(true);
    const QDBusConnection profilesConnection =
        bus.openConnection(QStringLiteral("profiles"));
    FakePpdService profiles(profilesConnection, false);
    QVERIFY(profiles.registerService());
    profiles.setProfiles({QStringLiteral("power-saver"), QStringLiteral("balanced"),
                          QStringLiteral("performance")});
    profiles.setActiveProfile(QStringLiteral("balanced"));
    const QDBusConnection logindConnection = bus.openConnection(QStringLiteral("logind"));
    FakeLogindService logind(logindConnection);
    QVERIFY(logind.registerService());
    logind.setSessionTruth(false, false, false);

    activateAndWaitForOwner(bus.connection, &bus.servicePid);
    QVERIFY(bus.servicePid > 0);

    QtPowerTransport transport(bus.connection);
    PowerClient client(&transport);
    client.start();
    QTRY_VERIFY_WITH_TIMEOUT(client.hasSnapshot(), 10'000);
    const Snapshot snapshot = client.snapshot();
    QCOMPARE(snapshot.availability, Availability::Ready);
    QVERIFY(snapshot.capabilities.testFlag(Capability::Supplies));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Profiles));
    QVERIFY(snapshot.capabilities.testFlag(Capability::Inhibitors));
    QVERIFY(snapshot.capabilities.testFlag(Capability::InternalBacklight));
    QCOMPARE(snapshot.supplies.size(), 1);
    QCOMPARE(snapshot.profiles.supported.size(), 3);
    QVERIFY(snapshot.source.onBattery);
    QCOMPARE(snapshot.internalBacklights.size(), 1);
    QCOMPARE(snapshot.internalBacklights.constFirst().deviceName,
             QStringLiteral("panel"));
    QCOMPARE(snapshot.internalBacklights.constFirst().maximum, quint32(255));
    QVERIFY(snapshot.epoch != 0);
    client.stop();
}

void PowerProductionActivationTests::productionModeWithoutFakesStaysHonest()
{
    registerDBusTypes();
    ProductionBus bus;
    QVERIFY(bus.start(QStringLiteral("--upstream=production")));
    bus.connectionName =
        QStringLiteral("qindaqt-production-bare-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    bus.connection = QDBusConnection::connectToBus(bus.address, bus.connectionName);
    QVERIFY(bus.connection.isConnected());

    activateAndWaitForOwner(bus.connection, &bus.servicePid);
    QVERIFY(bus.servicePid > 0);

    QtPowerTransport transport(bus.connection);
    PowerClient client(&transport);
    client.start();
    QTRY_VERIFY_WITH_TIMEOUT(client.hasSnapshot(), 10'000);
    const Snapshot snapshot = client.snapshot();
    // With no upstream daemons on the private system bus, every bus domain is
    // unavailable; the backlight rows fall with the battery domain instead of
    // publishing device truth without AC/on-battery authority.
    QCOMPARE(snapshot.availability, Availability::Unavailable);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("upower-unavailable"));
    QCOMPARE(snapshot.supplies.size(), 0);
    QCOMPARE(snapshot.internalBacklights.size(), 0);
    QVERIFY(!snapshot.capabilities.testFlag(Capability::InternalBacklight));
    QVERIFY(snapshot.epoch != 0);
    client.stop();
}

void PowerProductionActivationTests::defaultModeKeepsUnavailableTruth()
{
    registerDBusTypes();
    ProductionBus bus;
    QVERIFY(bus.start(QString()));
    bus.connectionName =
        QStringLiteral("qindaqt-production-default-%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
    bus.connection = QDBusConnection::connectToBus(bus.address, bus.connectionName);
    QVERIFY(bus.connection.isConnected());

    // Even with fake upstream services present, an unconfigured descriptor
    // must not contact them: the binary default stays the PB-1 truth.
    FakeUpowerService upower(bus.connection);
    QVERIFY(upower.registerService());
    upower.setDevices({productionBattery()});
    upower.setOnBattery(true);

    activateAndWaitForOwner(bus.connection, &bus.servicePid);
    QVERIFY(bus.servicePid > 0);

    QtPowerTransport transport(bus.connection);
    PowerClient client(&transport);
    client.start();
    QTRY_VERIFY_WITH_TIMEOUT(client.hasSnapshot(), 10'000);
    const Snapshot snapshot = client.snapshot();
    QCOMPARE(snapshot.availability, Availability::Unavailable);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("upstream-not-integrated"));
    QCOMPARE(snapshot.capabilities, Capabilities{});
    client.stop();
}

void PowerProductionActivationTests::invalidModeExitsFailClosed()
{
    QProcess process;
    process.setProgram(QStringLiteral(QINDAQT_POWER_SERVICE_EXECUTABLE));
    process.setArguments({QStringLiteral("--upstream=banana")});
    process.start();
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 1);
}

void PowerProductionActivationTests::packagedDescriptorSelectsProduction()
{
    QFile descriptor(QStringLiteral(QINDAQT_DBUS_SERVICE_FILE));
    QVERIFY(descriptor.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(descriptor.readAll());
    descriptor.close();
    QVERIFY(content.contains(QStringLiteral("/qindaqt-power-service --upstream=production\n")));
    QVERIFY(content.contains(QStringLiteral("SystemdService=qindaqt-power-service.service")));

    QFile unit(QStringLiteral(QINDAQT_SYSTEMD_UNIT_FILE));
    QVERIFY(unit.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString unitContent = QString::fromUtf8(unit.readAll());
    unit.close();
    QVERIFY(unitContent.contains(
        QStringLiteral("/qindaqt-power-service --upstream=production")));
}

QTEST_GUILESS_MAIN(PowerProductionActivationTests)
#include "tst_power_production_activation.moc"
