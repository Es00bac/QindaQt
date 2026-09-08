// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h>
#include <qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h>
#include "status_notifier_fake_item_test_support.h"
#include "../global_menu/dbusmenu/fake_dbusmenu_exporter.h"
#include <QDBusPendingCall>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifier::TestSupport;
namespace Menu = QindaQt::Shell::GlobalMenu::DbusMenu;
namespace MenuTest = QindaQt::Shell::GlobalMenu::Test;

namespace {
struct Fixture {
    PrivateSessionBus bus;
    QDBusConnection watcherBus{QStringLiteral("none")};
    QDBusConnection itemBus{QStringLiteral("none")};
    QDBusConnection clientBus{QStringLiteral("none")};
    std::unique_ptr<StatusNotifierWatcherService> watcher;
    FakeStatusNotifierItem item;
    MenuTest::FakeDbusMenuExporter exporter;
    StatusNotifierRegistry registry;
    std::unique_ptr<StatusNotifierItemMonitor> monitor;
    OwnerKey key;
    QString error;
    const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ~Fixture() {
        monitor.reset();
        watcher.reset();
        QDBusConnection::disconnectFromBus(watcherBus.name());
        QDBusConnection::disconnectFromBus(itemBus.name());
        QDBusConnection::disconnectFromBus(clientBus.name());
    }

