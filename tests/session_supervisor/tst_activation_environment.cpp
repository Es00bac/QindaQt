// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVirtualObject>
#include <QtDBus/QDBusConnectionInterface>
#include <QtTest>

class UserManager final : public QDBusVirtualObject {
public:
    QStringList assignments;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override {
        if (message.member() != QStringLiteral("SetEnvironment")) return false;
        assignments = message.arguments().constFirst().toStringList();
        connection.send(message.createReply());
        return true;
    }
};
namespace {

// Stands up a standalone dbus-daemon on an explicit unix:path and registers
// the fake user manager there, mirroring the systemd private control socket
// the user manager always exposes at $XDG_RUNTIME_DIR/systemd/private.
struct PrivateManagerBus {
    QProcess daemon;
    QString socket;
    QString connectionName;
    UserManager manager;

    bool start(const QString &tag)
    {
        socket = QDir::temp().filePath(
            QStringLiteral("qindaqt-%1-%2.sock").arg(tag).arg(QCoreApplication::applicationPid()));
        connectionName = QStringLiteral("qindaqt-%1-test").arg(tag);
        daemon.start(QStringLiteral("dbus-daemon"),
                     {QStringLiteral("--session"), QStringLiteral("--nofork"),
                      QStringLiteral("--print-address=1"),
                      QStringLiteral("--address=unix:path=") + socket});
        return daemon.waitForStarted();
    }

    [[nodiscard]] bool bind()
    {
        QDBusConnection bus = QDBusConnection::connectToBus(
            QStringLiteral("unix:path=") + socket, connectionName);
        if (!bus.isConnected()) return false;
        if (!bus.registerService(QStringLiteral("org.freedesktop.systemd1"))) return false;
        return bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager);
    }

    ~PrivateManagerBus()
    {
        QDBusConnection::disconnectFromBus(connectionName);
        daemon.terminate();
        daemon.waitForFinished(3000);
    }
};

} // namespace

class ActivationEnvironmentTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void exportsCurrentDesktopBeforeConsumersStart() {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        UserManager manager;
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"), &manager));
        QProcess publisher;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("qindaqt-test-socket"));
        environment.insert(QStringLiteral("DISPLAY"), QStringLiteral(":99"));
        environment.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("QindaQt"));
        environment.insert(QStringLiteral("UNRELATED_PRIVATE_VALUE"), QStringLiteral("must-not-export"));
        publisher.setProcessEnvironment(environment);
        // Force the session-bus fallback lane: no private socket there, and
        // the fake manager owns the session-bus name. Production's native
        // lane is covered by the private-socket test below.
        publisher.start(QStringLiteral(QINDAQT_ACTIVATION_PUBLISHER),
                        {QStringLiteral("/nonexistent")});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 0);
        QVERIFY(manager.assignments.contains(QStringLiteral("WAYLAND_DISPLAY=qindaqt-test-socket")));
        QVERIFY(manager.assignments.contains(QStringLiteral("DISPLAY=:99")));
        QVERIFY(manager.assignments.contains(QStringLiteral("XDG_CURRENT_DESKTOP=QindaQt")));
        QVERIFY(manager.assignments.contains(QStringLiteral("DBUS_SESSION_BUS_ADDRESS=")
                                             + environment.value(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"))));
        for (const QString &assignment : manager.assignments)
            QVERIFY(!assignment.startsWith(QStringLiteral("UNRELATED_PRIVATE_VALUE=")));
        QVERIFY(!publisher.readAllStandardError().contains("Could not update"));
        bus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.systemd1"));
    }

    // Production regression (2026-09-13): on a private dbus-run-session
    // session bus, org.freedesktop.systemd1 is unowned and a session-bus
    // SetEnvironment activated a second, failing user manager, so the user
    // manager kept a stale bus address across logins. The manager must be
    // reached through its private control socket instead.
    void publishesSetEnvironmentToTheSystemdPrivateSocketWhenItExists()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        QVERIFY(!bus.interface()->isServiceRegistered(
            QStringLiteral("org.freedesktop.systemd1")));

        PrivateManagerBus managerBus;
        QVERIFY(managerBus.start(QStringLiteral("activation")));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(managerBus.socket), 4000);
        QVERIFY(managerBus.bind());

        QProcess publisher;
        publisher.start(QStringLiteral(QINDAQT_ACTIVATION_PUBLISHER), {managerBus.socket});
        QVERIFY(publisher.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(publisher.state() == QProcess::NotRunning, 6000);
        QCOMPARE(publisher.exitCode(), 0);
        QVERIFY(!managerBus.manager.assignments.isEmpty());
        QVERIFY(managerBus.manager.assignments.contains(
            QStringLiteral("DBUS_SESSION_BUS_ADDRESS=")
            + QProcessEnvironment::systemEnvironment().value(
                QStringLiteral("DBUS_SESSION_BUS_ADDRESS"))));
        QVERIFY(!publisher.readAllStandardError().contains("Could not update"));
    }
};
QTEST_GUILESS_MAIN(ActivationEnvironmentTests)
#include "tst_activation_environment.moc"
