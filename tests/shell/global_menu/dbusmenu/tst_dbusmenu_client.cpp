// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>

#include "fake_dbusmenu_exporter.h"

#include <QtDBus/QDBusConnection>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Shell::GlobalMenu;

class DbusMenuClientTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void privateBusFencesRevisionsAndDoesNotReplayEvents();
};

void DbusMenuClientTest::privateBusFencesRevisionsAndDoesNotReplayEvents()
{
    DbusMenu::registerDbusMenuWireTypes();
    const QString providerConnectionName = QStringLiteral("qindaqt-dbusmenu-provider");
    const QString clientConnectionName = QStringLiteral("qindaqt-dbusmenu-client");
    QDBusConnection provider =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, providerConnectionName);
    QDBusConnection clientBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(provider.isConnected());
    QVERIFY(clientBus.isConnected());

    Test::FakeDbusMenuExporter exporter;
    exporter.setLayout(1, Test::menuLayout());
    QVERIFY(provider.registerObject(
        QStringLiteral("/Menu"), &exporter,
        QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
            | QDBusConnection::ExportScriptableProperties));

    DbusMenu::DbusMenuClient client(clientBus, provider.baseService(),
                                    QDBusObjectPath(QStringLiteral("/Menu")),
                                    QUuid::createUuid());
    QSignalSpy changed(&client, &DbusMenu::DbusMenuClient::treeChanged);
    QSignalSpy rejected(&client, &DbusMenu::DbusMenuClient::rejected);
    QSignalSpy unavailable(&client, &DbusMenu::DbusMenuClient::unavailable);
    QVERIFY(client.start());
    QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, 5'000);
    QTRY_VERIFY_WITH_TIMEOUT(client.metadata().valid, 5'000);
    QCOMPARE(client.remoteRevision(), quint32{1});
    QCOMPARE(client.snapshot().tree.items.first().text, QStringLiteral("File"));

    client.requestGroupProperties({1});
    client.aboutToShow(10);
    QTRY_COMPARE_WITH_TIMEOUT(exporter.groupPropertiesCallCount(), 1, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(exporter.aboutToShowCallCount(), 1, 5'000);
    const int callsBeforePropertiesSignal = exporter.layoutCallCount();
    Q_EMIT exporter.ItemsPropertiesUpdated(
        {{.id = 1, .properties = {{QStringLiteral("enabled"), false}}}}, {});
    QTRY_VERIFY_WITH_TIMEOUT(exporter.layoutCallCount() > callsBeforePropertiesSignal, 5'000);

    exporter.setLayout(1, Test::menuLayout(QStringLiteral("_Changed")));
    exporter.announceLayout(2);
    QTRY_VERIFY_WITH_TIMEOUT(!rejected.isEmpty(), 5'000);
    QCOMPARE(client.snapshot().tree.items.first().text, QStringLiteral("File"));

    exporter.setLayout(2, Test::menuLayout(QStringLiteral("_Changed")));
    exporter.announceLayout(2);
    QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 2, 5'000);
    QCOMPARE(client.snapshot().tree.items.first().text, QStringLiteral("Changed"));

    QSignalSpy completed(&client, &DbusMenu::DbusMenuClient::eventCompleted);
    client.sendEvent(1, QStringLiteral("clicked"));
    QTRY_COMPARE_WITH_TIMEOUT(exporter.eventCount(), 1, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 5'000);
    QTest::qWait(100);
    QCOMPARE(exporter.eventCount(), 1);

    QDBusConnection::disconnectFromBus(providerConnectionName);
    provider = QDBusConnection(QStringLiteral("qindaqt-retired-dbusmenu-provider"));
    QTRY_COMPARE_WITH_TIMEOUT(unavailable.size(), 1, 5'000);
    QVERIFY(!client.isStarted());
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

QTEST_GUILESS_MAIN(DbusMenuClientTest)

#include "tst_dbusmenu_client.moc"
