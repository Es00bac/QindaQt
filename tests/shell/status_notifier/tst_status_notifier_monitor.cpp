// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_presentation.h>
#include <qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h>

#include "status_notifier_private_bus_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QStandardPaths>
#include <QtTest>

#include <memory>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifier::TestSupport;

namespace
{

void registerItem(QDBusConnection &itemConnection, const QString &path)
{
    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath),
        QString::fromLatin1(kWatcherInterfaceName),
        QStringLiteral("RegisterStatusNotifierItem"));
    message << QVariant(path);
    // Async + event-loop pumping: the watcher lives in this thread and its
    // slots cannot dispatch while the caller blocks.
    QDBusPendingCall pending = itemConnection.asyncCall(message);
    QTRY_VERIFY_WITH_TIMEOUT(pending.isFinished(), 5'000);
    QCOMPARE(pending.reply().type(), QDBusMessage::ReplyMessage);
}

} // namespace

class StatusNotifierMonitorTests final : public QObject
{
    Q_OBJECT

private slots:
    void populatesRegistryFromLiveWatcher();
    void removesItemAndFreesOwnerOnDisconnect();
    void rebaselinesPopulationWhenWatcherRestarts();
    void degradesTruthfullyWhileKeepingLastKnownGood();
    void dispatchesOnlyValidatedIntents();
    void refusesAttachmentContractViolations();
};

void StatusNotifierMonitorTests::populatesRegistryFromLiveWatcher()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-a"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-client-a"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-item-a"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    auto item = std::make_unique<FakeStatusNotifierItem>();
    item->title = QStringLiteral("Monitored item");
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));
    registerItem(itemConnection, QStringLiteral("/StatusNotifierItem"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 2'000);
    QCOMPARE(monitor.isWatcherLive(), false);
    monitor.attach(&registry);
    QVERIFY(monitor.isAttached());
    QCOMPARE(monitor.isWatcherLive(), true);

    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 5'000);
    QVERIFY(registry.initialPopulationComplete());
    const auto keys = registry.itemKeys();
    QCOMPARE(keys.size(), 1);
    QCOMPARE(keys.constFirst().uniqueName, itemConnection.baseService());
    QCOMPARE(keys.constFirst().objectPath, QStringLiteral("/StatusNotifierItem"));
    const auto descriptor = registry.find(keys.constFirst());
    QVERIFY(descriptor.has_value());
    QCOMPARE(descriptor->identity, QStringLiteral("org.qindaqt.fake"));
    QCOMPARE(descriptor->title, QStringLiteral("Monitored item"));

    // Live presentation maps the monitor's liveness to Ready, not Loading.
    const TrayPresentation projected =
        projectPresentation(registry, {.transportLive = monitor.isWatcherLive()});
    QCOMPARE(projected.state, PresentationState::Ready);

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-a"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-a"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-item-a"));
}

void StatusNotifierMonitorTests::removesItemAndFreesOwnerOnDisconnect()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-b"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-client-b"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-item-b"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));
    registerItem(itemConnection, QStringLiteral("/StatusNotifierItem"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 2'000);
    monitor.attach(&registry);
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 5'000);
    QVERIFY(registry.isOwnerLive(itemConnection.baseService()));

    const QString owner = itemConnection.baseService();
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-item-b"));
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 0, 5'000);
    // The bounded owner tracking slot must be freed by the loss report, not
    // leak until capacity.
    QVERIFY(!registry.isOwnerLive(owner));

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-b"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-b"));
}

