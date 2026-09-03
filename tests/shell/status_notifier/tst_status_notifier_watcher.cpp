// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h>

#include "status_notifier_private_bus_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include <memory>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifier::TestSupport;

namespace
{

class HostSignalRecorder final : public QObject
{
    Q_OBJECT

public:
    QStringList owners;

public slots:
    void record(const QString &owner) { owners.append(owner); }
};

// Blocking D-Bus calls from this thread would starve the watcher object,
// which lives in the same thread: its slots can only be dispatched while the
// event loop runs. Async calls plus QTRY_VERIFY keep the loop pumping.
QDBusMessage watcherCall(QDBusConnection &caller,
                         const QString &member,
                         const QVariant &argument,
                         const QString &service = {})
{
    auto message = QDBusMessage::createMethodCall(
        service.isEmpty() ? QString::fromLatin1(kWatcherServiceName) : service,
        QString::fromLatin1(kWatcherObjectPath),
        QString::fromLatin1(kWatcherInterfaceName),
        member);
    message << argument;
    QDBusPendingCall pending = caller.asyncCall(message);
    if (!QTest::qWaitFor([&pending]() { return pending.isFinished(); }, 5'000)) {
        return {};
    }
    return pending.reply();
}

QVariant watcherProperty(QDBusConnection &caller, const QString &name)
{
    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath),
        QStringLiteral("org.freedesktop.D-Bus.Properties"),
        QStringLiteral("Get"));
    message << QVariant(QString::fromLatin1(kWatcherInterfaceName)) << QVariant(name);
    QDBusPendingCall pending = caller.asyncCall(message);
    if (!QTest::qWaitFor([&pending]() { return pending.isFinished(); }, 5'000)) {
        return {};
    }
    const QDBusMessage reply = pending.reply();
    QVariant value = reply.arguments().value(0);
    if (value.canConvert<QDBusVariant>()) {
        value = value.value<QDBusVariant>().variant();
    }
    return value;
}

} // namespace

class StatusNotifierWatcherTests final : public QObject
{
    Q_OBJECT

private slots:
    void registersBarePathAgainstCaller();
    void registersServiceNameResolvedThroughBus();
    void duplicateRegistrationIsIgnored();
    void rejectsMalformedRegistrations();
    void registersHostsAndTracksHostProperty();
    void emitsHostUnregisteredWhenOwnerDisconnects();
    void retiresItemsWhenOwnerDisconnects();
    void refusesRegistrationsWhileNameOwnedElsewhere();
    void degradedStartIsIdempotent();
    void stopReleasesNameAndClearsState();
    void enforcesItemCapacity();
};

void StatusNotifierWatcherTests::registersBarePathAgainstCaller()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-a"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("item-a"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    QCOMPARE(watcher.state(), WatcherServiceState::Active);
    QVERIFY(watcher.degradedReason().isEmpty());
    QCOMPARE(qdbus_cast<int>(watcherProperty(watcherConnection,
                                             QStringLiteral("ProtocolVersion"))),
             0);
    QCOMPARE(qdbus_cast<bool>(watcherProperty(watcherConnection,
                                              QStringLiteral("IsStatusNotifierHostRegistered"))),
             false);

    QSignalSpy registeredSpy(&watcher, &StatusNotifierWatcherService::itemRegistered);
    const QDBusMessage reply =
        watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                    QVariant(QStringLiteral("/org/qindaqt/TrayItem")));
    QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
    QCOMPARE(registeredSpy.count(), 1);
    const OwnerKey key = registeredSpy.constFirst().at(0).value<OwnerKey>();
    QCOMPARE(key.uniqueName, itemConnection.baseService());
    QCOMPARE(key.objectPath, QStringLiteral("/org/qindaqt/TrayItem"));
    QCOMPARE(watcher.registeredItemServiceIds(),
             QStringList{itemConnection.baseService()
                             + QStringLiteral("/org/qindaqt/TrayItem")});
    QCOMPARE(qdbus_cast<QStringList>(watcherProperty(watcherConnection,
                                                     QStringLiteral("RegisteredStatusNotifierItems"))),
             watcher.registeredItemServiceIds());

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-a"));
    QDBusConnection::disconnectFromBus(QStringLiteral("item-a"));
}

void StatusNotifierWatcherTests::registersServiceNameResolvedThroughBus()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-b"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("item-b"));
    QVERIFY(itemConnection.registerService(QStringLiteral("org.qindaqt.FakeTray")));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    const QDBusMessage reply =
        watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                    QVariant(QStringLiteral("org.qindaqt.FakeTray")));
    QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
    // The item is keyed to the resolved owner's unique name at the
    // conventional /StatusNotifierItem path, never the well-known name.
    QCOMPARE(watcher.registeredItemServiceIds(),
             QStringList{itemConnection.baseService() + QStringLiteral("/StatusNotifierItem")});

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-b"));
    QDBusConnection::disconnectFromBus(QStringLiteral("item-b"));
}

