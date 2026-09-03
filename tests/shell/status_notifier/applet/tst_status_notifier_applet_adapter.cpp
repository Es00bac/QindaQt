// SPDX-License-Identifier: GPL-3.0-or-later

// Real-composition test for the tray applet source adapter: the S1 watcher
// service, item monitor, registry, and icon renderer composed behind the
// controller-facing seam, driven over a private session bus by the scripted
// fake StatusNotifierItem. Never touches the host bus.

#include <qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h>
#include <qindaqt/shell/status_notifier/applet/status_notifier_monitor_adapter.h>
#include <qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h>

#include "status_notifier_fake_item_test_support.h"
#include "status_notifier_private_bus_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QStandardPaths>
#include <QtTest>

#include <memory>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifierApplet;
using namespace QindaQt::StatusNotifier::TestSupport;

namespace
{

void registerItemOnWatcher(QDBusConnection &itemConnection, const QString &path)
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

class StatusNotifierAppletAdapterTests final : public QObject
{
    Q_OBJECT

private slots:
    void servesPopulationThroughTheSeam();
    void dispatchesExactlyOneActivateThroughController();
    void ownerDisconnectRemovesTheItem();
    void watcherLossDegradesAndRestartRepopulates();
};

void StatusNotifierAppletAdapterTests::servesPopulationThroughTheSeam()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-watcher-a"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-monitor-a"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-item-a"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    item->title = QStringLiteral("Adapter item");
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"), item.get()));
    registerItemOnWatcher(itemConnection, QStringLiteral("/StatusNotifierItem"));

    {
        StatusNotifierMonitorAdapter adapter(monitorConnection, {}, 2'000);
        QCOMPARE(adapter.isRunning(), false);
        adapter.start();
        adapter.start(); // idempotent
        QVERIFY(adapter.isRunning());

        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().state, PresentationState::Ready, 5'000);
        QCOMPARE(adapter.itemDescriptors().size(), 1);
        QCOMPARE(adapter.itemDescriptors().constFirst().title, QStringLiteral("Adapter item"));
        const quint64 generation = adapter.currentGeneration(itemConnection.baseService());
        QVERIFY(generation != 0);

        const OwnerKey key = adapter.presentation().items.constFirst().owner;
        QCOMPARE(key.uniqueName, itemConnection.baseService());
        // The seam's icon boundary never returns null, even with no theme
        // roots and no wire pixmaps: the deterministic placeholder.
        const QImage icon = adapter.renderIcon(key, 22);
        QVERIFY(!icon.isNull());
        QCOMPARE(icon, StatusNotifierIconRenderer::fallbackIcon(22));
        // An unknown key renders the same placeholder instead of failing.
        const OwnerKey unknown { QStringLiteral(":1.999"), QStringLiteral("/Nope"), 77 };
        QCOMPARE(adapter.renderIcon(unknown, 22), StatusNotifierIconRenderer::fallbackIcon(22));

        // The controller-facing surface sees the same truth.
        StatusNotifierAppletController controller(&adapter, true, true);
        QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
        QCOMPARE(controller.watcherLive(), true);
        QCOMPARE(controller.itemRows().size(), 1);
        QCOMPARE(controller.itemCount(), 1);

        adapter.stop();
        adapter.stop(); // idempotent
        QCOMPARE(adapter.isRunning(), false);
        // AGENT-LIFETIME: the adapter is destroyed here, BEFORE the bus
        // connections below disconnect; the monitor must never outlive the
        // registry/sink it drives or the connection it reads.
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("app-watcher-a"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-monitor-a"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-item-a"));
}

void StatusNotifierAppletAdapterTests::dispatchesExactlyOneActivateThroughController()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-watcher-b"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-monitor-b"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-item-b"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"), item.get()));
    registerItemOnWatcher(itemConnection, QStringLiteral("/StatusNotifierItem"));

    {
        StatusNotifierMonitorAdapter adapter(monitorConnection, {}, 2'000);
        adapter.start();
        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().state, PresentationState::Ready, 5'000);
        const OwnerKey key = adapter.presentation().items.constFirst().owner;

        StatusNotifierAppletController controller(&adapter, true, true);
        QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, key.generation), true);
        QTRY_COMPARE_WITH_TIMEOUT(item->recordedIntents.size(), 1, 5'000);
        QCOMPARE(item->recordedIntents.constFirst().member, QStringLiteral("Activate"));
        // The wire contract's signed coordinates are 0,0 from this surface.
        QCOMPARE(item->recordedIntents.constFirst().arguments,
                 QList<QVariant>({ 0, 0 }));
        QTest::qWait(200);
        QCOMPARE(item->recordedIntents.size(), 1);

        // A stale generation never reaches the bus.
        QCOMPARE(controller.activateItem(key.uniqueName, key.objectPath, key.generation + 1),
                 false);
        QTest::qWait(200);
        QCOMPARE(item->recordedIntents.size(), 1);
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("app-watcher-b"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-monitor-b"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-item-b"));
}