void StatusNotifierMonitorTests::rebaselinesPopulationWhenWatcherRestarts()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto firstWatcherConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-c1"));
    auto secondWatcherConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-c2"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-client-c"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-item-c"));

    auto firstWatcher = std::make_unique<StatusNotifierWatcherService>(firstWatcherConnection);
    QVERIFY2(firstWatcher->start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));
    registerItem(itemConnection, QStringLiteral("/StatusNotifierItem"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 2'000);
    monitor.attach(&registry);
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 5'000);
    const quint64 firstEpoch = registry.currentWatcherEpoch();

    // The watcher process dies; the registry keeps last-known-good items.
    firstWatcher->stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-c1"));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.isWatcherLive(), false, 5'000);
    QCOMPARE(registry.count(), 1);
    QCOMPARE(projectPresentation(registry, {.transportLive = false}).state,
             PresentationState::Degraded);

    // A replacement watcher opens a fresh epoch and re-populates.
    StatusNotifierWatcherService secondWatcher(secondWatcherConnection);
    QVERIFY2(secondWatcher.start(&error), qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 5'000);
    QVERIFY(registry.currentWatcherEpoch() > firstEpoch);
    QVERIFY(registry.initialPopulationComplete());
    QCOMPARE(projectPresentation(registry, {.transportLive = monitor.isWatcherLive()}).state,
             PresentationState::Ready);

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-c2"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-c"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-item-c"));
}

void StatusNotifierMonitorTests::degradesTruthfullyWhileKeepingLastKnownGood()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-d"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-client-d"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-item-d"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));
    registerItem(itemConnection, QStringLiteral("/StatusNotifierItem"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 2'000);
    monitor.attach(&registry);
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 5'000);

    monitor.detach();
    QVERIFY(!monitor.isAttached());
    QCOMPARE(monitor.isWatcherLive(), false);
    // Detach must not corrupt the registry the shell still presents from.
    QCOMPARE(registry.count(), 1);

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-d"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-d"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-item-d"));
}

void StatusNotifierMonitorTests::dispatchesOnlyValidatedIntents()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-e"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-client-e"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-item-e"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"),
                             item.get()));
    registerItem(itemConnection, QStringLiteral("/StatusNotifierItem"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 2'000);
    monitor.attach(&registry);
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 5'000);
    const OwnerKey liveKey = registry.itemKeys().constFirst();

    const RegistryOutcome activate = monitor.requestActivate(liveKey, 5, 9);
    QVERIFY2(activate.accepted(), qPrintable(activate.reasonCode));
    const RegistryOutcome scroll = monitor.requestScroll(liveKey, -3,
                                                         QStringLiteral("horizontal"));
    QVERIFY2(scroll.accepted(), qPrintable(scroll.reasonCode));
    const RegistryOutcome menu = monitor.requestContextMenu(liveKey, 1, 1);
    QVERIFY2(menu.accepted(), qPrintable(menu.reasonCode));

    QTRY_COMPARE_WITH_TIMEOUT(item->recordedIntents.size(), 3, 5'000);
    const QList<QVariant> scrollArgs{-3, QVariant(QStringLiteral("horizontal"))};
    QCOMPARE(item->recordedIntents.at(0).member, QStringLiteral("Activate"));
    QCOMPARE(item->recordedIntents.at(1).member, QStringLiteral("Scroll"));
    QCOMPARE(item->recordedIntents.at(1).arguments, scrollArgs);
    QCOMPARE(item->recordedIntents.at(2).member, QStringLiteral("ContextMenu"));

    // A stale generation is refused and nothing is sent.
    OwnerKey staleKey = liveKey;
    staleKey.generation += 100;
    QCOMPARE(monitor.requestActivate(staleKey, 0, 0).status,
             RegistryStatus::StaleOwner);
    QCOMPARE(monitor.requestScroll(liveKey, 1, QStringLiteral("diagonal")).status,
             RegistryStatus::InvalidRequest);
    QTest::qWait(200);
    QCOMPARE(item->recordedIntents.size(), 3);

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-e"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-e"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-item-e"));
}

void StatusNotifierMonitorTests::refusesAttachmentContractViolations()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-client-f"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 2'000);
    QCOMPARE(monitor.isAttached(), false);

    monitor.attach(nullptr); // Null-first refusal.
    QCOMPARE(monitor.isAttached(), false);
    monitor.attach(&registry);
    QVERIFY(monitor.isAttached());
    monitor.attach(&registry); // Re-attachment refusal.
    QVERIFY(monitor.isAttached());
    monitor.detach();
    QVERIFY(!monitor.isAttached());
    monitor.detach(); // Idempotent detach.
    QVERIFY(!monitor.isAttached());
    QCOMPARE(registry.currentWatcherEpoch(), 0u); // No epoch without a watcher.

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-f"));
}

QTEST_GUILESS_MAIN(StatusNotifierMonitorTests)
#include "tst_status_notifier_monitor.moc"
