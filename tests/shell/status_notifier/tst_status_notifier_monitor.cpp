// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_presentation.h>
#include <qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h>

#include "status_notifier_fake_item_test_support.h"
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

void emitItemUnregistered(QDBusConnection &watcherConnection,
                          const QString &serviceId)
{
    auto message = QDBusMessage::createSignal(
        QString::fromLatin1(kWatcherObjectPath),
        QString::fromLatin1(kWatcherInterfaceName),
        QStringLiteral("StatusNotifierItemUnregistered"));
    message << QVariant(serviceId);
    QVERIFY(watcherConnection.send(message));
}

} // namespace

class StatusNotifierMonitorTests final : public QObject
{
    Q_OBJECT

private slots:
    void populatesRegistryFromLiveWatcher();
    void populatesTwoPathsFromOneOwner();
    void preservesOwnerGenerationAfterLastPathRetires();
    void populatesRootObjectPath();
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

void StatusNotifierMonitorTests::populatesTwoPathsFromOneOwner()
{
    // AGENT-NOTE: P1-3 regression: two valid paths from one unique owner
    // share one generation and both drain initial-population accounting.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-multi"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-client-multi"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-item-multi"));
    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    auto first = std::make_unique<FakeStatusNotifierItem>();
    auto second = std::make_unique<FakeStatusNotifierItem>();
    first->id = QStringLiteral("org.qindaqt.first");
    second->id = QStringLiteral("org.qindaqt.second");
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/One"), first.get()));
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/Two"), second.get()));
    registerItem(itemConnection, QStringLiteral("/One"));
    registerItem(itemConnection, QStringLiteral("/Two"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 100);
    monitor.attach(&registry);
    QTRY_VERIFY_WITH_TIMEOUT(registry.initialPopulationComplete(), 2'000);
    QCOMPARE(registry.count(), 2);
    const QList<OwnerKey> keys = registry.itemKeys();
    QCOMPARE(keys.at(0).generation, keys.at(1).generation);

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-multi"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-multi"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-item-multi"));
}

void StatusNotifierMonitorTests::preservesOwnerGenerationAfterLastPathRetires()
{
    // AGENT-NOTE: second-review P1-1 regression: a path-retirement signal is
    // not owner loss. A later path from the same live unique owner and watcher
    // epoch must retain its generation even when no item slot survived.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-path-turnover"));
    auto monitorConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("mon-client-path-turnover"));
    auto itemConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("mon-item-path-turnover"));
    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    auto first = std::make_unique<FakeStatusNotifierItem>();
    auto second = std::make_unique<FakeStatusNotifierItem>();
    first->id = QStringLiteral("org.qindaqt.turnover.first");
    second->id = QStringLiteral("org.qindaqt.turnover.second");
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/One"), first.get()));
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/Two"), second.get()));
    registerItem(itemConnection, QStringLiteral("/One"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 500);
    monitor.attach(&registry);
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 2'000);
    const quint64 epoch = registry.currentWatcherEpoch();
    const quint64 generation = registry.itemKeys().constFirst().generation;
    const QString owner = itemConnection.baseService();

    emitItemUnregistered(watcherConnection, owner + QStringLiteral("/One"));
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 0, 2'000);
    QVERIFY(registry.isOwnerLive(owner));
    QCOMPARE(registry.currentWatcherEpoch(), epoch);
    QCOMPARE(registry.currentGeneration(owner), generation);

    registerItem(itemConnection, QStringLiteral("/Two"));
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 2'000);
    const OwnerKey replacement = registry.itemKeys().constFirst();
    QCOMPARE(replacement.uniqueName, owner);
    QCOMPARE(replacement.objectPath, QStringLiteral("/Two"));
    QCOMPARE(replacement.generation, generation);
    QCOMPARE(registry.currentWatcherEpoch(), epoch);

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-path-turnover"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-path-turnover"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-item-path-turnover"));
}

void StatusNotifierMonitorTests::populatesRootObjectPath()
{
    // AGENT-NOTE: P1-4 regression: ":1.N/" contains the complete valid root
    // path and must survive service-ID parsing.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-watcher-root"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-client-root"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("mon-item-root"));
    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/"), item.get()));
    registerItem(itemConnection, QStringLiteral("/"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 500);
    monitor.attach(&registry);
    QTRY_VERIFY_WITH_TIMEOUT(registry.initialPopulationComplete(), 2'000);
    QCOMPARE(registry.count(), 1);
    QCOMPARE(registry.itemKeys().constFirst().objectPath, QStringLiteral("/"));

    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-root"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-client-root"));
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-item-root"));
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
    // Drop the last connection reference before disconnectFromBus: while a
    // QDBusConnection copy is alive the socket stays open and the daemon
    // never emits the owner-loss signal the monitor must observe.
    itemConnection = QDBusConnection(QString());
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
    // Real items re-register with a replacement watcher; arm the fake before
    // its initial registration so the restart below re-admits it.
    item->watchAndReregister(itemConnection, QStringLiteral("/StatusNotifierItem"));
    registerItem(itemConnection, QStringLiteral("/StatusNotifierItem"));

    StatusNotifierRegistry registry;
    StatusNotifierItemMonitor monitor(monitorConnection, registry, 2'000);
    monitor.attach(&registry);
    QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 5'000);
    const quint64 firstEpoch = registry.currentWatcherEpoch();
    const quint64 firstItemGeneration = registry.itemKeys().constFirst().generation;

    // The watcher process dies; the registry keeps last-known-good items.
    firstWatcher->stop();
    // Dropping the last reference closes the socket so the daemon emits the
    // watcher-name loss the monitor rebaselines on.
    firstWatcherConnection = QDBusConnection(QString());
    QDBusConnection::disconnectFromBus(QStringLiteral("mon-watcher-c1"));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.isWatcherLive(), false, 5'000);
    QCOMPARE(registry.count(), 1);
    QCOMPARE(projectPresentation(registry, {.transportLive = false}).state,
             PresentationState::Degraded);

    // A replacement watcher opens a fresh epoch and re-populates.
    StatusNotifierWatcherService secondWatcher(secondWatcherConnection);
    QVERIFY2(secondWatcher.start(&error), qPrintable(error));
    // The name-acquisition signal is delivered asynchronously; wait on the
    // liveness and epoch transitions themselves. The count alone passes
    // vacuously on last-known-good, so gate the rest on the item being
    // re-admitted under a fresh owner generation by the new population.
    QTRY_COMPARE_WITH_TIMEOUT(monitor.isWatcherLive(), true, 5'000);
    QTRY_VERIFY_WITH_TIMEOUT(registry.currentWatcherEpoch() > firstEpoch, 5'000);
    QTRY_VERIFY_WITH_TIMEOUT(!registry.itemKeys().isEmpty()
                                 && registry.itemKeys().constFirst().generation
                                     != firstItemGeneration,
                             5'000);
    QVERIFY(registry.initialPopulationComplete());
    QCOMPARE(registry.count(), 1);
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