void StatusNotifierAppletAdapterTests::ownerDisconnectRemovesTheItem()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-watcher-c"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-monitor-c"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-item-c"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"), item.get()));
    registerItemOnWatcher(itemConnection, QStringLiteral("/StatusNotifierItem"));

    {
        StatusNotifierMonitorAdapter adapter(monitorConnection, {}, 2'000);
        adapter.start();
        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().state, PresentationState::Ready, 5'000);
        StatusNotifierAppletController controller(&adapter, true, true);
        QCOMPARE(controller.itemRows().size(), 1);

        // Drop the last connection reference before disconnectFromBus: while
        // a QDBusConnection copy is alive the socket stays open and the daemon
        // never emits the owner-loss signal the monitor must observe.
        const QString owner = itemConnection.baseService();
        itemConnection = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(QStringLiteral("app-item-c"));

        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().items.size(), 0, 5'000);
        QCOMPARE(adapter.presentation().state, PresentationState::Empty);
        QCOMPARE(adapter.currentGeneration(owner), 0u);
        QTRY_COMPARE_WITH_TIMEOUT(controller.itemRows().size(), 0, 5'000);
        QCOMPARE(controller.phaseText(), QStringLiteral("empty"));
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("app-watcher-c"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-monitor-c"));
}

void StatusNotifierAppletAdapterTests::watcherLossDegradesAndRestartRepopulates()
{
    // AGENT-NOTE: this scenario is what the adapter composition proves beyond
    // the S1 monitor tests: watcher loss surfaces through the seam as
    // truthful Degraded WITH last-known-good rows retained, and a replacement
    // watcher produces a fresh population without any controller restart.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto firstWatcherConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("app-watcher-d1"));
    auto secondWatcherConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("app-watcher-d2"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-monitor-d"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-item-d"));

    auto firstWatcher = std::make_unique<StatusNotifierWatcherService>(firstWatcherConnection);
    QVERIFY2(firstWatcher->start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"), item.get()));
    // Real items re-register with a replacement watcher; arm the fake before
    // its initial registration so the restart below re-admits it.
    item->watchAndReregister(itemConnection, QStringLiteral("/StatusNotifierItem"));
    registerItemOnWatcher(itemConnection, QStringLiteral("/StatusNotifierItem"));

    {
        StatusNotifierMonitorAdapter adapter(monitorConnection, {}, 2'000);
        adapter.start();
        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().state, PresentationState::Ready, 5'000);
        const OwnerKey firstKey = adapter.presentation().items.constFirst().owner;

        StatusNotifierAppletController controller(&adapter, true, true);
        QCOMPARE(controller.itemRows().size(), 1);

        // The watcher dies; last-known-good items stay visible and actionable.
        firstWatcher->stop();
        firstWatcherConnection = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(QStringLiteral("app-watcher-d1"));
        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().state, PresentationState::Degraded,
                                  5'000);
        QCOMPARE(adapter.presentation().diagnostic,
                 QStringLiteral("status-notifier-watcher-unavailable"));
        QCOMPARE(adapter.presentation().items.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(controller.phaseText(), QStringLiteral("degraded"), 5'000);
        QCOMPARE(controller.itemRows().size(), 1);
        QCOMPARE(controller.watcherLive(), false);

        // A replacement watcher opens a fresh epoch; the fake re-registers
        // and the seam shows a fresh population under a new generation.
        StatusNotifierWatcherService secondWatcher(secondWatcherConnection);
        QVERIFY2(secondWatcher.start(&error), qPrintable(error));
        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().state, PresentationState::Ready, 5'000);
        QTRY_VERIFY_WITH_TIMEOUT(!adapter.presentation().items.isEmpty()
                                     && adapter.presentation().items.constFirst().owner.generation
                                         != firstKey.generation,
                                 5'000);
        QTRY_COMPARE_WITH_TIMEOUT(controller.phaseText(), QStringLiteral("ready"), 5'000);
        QCOMPARE(controller.watcherLive(), true);
        QCOMPARE(controller.itemRows().size(), 1);
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("app-watcher-d2"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-monitor-d"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-item-d"));
}

QTEST_GUILESS_MAIN(StatusNotifierAppletAdapterTests)
#include "tst_status_notifier_applet_adapter.moc"