void StatusNotifierWatcherTests::duplicateRegistrationIsIgnored()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-c"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("item-c"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    QSignalSpy registeredSpy(&watcher, &StatusNotifierWatcherService::itemRegistered);
    QVERIFY(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                        QVariant(QStringLiteral("/StatusNotifierItem")))
                .type()
            == QDBusMessage::ReplyMessage);
    QVERIFY(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                        QVariant(QStringLiteral("/StatusNotifierItem")))
                .type()
            == QDBusMessage::ReplyMessage);
    QCOMPARE(registeredSpy.count(), 1);
    QCOMPARE(watcher.registeredItems().size(), 1);

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-c"));
    QDBusConnection::disconnectFromBus(QStringLiteral("item-c"));
}

void StatusNotifierWatcherTests::rejectsMalformedRegistrations()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-d"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("item-d"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    QCOMPARE(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                         QVariant(QStringLiteral("/bad//path")))
                 .type(),
             QDBusMessage::ErrorMessage);
    QCOMPARE(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                         QVariant(QStringLiteral("not a bus name")))
                 .type(),
             QDBusMessage::ErrorMessage);
    QCOMPARE(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                         QVariant(QStringLiteral("org.missing.Service")))
                 .type(),
             QDBusMessage::ErrorMessage);
    QCOMPARE(watcher.registeredItems().size(), 0);

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-d"));
    QDBusConnection::disconnectFromBus(QStringLiteral("item-d"));
}

void StatusNotifierWatcherTests::registersHostsAndTracksHostProperty()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-e"));
    auto hostConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-e"));
    QVERIFY(hostConnection.registerService(QStringLiteral("org.qindaqt.TrayHost")));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    QCOMPARE(qdbus_cast<bool>(watcherProperty(watcherConnection,
                                              QStringLiteral("IsStatusNotifierHostRegistered"))),
             false);

    QSignalSpy hostSpy(&watcher, &StatusNotifierWatcherService::hostRegistered);
    const QDBusMessage reply =
        watcherCall(hostConnection, QStringLiteral("RegisterStatusNotifierHost"),
                    QVariant(QStringLiteral("org.qindaqt.TrayHost")));
    QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
    QCOMPARE(hostSpy.count(), 1);
    QCOMPARE(hostSpy.constFirst().at(0).toString(), hostConnection.baseService());
    QCOMPARE(qdbus_cast<bool>(watcherProperty(watcherConnection,
                                              QStringLiteral("IsStatusNotifierHostRegistered"))),
             true);

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-e"));
    QDBusConnection::disconnectFromBus(QStringLiteral("host-e"));
}

void StatusNotifierWatcherTests::emitsHostUnregisteredWhenOwnerDisconnects()
{
    // AGENT-NOTE: P1-1 regression: host retirement must emit the documented
    // wire signal, not only update IsStatusNotifierHostRegistered.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-host-loss"));
    auto hostConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-loss"));
    QVERIFY(hostConnection.registerService(QStringLiteral("org.qindaqt.LossHost")));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    HostSignalRecorder recorder;
    QVERIFY(watcherConnection.connect(
        QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath),
        QString::fromLatin1(kWatcherInterfaceName),
        QStringLiteral("StatusNotifierHostUnregistered"),
        &recorder,
        SLOT(record(QString))));
    QSignalSpy localSpy(&watcher, &StatusNotifierWatcherService::hostUnregistered);
    QCOMPARE(watcherCall(hostConnection, QStringLiteral("RegisterStatusNotifierHost"),
                         QVariant(QStringLiteral("org.qindaqt.LossHost")))
                 .type(),
             QDBusMessage::ReplyMessage);
    const QString owner = hostConnection.baseService();
    hostConnection = QDBusConnection(QString());
    QDBusConnection::disconnectFromBus(QStringLiteral("host-loss"));

    QTRY_COMPARE_WITH_TIMEOUT(recorder.owners.size(), 1, 5'000);
    QCOMPARE(recorder.owners.constFirst(), owner);
    QCOMPARE(localSpy.size(), 1);
    QCOMPARE(watcher.registeredHosts(), QStringList());

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-host-loss"));
}

void StatusNotifierWatcherTests::retiresItemsWhenOwnerDisconnects()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-f"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    QString owner;
    {
        auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("item-f"));
        owner = itemConnection.baseService();
        QVERIFY(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                            QVariant(QStringLiteral("/StatusNotifierItem")))
                    .type()
                == QDBusMessage::ReplyMessage);
        QCOMPARE(watcher.registeredItems().size(), 1);
    }
    // Drop every QDBusConnection copy before disconnectFromBus: with a live
    // copy the named connection stays connected and the daemon never emits
    // NameOwnerChanged, so the watcher has nothing to retire.
    QDBusConnection::disconnectFromBus(QStringLiteral("item-f"));

    QSignalSpy unregisteredSpy(&watcher, &StatusNotifierWatcherService::itemUnregistered);
    QTRY_COMPARE_WITH_TIMEOUT(unregisteredSpy.count(), 1, 5'000);
    QCOMPARE(unregisteredSpy.constFirst().at(0).value<OwnerKey>().uniqueName, owner);
    QCOMPARE(watcher.registeredItems().size(), 0);

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-f"));
}

