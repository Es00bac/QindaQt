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
#include <vector>

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

// A minimal scripted org.kde.StatusNotifierWatcher that, unlike the
// production watcher service, does NOT cap its registered-item inventory —
// the over-advertising (broken or hostile) watcher shape the registry's
// membership-capacity defense exists for. Serves only the property read
// through FakePropertiesAdaptor and emits the protocol signals.
class ScriptedOveradvertisingWatcher final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")

signals:
    void StatusNotifierItemRegistered(const QString &serviceId);
    void StatusNotifierItemUnregistered(const QString &serviceId);
};

class StatusNotifierAppletAdapterTests final : public QObject
{
    Q_OBJECT

private slots:
    void servesPopulationThroughTheSeam();
    void dispatchesExactlyOneActivateThroughController();
    void ownerDisconnectRemovesTheItem();
    void watcherLossDegradesAndRestartRepopulates();
    void malformedReplacementNotifiesDegradedImmediatelyAndAcknowledges();
    void capacityRejectionNotifiesDegradedImmediately();
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

void StatusNotifierAppletAdapterTests::malformedReplacementNotifiesDegradedImmediatelyAndAcknowledges()
{
    // AGENT-NOTE: P1 regression control. A malformed live replacement makes
    // the registry REJECT the update while setting its degradation marker and
    // retaining the last-known-good descriptor. The adapter must notify the
    // controller on that transition itself; on the unrepaired seam the
    // controller kept projecting ready until an unrelated accepted update.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-watcher-e"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-monitor-e"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-item-e"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    auto item = std::make_unique<FakeStatusNotifierItem>();
    item->title = QStringLiteral("Live item");
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"), item.get()));
    registerItemOnWatcher(itemConnection, QStringLiteral("/StatusNotifierItem"));

    {
        StatusNotifierMonitorAdapter adapter(monitorConnection, {}, 2'000);
        adapter.start();
        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().state, PresentationState::Ready, 5'000);

        StatusNotifierAppletController controller(&adapter, true, true);
        QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
        QCOMPARE(controller.itemRows().size(), 1);

        // The live item publishes a malformed replacement: an embedded C0
        // control character fails the descriptor admission gate. The
        // monitor's refetch still reaches registerItem (the key counts as
        // observed), so the registry marks itself degraded and keeps the
        // last-known-good descriptor presented.
        item->title = QStringLiteral("Bad\u0001title");
        emit item->NewTitle();
        QTRY_COMPARE_WITH_TIMEOUT(controller.phaseText(), QStringLiteral("degraded"), 5'000);
        QCOMPARE(controller.phaseReasonText(), QStringLiteral("malformed-item-replacement"));
        QCOMPARE(adapter.presentation().diagnostic,
                 QStringLiteral("malformed-item-replacement"));
        // Last-known-good stays presented while degraded.
        QCOMPARE(controller.itemRows().size(), 1);
        QCOMPARE(adapter.itemDescriptors().size(), 1);
        QCOMPARE(adapter.itemDescriptors().constFirst().title, QStringLiteral("Live item"));

        // The contract-prescribed acknowledgement transition is exposed at
        // the composed boundary: the controller action clears the registry
        // degradation through the seam and the phase recomputes immediately.
        controller.acknowledgeDegraded();
        QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
        QCOMPARE(controller.phaseReasonText(), QString());
        QCOMPARE(controller.itemRows().size(), 1);

        // A later valid replacement is admitted on its own merits and must
        // not be the event that first revealed or cleared the degradation.
        item->title = QStringLiteral("Recovered item");
        emit item->NewTitle();
        QTRY_VERIFY_WITH_TIMEOUT(
            adapter.itemDescriptors().constFirst().title == QStringLiteral("Recovered item"),
            5'000);
        QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
        QCOMPARE(adapter.presentation().state, PresentationState::Ready);
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("app-watcher-e"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-monitor-e"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-item-e"));
}

void StatusNotifierAppletAdapterTests::capacityRejectionNotifiesDegradedImmediately()
{
    // AGENT-NOTE: P1 regression control, capacity arm. The production watcher
    // service enforces kMaxItems at registration time, so a registry
    // membership overflow can only arrive from a watcher that over-advertises
    // — scripted here with an uncapped fake. The 65th live registration is
    // rejected AND marks the registry degraded; the adapter must notify
    // immediately so the controller projects degraded with the retained
    // 64-item last-known-good set.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-watcher-f"));
    auto monitorConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-monitor-f"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("app-item-f"));

    // One item owner serves 65 distinct object paths with 65 distinct item
    // identities; the scripted watcher advertises the first 64.
    std::vector<std::unique_ptr<FakeStatusNotifierItem>> items;
    QStringList serviceIds;
    for (int index = 1; index <= 65; ++index) {
        auto capacityItem = std::make_unique<FakeStatusNotifierItem>();
        capacityItem->id = QStringLiteral("org.qindaqt.capacity.%1").arg(index);
        capacityItem->title = QStringLiteral("Capacity item %1").arg(index);
        const QString path = QStringLiteral("/Item%1").arg(index);
        QVERIFY(registerFakeItem(itemConnection, path, capacityItem.get()));
        if (index <= 64) {
            serviceIds.append(itemConnection.baseService() + path);
        }
        items.push_back(std::move(capacityItem));
    }

    // Minimal uncapped watcher: serves only the RegisteredStatusNotifierItems
    // property read and emits the protocol registration signal. The dynamic
    // property is what FakePropertiesAdaptor::Get answers from.
    ScriptedOveradvertisingWatcher watcher;
    watcher.setProperty("RegisteredStatusNotifierItems", serviceIds);
    new FakePropertiesAdaptor(&watcher);
    QVERIFY(watcherConnection.registerObject(
        QString::fromLatin1(kWatcherObjectPath), &watcher,
        QDBusConnection::ExportAdaptors | QDBusConnection::ExportAllSignals));
    QVERIFY(watcherConnection.registerService(QString::fromLatin1(kWatcherServiceName)));

    {
        StatusNotifierMonitorAdapter adapter(monitorConnection, {}, 2'000);
        adapter.start();
        QTRY_COMPARE_WITH_TIMEOUT(adapter.presentation().state, PresentationState::Ready, 30'000);
        QCOMPARE(adapter.presentation().items.size(), 64);

        StatusNotifierAppletController controller(&adapter, true, true);
        QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
        QCOMPARE(controller.itemCount(), 64);
        QCOMPARE(controller.presentedCount(), 24);
        QCOMPARE(controller.overflowCount(), 40);

        // A 65th live registration arrives after population completed: the
        // registry rejects it on capacity and marks itself degraded. No
        // accepted event follows, so only an immediate notification moves the
        // controller.
        emit watcher.StatusNotifierItemRegistered(
            itemConnection.baseService() + QStringLiteral("/Item65"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.phaseText(), QStringLiteral("degraded"), 5'000);
        QCOMPARE(controller.phaseReasonText(), QStringLiteral("item-capacity-exceeded"));
        // The retained last-known-good set is intact and still presented.
        QCOMPARE(adapter.presentation().items.size(), 64);
        QCOMPARE(controller.itemCount(), 64);

        // Acknowledgement recovers to ready with the same retained set.
        controller.acknowledgeDegraded();
        QCOMPARE(controller.phaseText(), QStringLiteral("ready"));
        QCOMPARE(controller.itemCount(), 64);
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("app-watcher-f"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-monitor-f"));
    QDBusConnection::disconnectFromBus(QStringLiteral("app-item-f"));
}

QTEST_GUILESS_MAIN(StatusNotifierAppletAdapterTests)
#include "tst_status_notifier_applet_adapter.moc"
