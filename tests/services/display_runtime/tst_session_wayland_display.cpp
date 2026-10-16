// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0379: Display1 waits, bounded, for the session's Wayland display instead
// of exiting with status 4 when activation precedes the session publication.

#include "../../../src/services/display_runtime/src/session_wayland_display_p.h"
#include "../display_service/support/private_bus_test_support.h"

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusVirtualObject>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace QindaQt::DisplayRuntime;
using QindaQt::Platform::Recovery::ManualRecoveryTimer;
using namespace QindaQt::DisplayService::TestSupport;

namespace
{

int listenAt(const QString &path)
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const QByteArray bytes = QFile::encodeName(path);
    std::memcpy(address.sun_path, bytes.constData(), static_cast<size_t>(bytes.size()) + 1);
    if (fd < 0 || ::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0
        || ::listen(fd, 4) != 0) {
        return -1;
    }
    return fd;
}

// The systemd user manager's published environment, as qindaqt-session
// updates it with SetEnvironment.
class FakeManager final : public QDBusVirtualObject
{
public:
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() != QStringLiteral("org.freedesktop.DBus.Properties")
            || message.member() != QStringLiteral("Get")
            || message.arguments().value(1).toString() != QStringLiteral("Environment")) {
            return false;
        }
        ++queries;
        connection.send(message.createReply(QVariant::fromValue(QDBusVariant(environment))));
        return true;
    }
    QStringList environment{QStringLiteral("XDG_CURRENT_DESKTOP=QindaQt")};
    int queries = 0;
};

struct Timers {
    QList<ManualRecoveryTimer *> created;
    WaylandDisplayWaitOptions options()
    {
        WaylandDisplayWaitOptions value;
        value.maximumAttempts = 4;
        value.timers = [this] {
            auto timer = std::make_unique<ManualRecoveryTimer>();
            created.append(timer.get());
            return timer;
        };
        return value;
    }
};

} // namespace

class SessionWaylandDisplayTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void environmentBlockAndSocketChecks()
    {
        QCOMPARE(waylandDisplayFromEnvironment({QStringLiteral("A=1"),
                                                QStringLiteral("WAYLAND_DISPLAY=qindaqt-0"),
                                                QStringLiteral("WAYLAND_DISPLAY=qindaqt-1")}),
                 QStringLiteral("qindaqt-1"));
        QVERIFY(waylandDisplayFromEnvironment({QStringLiteral("DISPLAY=:0")}).isEmpty());
        QTemporaryDir runtime;
        const int fd = listenAt(runtime.filePath(QStringLiteral("qindaqt-0")));
        QVERIFY(fd >= 0);
        QFile plain(runtime.filePath(QStringLiteral("plain")));
        QVERIFY(plain.open(QIODevice::WriteOnly));
        plain.close();
        QVERIFY(waylandSocketPresent(runtime.path(), QStringLiteral("qindaqt-0")));
        QVERIFY(waylandSocketPresent({}, runtime.filePath(QStringLiteral("qindaqt-0"))));
        QVERIFY(!waylandSocketPresent(runtime.path(), QStringLiteral("plain")));
        QVERIFY(!waylandSocketPresent(runtime.path(), QStringLiteral("qindaqt-9")));
        QVERIFY(!waylandSocketPresent(runtime.path(), QStringLiteral("../qindaqt-0")));
        QVERIFY(!waylandSocketPresent({}, QStringLiteral("qindaqt-0")));
        ::close(fd);
    }

    void presentProcessValueResolvesAtOnce()
    {
        QTemporaryDir runtime;
        const int fd = listenAt(runtime.filePath(QStringLiteral("qindaqt-0")));
        Timers timers;
        WaylandDisplayWait wait(QDBusConnection(QStringLiteral("absent")),
                                QStringLiteral("qindaqt-0"), runtime.path(),
                                timers.options());
        wait.start();
        QVERIFY(wait.finished());
        QCOMPARE(wait.display(), QStringLiteral("qindaqt-0"));
        QCOMPARE(wait.attempts(), 1);
        ::close(fd);
    }

    void waitsForTheSessionToPublishItsDisplay()
    {
        PrivateSessionBus bus;
        QString error;
        QVERIFY2(bus.start(&error), qPrintable(error));
        const QString managerName = privateConnectionName(QStringLiteral("manager"));
        const QString clientName = privateConnectionName(QStringLiteral("display1"));
        QDBusConnection managerBus = QDBusConnection::connectToBus(bus.address(), managerName);
        QDBusConnection client = QDBusConnection::connectToBus(bus.address(), clientName);
        FakeManager manager;
        QVERIFY(managerBus.registerVirtualObject(QStringLiteral("/org/freedesktop/systemd1"),
                                                 &manager));
        QVERIFY(managerBus.registerService(QStringLiteral("org.freedesktop.systemd1")));
        QTemporaryDir runtime;
        Timers timers;
        // Activated at login with no WAYLAND_DISPLAY in its environment.
        WaylandDisplayWait wait(client, QString(), runtime.path(), timers.options());
        QSignalSpy done(&wait, &WaylandDisplayWait::done);
        wait.start();
        auto &timer = *timers.created.first();
        QTRY_VERIFY(timer.isActive());
        QVERIFY(!wait.finished());
        // The compositor's socket exists, but the session has not published it.
        const int fd = listenAt(runtime.filePath(QStringLiteral("qindaqt-1")));
        QVERIFY(timer.fire());
        QTRY_VERIFY(timer.isActive());
        QVERIFY(!wait.finished());
        // qindaqt-session publishes the activation environment.
        manager.environment.append(QStringLiteral("WAYLAND_DISPLAY=qindaqt-1"));
        QVERIFY(timer.fire());
        QTRY_COMPARE(done.size(), 1);
        QCOMPARE(wait.display(), QStringLiteral("qindaqt-1"));
        QCOMPARE(manager.queries, 3);
        using std::chrono::milliseconds;
        QCOMPARE(timer.requested(), (QList<milliseconds>{milliseconds{250}, milliseconds{500}}));
        ::close(fd);
        managerBus.unregisterObject(QStringLiteral("/org/freedesktop/systemd1"));
        QDBusConnection::disconnectFromBus(clientName);
        QDBusConnection::disconnectFromBus(managerName);
        client = QDBusConnection(QStringLiteral("released-client"));
        managerBus = QDBusConnection(QStringLiteral("released-manager"));
        bus.stop();
    }

    void boundedWaitFailsWithAReason()
    {
        QTemporaryDir runtime;
        Timers timers;
        // No bus at all: every attempt fails; the bound ends the wait.
        WaylandDisplayWait wait(QDBusConnection(QStringLiteral("absent")),
                                QStringLiteral("qindaqt-7"), runtime.path(),
                                timers.options());
        wait.start();
        for (int attempt = 1; attempt < 4; ++attempt) {
            QVERIFY(!wait.finished());
            QVERIFY(timers.created.first()->fire());
        }
        QVERIFY(wait.finished());
        QVERIFY(wait.display().isEmpty());
        QCOMPARE(wait.attempts(), 4);
        QVERIFY(!wait.failure().isEmpty());
    }
};

QTEST_GUILESS_MAIN(SessionWaylandDisplayTest)

#include "tst_session_wayland_display.moc"
