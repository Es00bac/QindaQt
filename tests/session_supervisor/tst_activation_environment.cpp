// SPDX-License-Identifier: GPL-3.0-or-later
#include <QProcess>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVirtualObject>
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
        publisher.start(QStringLiteral(QINDAQT_ACTIVATION_PUBLISHER), {});
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
};
QTEST_GUILESS_MAIN(ActivationEnvironmentTests)
#include "tst_activation_environment.moc"
