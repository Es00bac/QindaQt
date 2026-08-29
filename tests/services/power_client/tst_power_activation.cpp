// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/power_protocol/power_dbus.h>
#include <qindaqt/services/power_protocol/power_limits.h>

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

namespace
{

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
    for (int attempt = 0; attempt < 50 && isExactServiceProcess(pid); ++attempt) {
        QTest::qSleep(20);
    }
    if (isExactServiceProcess(pid)) {
        ::kill(pid, SIGKILL);
        for (int attempt = 0; attempt < 50 && isExactServiceProcess(pid); ++attempt) {
            QTest::qSleep(20);
        }
    }
}

class PrivateActivatingBus final
{
public:
    bool start(const bool reserveUniqueOwner = false)
    {
        if (!root.isValid()) {
            return false;
        }
        const QString serviceDir = root.filePath(QStringLiteral("share/dbus-1/services"));
        const QString runtimeDir = root.filePath(QStringLiteral("runtime"));
        if (!QDir().mkpath(serviceDir) || !QDir().mkpath(runtimeDir)) {
            return false;
        }
        QFile descriptor(serviceDir + QStringLiteral("/org.qindaqt.Power1.service"));
        if (!descriptor.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        descriptor.write("[D-BUS Service]\nName=org.qindaqt.Power1\nExec=");
        descriptor.write(QByteArray(QINDAQT_POWER_SERVICE_EXECUTABLE));
        descriptor.write("\n");
        descriptor.close();

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("XDG_DATA_DIRS"),
                           root.filePath(QStringLiteral("share")));
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDir);
        daemon.setProcessEnvironment(environment);
        daemon.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
        daemon.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                             QStringLiteral("--nopidfile"),
                             QStringLiteral("--print-address=1")});
        daemon.start();
        if (!daemon.waitForStarted() || !daemon.waitForReadyRead(5000)) {
            return false;
        }
        address = QString::fromUtf8(daemon.readLine()).trimmed();
        connectionName = QStringLiteral("qindaqt-power-activation-%1")
                             .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, connectionName);
        if (address.isEmpty() || !connection.isConnected()) {
            return false;
        }
        if (reserveUniqueOwner) {
            peerConnectionName = connectionName + QStringLiteral("-peer");
            peerConnection = QDBusConnection::connectToBus(address, peerConnectionName);
            if (!peerConnection.isConnected()) {
                return false;
            }
        }
        return true;
    }

    ~PrivateActivatingBus()
    {
        if (servicePid > 0) {
            // AGENT-GUARD: Never signal a cached raw PID after a failed test;
            // Linux may already have reused it. Cleanup authority is limited
            // to the canonical executable built for this fixture.
            terminateExactServiceProcess(servicePid);
        }
        if (!connectionName.isEmpty()) {
            QDBusConnection::disconnectFromBus(connectionName);
        }
        if (!peerConnectionName.isEmpty()) {
            QDBusConnection::disconnectFromBus(peerConnectionName);
        }
        stopDaemon();
    }

    void stopDaemon()
    {
        if (daemon.state() == QProcess::NotRunning) {
            return;
        }
        daemon.terminate();
        if (!daemon.waitForFinished(2000)) {
            daemon.kill();
            daemon.waitForFinished();
        }
    }

    pid_t findServicePid()
    {
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("GetConnectionUnixProcessID"));
        call.setArguments({QString::fromLatin1(kServiceName)});
        const QDBusMessage reply = connection.call(call);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            return 0;
        }
        servicePid = static_cast<pid_t>(reply.arguments().constFirst().toUInt());
        if (!isExactServiceProcess(servicePid)) {
            servicePid = 0;
        }
        return servicePid;
    }

    QTemporaryDir root{QStringLiteral("/tmp/qindaqt-power-activation-XXXXXX")};
    QProcess daemon;
    QString address;
    QString connectionName;
    QString peerConnectionName;
    QDBusConnection connection{QStringLiteral("invalid")};
    QDBusConnection peerConnection{QStringLiteral("invalid-peer")};
    pid_t servicePid = 0;
};

bool processExists(const pid_t pid)
{
    return QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

} // namespace

class PowerActivationTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void activatedServicePublishesHonestUnavailableTruth();
    void daemonLossExitsAndReplacementStartsFresh();
};