    void start(const QString &menuPath = QStringLiteral("/Menu")) {
        QVERIFY2(bus.start(&error), qPrintable(error));
        watcherBus = connectToPrivateBus(bus.address(), QStringLiteral("tray-menu-watcher-") + suffix);
        itemBus = connectToPrivateBus(bus.address(), QStringLiteral("tray-menu-item-") + suffix);
        clientBus = connectToPrivateBus(bus.address(), QStringLiteral("tray-menu-client-") + suffix);
        watcher = std::make_unique<StatusNotifierWatcherService>(watcherBus);
        QVERIFY2(watcher->start(&error), qPrintable(error));
        Menu::registerDbusMenuWireTypes();
        exporter.setLayout(1, MenuTest::menuLayout());
        QVERIFY(itemBus.registerObject(QStringLiteral("/Menu"), &exporter,
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
                | QDBusConnection::ExportScriptableProperties));
        item.menu = QDBusObjectPath(menuPath);
        QVERIFY(registerFakeItem(itemBus, QStringLiteral("/StatusNotifierItem"), &item));
        auto message = QDBusMessage::createMethodCall(QString::fromLatin1(kWatcherServiceName),
            QString::fromLatin1(kWatcherObjectPath), QString::fromLatin1(kWatcherInterfaceName),
            QStringLiteral("RegisterStatusNotifierItem"));
        message << QStringLiteral("/StatusNotifierItem");
        auto pending = itemBus.asyncCall(message);
        QTRY_VERIFY_WITH_TIMEOUT(pending.isFinished(), 5'000);
        QCOMPARE(pending.reply().type(), QDBusMessage::ReplyMessage);
        monitor = std::make_unique<StatusNotifierItemMonitor>(clientBus, registry, 500);
        monitor->attach(&registry);
        QTRY_COMPARE_WITH_TIMEOUT(registry.count(), 1, 5'000);
        key = registry.itemKeys().first();
    }
    void open() {
        QVERIFY(monitor->openMenu(key, 30, 40).accepted());
        QTRY_COMPARE_WITH_TIMEOUT(state(), QStringLiteral("ready"), 5'000);
    }
    QString state() const { return monitor->menuState(key).value(QStringLiteral("status")).toString(); }
    quint64 revision() const { return monitor->menuState(key).value(QStringLiteral("revision")).toString().toULongLong(); }
    QVariantList entries() const { return monitor->menuState(key).value(QStringLiteral("entries")).toList(); }
    void newMenu() {
        auto signal = QDBusMessage::createSignal(QStringLiteral("/StatusNotifierItem"),
            QString::fromLatin1(kItemInterfaceName), QStringLiteral("NewMenu"));
        QVERIFY(itemBus.send(signal));
    }
};
}

class StatusNotifierMenuTests final : public QObject {
    Q_OBJECT
private slots:
    void opensRealMenuAndDispatchesOneLeaf();
    void blocksStaleDisabledHiddenAndParentActions();
    void newMenuRebindsPathAndItemIsMenu();
    void removalFencesMenuActions();
    void ownerLossFencesMenuActions();
    void failedAdvertisedMenuDoesNotFallBack();
    void opensEmptyLazySubmenu();
    void malformedDescriptorRetiresActionableMenu();
    void legacyFallbackAndInvalidOrientation();
};

void StatusNotifierMenuTests::opensRealMenuAndDispatchesOneLeaf() {
    Fixture f;
    f.start();
    QVERIFY(f.monitor);
    QVERIFY(f.monitor->hasExportedMenu(f.key));
    QVERIFY(!f.monitor->itemIsMenu(f.key));
    QCOMPARE(f.exporter.layoutCallCount(), 0);
    f.open();
    QCOMPARE(f.exporter.lastAboutToShowId(), 0);
    QCOMPARE(f.entries().size(), 2);
    const auto submenu = f.entries().at(1).toMap();
    QCOMPARE(submenu.value(QStringLiteral("kind")).toString(), QStringLiteral("submenu"));
    QCOMPARE(submenu.value(QStringLiteral("children")).toList().size(), 1);
    const auto beforeSubmenu = f.revision();
    QVERIFY(f.monitor->aboutToShowMenu(f.key, beforeSubmenu, 10).accepted());
    QVERIFY(!f.monitor->invokeMenu(f.key, beforeSubmenu, 11).accepted());
    QTRY_COMPARE_WITH_TIMEOUT(f.exporter.lastAboutToShowId(), 10, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(f.state(), QStringLiteral("ready"), 5'000);
    QCOMPARE(f.revision(), beforeSubmenu);
    const auto revision = f.revision();
    QVERIFY(f.monitor->invokeMenu(f.key, revision, 11).accepted());
    QVERIFY(!f.monitor->invokeMenu(f.key, revision, 11).accepted());
    QTRY_COMPARE_WITH_TIMEOUT(f.exporter.eventCount(), 1, 5'000);
    QVERIFY(f.item.recordedIntents.isEmpty());
}

void StatusNotifierMenuTests::blocksStaleDisabledHiddenAndParentActions() {
    Fixture f;
    f.start();
    QVERIFY(f.monitor);
    f.open();
    const auto revision = f.revision();
    QVERIFY(!f.monitor->invokeMenu(f.key, revision, 0).accepted());
    QVERIFY(!f.monitor->invokeMenu(f.key, revision, 10).accepted());
    QVERIFY(!f.monitor->invokeMenu(f.key, revision, 999).accepted());
    auto layout = MenuTest::menuLayout();
    auto action = layout.children[0].value<Menu::LayoutItem>();
    action.properties[QStringLiteral("enabled")] = false;
    layout.children[0] = QVariant::fromValue(action);
    auto parent = layout.children[1].value<Menu::LayoutItem>();
    parent.properties[QStringLiteral("visible")] = false;
    layout.children[1] = QVariant::fromValue(parent);
    f.exporter.setLayout(2, layout);
    bool pendingRefused = false;
    connect(f.monitor.get(), &StatusNotifierItemMonitor::menuChanged, this, [&] {
        if (f.state() == QStringLiteral("loading")) {
            pendingRefused = !f.monitor->invokeMenu(f.key, revision, 1).accepted();
        }
    });
    f.exporter.announceLayout(2);
    QTRY_VERIFY_WITH_TIMEOUT(f.revision() > revision && f.state() == QStringLiteral("ready"), 5'000);
    QVERIFY(pendingRefused);
    QVERIFY(!f.monitor->invokeMenu(f.key, revision, 1).accepted());
    QVERIFY(!f.monitor->invokeMenu(f.key, f.revision(), 1).accepted());
    QVERIFY(!f.monitor->invokeMenu(f.key, f.revision(), 11).accepted());
    QCOMPARE(f.entries().size(), 1);
    QCOMPARE(f.exporter.eventCount(), 0);
}

void StatusNotifierMenuTests::newMenuRebindsPathAndItemIsMenu() {
    Fixture f;
    f.start();
    QVERIFY(f.monitor);
    f.open();
    const auto oldRevision = f.revision();
    MenuTest::FakeDbusMenuExporter replacement;
    auto layout = MenuTest::menuLayout(QStringLiteral("_Replacement"));
    auto action = layout.children[0].value<Menu::LayoutItem>();
    action.properties[QStringLiteral("toggle-type")] = QStringLiteral("checkmark");
    action.properties[QStringLiteral("toggle-state")] = 1;
    layout.children[0] = QVariant::fromValue(action);
    replacement.setLayout(1, layout);
    QVERIFY(f.itemBus.registerObject(QStringLiteral("/Replacement"), &replacement,
        QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
            | QDBusConnection::ExportScriptableProperties));
    f.item.menu = QDBusObjectPath(QStringLiteral("/Replacement"));
    f.item.itemIsMenu = true;
    f.newMenu();
    QTRY_VERIFY_WITH_TIMEOUT(f.monitor->itemIsMenu(f.key), 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(f.state(), QStringLiteral("ready"), 5'000);
    QCOMPARE(f.entries().first().toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Replacement"));
    QVERIFY(f.entries().first().toMap().value(QStringLiteral("checked")).toBool());
    QVERIFY(!f.monitor->invokeMenu(f.key, oldRevision, 1).accepted());
    QVERIFY(f.monitor->invokeMenu(f.key, f.revision(), 1).accepted());
    QTRY_COMPARE_WITH_TIMEOUT(replacement.eventCount(), 1, 5'000);
    QCOMPARE(f.exporter.eventCount(), 0);
}

void StatusNotifierMenuTests::removalFencesMenuActions() {
    Fixture f;
    f.start();
    QVERIFY(f.monitor);
    f.open();
    const auto revision = f.revision();
    auto stale = f.key;
    ++stale.generation;
    QVERIFY(!f.monitor->invokeMenu(stale, revision, 1).accepted());
    auto descriptor = *f.registry.find(f.key);
    descriptor.identity += QStringLiteral(".replacement");
    QVERIFY(f.registry.registerItem(f.registry.currentWatcherEpoch(), f.key, descriptor).accepted());
    QCOMPARE(f.monitor->invokeMenu(f.key, revision, 1).reasonCode, QStringLiteral("menu-identity-changed"));
    const auto result = f.registry.removeItem(f.registry.currentWatcherEpoch(), f.key);
    QVERIFY(result.accepted());
    QVERIFY(!f.monitor->invokeMenu(f.key, revision, 1).accepted());
    QCOMPARE(f.monitor->menuState(f.key).value(QStringLiteral("status")).toString(), QStringLiteral("none"));
    QCOMPARE(f.exporter.eventCount(), 0);
}

void StatusNotifierMenuTests::ownerLossFencesMenuActions() {
    Fixture f;
    f.start();
    QVERIFY(f.monitor);
    f.open();
    const auto revision = f.revision();
    QDBusConnection::disconnectFromBus(f.itemBus.name());
    f.itemBus = QDBusConnection(QStringLiteral("retired-tray-item"));
    QTRY_COMPARE_WITH_TIMEOUT(f.registry.count(), 0, 5'000);
    QVERIFY(!f.monitor->invokeMenu(f.key, revision, 1).accepted());
    QVERIFY(!f.monitor->openMenu(f.key, 1, 2).accepted());
    QCOMPARE(f.state(), QStringLiteral("none"));
    QCOMPARE(f.exporter.eventCount(), 0);
}

void StatusNotifierMenuTests::failedAdvertisedMenuDoesNotFallBack() {
    Fixture f;
    f.start(QStringLiteral("/MissingMenu"));
    QVERIFY(f.monitor);
    QVERIFY(f.monitor->hasExportedMenu(f.key));
    QVERIFY(f.monitor->openMenu(f.key, 1, 2).accepted());
    QTRY_COMPARE_WITH_TIMEOUT(f.state(), QStringLiteral("error"), 5'000);
    QVERIFY(!f.monitor->invokeMenu(f.key, f.revision(), 1).accepted());
    QVERIFY(f.item.recordedIntents.isEmpty());
}

void StatusNotifierMenuTests::opensEmptyLazySubmenu() {
    Fixture f;
    f.start();
    QVERIFY(f.monitor);
    auto layout = MenuTest::menuLayout();
    auto submenu = layout.children[1].value<Menu::LayoutItem>();
    submenu.children.clear();
    layout.children[1] = QVariant::fromValue(submenu);
    f.exporter.setLayout(1, layout);
    f.open();
    QCOMPARE(f.entries()[1].toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("submenu"));
    QVERIFY(f.entries()[1].toMap().value(QStringLiteral("children")).toList().isEmpty());
    QVERIFY(f.monitor->aboutToShowMenu(f.key, f.revision(), 10).accepted());
    QTRY_COMPARE_WITH_TIMEOUT(f.exporter.lastAboutToShowId(), 10, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(f.state(), QStringLiteral("ready"), 5'000);
}

void StatusNotifierMenuTests::malformedDescriptorRetiresActionableMenu() {
    Fixture f;
    f.start();
    QVERIFY(f.monitor);
    f.open();
    const auto revision = f.revision();
    f.item.title = QString(3000, QLatin1Char('x'));
    emit f.item.NewTitle();
    QTRY_COMPARE_WITH_TIMEOUT(f.state(), QStringLiteral("error"), 5'000);
    QVERIFY(!f.monitor->invokeMenu(f.key, revision, 1).accepted());
    QCOMPARE(f.exporter.eventCount(), 0);
}

void StatusNotifierMenuTests::legacyFallbackAndInvalidOrientation() {
    Fixture f;
    f.start(QStringLiteral("/NO_DBUSMENU"));
    QVERIFY(f.monitor);
    QVERIFY(!f.monitor->hasExportedMenu(f.key));
    QVERIFY(f.monitor->openMenu(f.key, -20, 40).accepted());
    QTRY_COMPARE_WITH_TIMEOUT(f.item.recordedIntents.size(), 1, 5'000);
    QCOMPARE(f.item.recordedIntents.first().member, QStringLiteral("ContextMenu"));
    QVERIFY(f.monitor->requestScroll(f.key, -120, QStringLiteral("vertical")).accepted());
    QTRY_COMPARE_WITH_TIMEOUT(f.item.recordedIntents.size(), 2, 5'000);
    QVERIFY(!f.monitor->requestScroll(f.key, 120, QStringLiteral("diagonal")).accepted());
    QCOMPARE(f.exporter.layoutCallCount(), 0);
}

QTEST_GUILESS_MAIN(StatusNotifierMenuTests)
#include "tst_status_notifier_menu.moc"