void StatusNotifierWatcherTests::refusesRegistrationsWhileNameOwnedElsewhere()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto firstConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-g1"));
    auto secondConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-g2"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("item-g"));

    StatusNotifierWatcherService first(firstConnection);
    QVERIFY2(first.start(&error), qPrintable(error));

    StatusNotifierWatcherService second(secondConnection);
    QVERIFY2(second.start(&error), qPrintable(error));
    QCOMPARE(second.state(), WatcherServiceState::NameOwnedElsewhere);
    QVERIFY(!second.isActive());
    QCOMPARE(second.degradedReason(), QStringLiteral("watcher-name-owned-elsewhere"));

    // The degraded watcher must not impersonate the real one: registrations
    // sent to it directly (addressed by its unique name, since it cannot own
    // the well-known name) are refused with an error reply while the first
    // watcher keeps serving.
    QCOMPARE(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                         QVariant(QStringLiteral("/StatusNotifierItem")),
                         secondConnection.baseService())
                 .type(),
             QDBusMessage::ErrorMessage);
    QVERIFY(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                        QVariant(QStringLiteral("/StatusNotifierItem")))
                .type()
            == QDBusMessage::ReplyMessage);
    QCOMPARE(first.registeredItems().size(), 1);
    QCOMPARE(second.registeredItems().size(), 0);

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-g1"));
    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-g2"));
    QDBusConnection::disconnectFromBus(QStringLiteral("item-g"));
}

void StatusNotifierWatcherTests::degradedStartIsIdempotent()
{
    // AGENT-NOTE: P2-1 regression: repeated degraded starts preserve the
    // initial successful NameOwnedElsewhere result.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto ownerConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-idempotent-owner"));
    auto degradedConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-idempotent-degraded"));

    StatusNotifierWatcherService owner(ownerConnection);
    StatusNotifierWatcherService degraded(degradedConnection);
    QVERIFY2(owner.start(&error), qPrintable(error));
    QVERIFY2(degraded.start(&error), qPrintable(error));
    QCOMPARE(degraded.state(), WatcherServiceState::NameOwnedElsewhere);
    QVERIFY(degraded.start(&error));
    QCOMPARE(degraded.state(), WatcherServiceState::NameOwnedElsewhere);
    QCOMPARE(degraded.degradedReason(), QStringLiteral("watcher-name-owned-elsewhere"));

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-idempotent-owner"));
    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-idempotent-degraded"));
}

void StatusNotifierWatcherTests::stopReleasesNameAndClearsState()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto firstConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-h1"));
    auto secondConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-h2"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("item-h"));

    auto first = std::make_unique<StatusNotifierWatcherService>(firstConnection);
    QVERIFY2(first->start(&error), qPrintable(error));
    QVERIFY(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                        QVariant(QStringLiteral("/StatusNotifierItem")))
                .type()
            == QDBusMessage::ReplyMessage);

    first->stop();
    QCOMPARE(first->state(), WatcherServiceState::Stopped);
    QCOMPARE(first->registeredItems().size(), 0);

    // The released name is claimable by a replacement watcher.
    StatusNotifierWatcherService second(secondConnection);
    QVERIFY2(second.start(&error), qPrintable(error));
    QCOMPARE(second.state(), WatcherServiceState::Active);
    QCOMPARE(second.degradedReason(), QString());

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-h1"));
    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-h2"));
    QDBusConnection::disconnectFromBus(QStringLiteral("item-h"));
}

void StatusNotifierWatcherTests::enforcesItemCapacity()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("watcher-i"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("item-i"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    for (int index = 0; index < kMaxItems; ++index) {
        QVERIFY2(watcherCall(itemConnection,
                             QStringLiteral("RegisterStatusNotifierItem"),
                             QVariant(QStringLiteral("/Item%1").arg(index)))
                     .type()
                     == QDBusMessage::ReplyMessage,
                 qPrintable(QStringLiteral("registration %1 refused").arg(index)));
    }
    QCOMPARE(watcherCall(itemConnection, QStringLiteral("RegisterStatusNotifierItem"),
                         QVariant(QStringLiteral("/Item64")))
                 .type(),
             QDBusMessage::ErrorMessage);
    QCOMPARE(watcher.registeredItems().size(), kMaxItems);

    QDBusConnection::disconnectFromBus(QStringLiteral("watcher-i"));
    QDBusConnection::disconnectFromBus(QStringLiteral("item-i"));
}

QTEST_GUILESS_MAIN(StatusNotifierWatcherTests)
#include "tst_status_notifier_watcher.moc"