void PowerActivationTests::activatedServicePublishesHonestUnavailableTruth()
{
    registerDBusTypes();
    PrivateActivatingBus bus;
    QVERIFY(bus.start());

    QDBusMessage start = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("StartServiceByName"));
    start.setArguments({QString::fromLatin1(kServiceName), quint32(0)});
    QDBusPendingCallWatcher activation(bus.connection.asyncCall(start));
    QSignalSpy activated(&activation, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE_WITH_TIMEOUT(activated.size(), 1, 5000);
    const QDBusPendingReply<quint32> activationReply = activation;
    QVERIFY2(!activationReply.isError(), qPrintable(activationReply.error().message()));

    QtPowerTransport transport(bus.connection);
    PowerClient client(&transport);
    client.start();
    QTRY_VERIFY_WITH_TIMEOUT(client.hasSnapshot(), 10'000);
    const Snapshot snapshot = client.snapshot();
    // PB-1 truth: the resident owns the name and speaks exact Power1, but no
    // host upstream is integrated, so the honest projection is Unavailable
    // with zero capabilities and a stable reason.
    QCOMPARE(snapshot.availability, Availability::Unavailable);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("upstream-not-integrated"));
    QCOMPARE(snapshot.capabilities, Capabilities{});
    QVERIFY(snapshot.epoch != 0);
    QVERIFY(client.owner().startsWith(QLatin1Char(':')));
    QVERIFY(bus.findServicePid() > 0);
    const pid_t pid = bus.servicePid;

    bus.stopDaemon();
    QTRY_VERIFY_WITH_TIMEOUT(!processExists(pid), 10'000);
    bus.servicePid = 0;
    client.stop();
}

void PowerActivationTests::daemonLossExitsAndReplacementStartsFresh()
{
    registerDBusTypes();
    PrivateActivatingBus firstBus;
    QVERIFY(firstBus.start());

    QDBusMessage start = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("StartServiceByName"));
    start.setArguments({QString::fromLatin1(kServiceName), quint32(0)});
    QDBusPendingCallWatcher firstActivation(firstBus.connection.asyncCall(start));
    QSignalSpy firstActivated(&firstActivation, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE_WITH_TIMEOUT(firstActivated.size(), 1, 5000);
    const QDBusPendingReply<quint32> firstActivationReply = firstActivation;
    QVERIFY2(!firstActivationReply.isError(),
             qPrintable(firstActivationReply.error().message()));

    QtPowerTransport firstTransport(firstBus.connection);
    PowerClient firstClient(&firstTransport);
    firstClient.start();
    QTRY_VERIFY_WITH_TIMEOUT(firstClient.hasSnapshot(), 10'000);
    const QString firstOwner = firstClient.owner();
    const quint64 firstEpoch = firstClient.snapshot().epoch;
    const pid_t firstPid = firstBus.findServicePid();
    QVERIFY(firstOwner.startsWith(QLatin1Char(':')));
    QVERIFY(firstEpoch != 0);
    QVERIFY(firstPid > 0);

    firstBus.stopDaemon();
    QTRY_VERIFY_WITH_TIMEOUT(!processExists(firstPid), 10'000);
    firstBus.servicePid = 0;
    firstClient.stop();

    // Reserve one extra unique name so a new daemon cannot coincidentally
    // assign the same textual owner as the first independent bus.
    PrivateActivatingBus secondBus;
    QVERIFY(secondBus.start(true));
    QDBusPendingCallWatcher secondActivation(secondBus.connection.asyncCall(start));
    QSignalSpy secondActivated(&secondActivation, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE_WITH_TIMEOUT(secondActivated.size(), 1, 5000);
    const QDBusPendingReply<quint32> secondActivationReply = secondActivation;
    QVERIFY2(!secondActivationReply.isError(),
             qPrintable(secondActivationReply.error().message()));

    QtPowerTransport secondTransport(secondBus.connection);
    PowerClient secondClient(&secondTransport);
    secondClient.start();
    QTRY_VERIFY_WITH_TIMEOUT(secondClient.hasSnapshot(), 10'000);
    const QString secondOwner = secondClient.owner();
    const quint64 secondEpoch = secondClient.snapshot().epoch;
    const pid_t secondPid = secondBus.findServicePid();
    QVERIFY(secondOwner.startsWith(QLatin1Char(':')));
    QVERIFY(secondOwner != firstOwner);
    QVERIFY(secondEpoch != firstEpoch);
    QVERIFY(secondPid > 0);
    QVERIFY(secondPid != firstPid);

    secondBus.stopDaemon();
    QTRY_VERIFY_WITH_TIMEOUT(!processExists(secondPid), 10'000);
    secondBus.servicePid = 0;
    secondClient.stop();
}

QTEST_GUILESS_MAIN(PowerActivationTests)
#include "tst_power_activation.moc"
