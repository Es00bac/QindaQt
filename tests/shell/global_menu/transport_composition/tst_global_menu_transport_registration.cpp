// SPDX-License-Identifier: GPL-3.0-or-later
#include "transport_composition_fixture.h"

#include <QtTest/QTest>

using namespace QindaQt::Shell::GlobalMenu;
using namespace QindaQt::Shell::GlobalMenu::TestSupport;

namespace {
class FakeAnnouncedMenuSource final : public Composition::AnnouncedMenuAddressSource
{
public:
    QUuid expectedWindow;
    std::optional<Composition::AnnouncedMenuAddress> address;

    [[nodiscard]] std::optional<Composition::AnnouncedMenuAddress>
    announcedMenuFor(const Ownership::WindowIdentity &window) const override
    {
        return window.windowId == expectedWindow ? address : std::nullopt;
    }
};
} // namespace

class GlobalMenuTransportRegistrationTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void focusedRegistrationPublishesAndActivatesExactlyOnce();
    void announcedNativeAddressUsesExactOwnerAndClearsOnLoss();
};

void GlobalMenuTransportRegistrationTest::focusedRegistrationPublishesAndActivatesExactlyOnce()
{
    DbusMenu::registerDbusMenuWireTypes();
    Registrar::registerRegistrarWireTypes();
    const QString registrarName = QStringLiteral("qindaqt-global-menu-registrar");
    const QString providerName = QStringLiteral("qindaqt-global-menu-provider");
    const QString shellName = QStringLiteral("qindaqt-global-menu-shell");
    QDBusConnection registrarBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, registrarName);
    QDBusConnection providerBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, providerName);
    QDBusConnection shellBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, shellName);
    QVERIFY(registrarBus.isConnected());
    QVERIFY(providerBus.isConnected());
    QVERIFY(shellBus.isConnected());

    Test::FakeDbusMenuExporter exporter;
    exporter.setLayout(1, Test::menuLayout());
    QVERIFY(providerBus.registerObject(
        QStringLiteral("/Menu"), &exporter,
        QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
            | QDBusConnection::ExportScriptableProperties));

    Registrar::AppMenuRegistrar registrar(registrarBus);
    QCOMPARE(registrar.start(), Registrar::RegistrarStartStatus::Started);
    QCOMPARE(registrarCall(providerBus, QStringLiteral("RegisterWindow"),
                           {QVariant::fromValue(quint32{77}),
                            QVariant::fromValue(QDBusObjectPath(QStringLiteral("/Menu")))})
                 .type(),
             QDBusMessage::ReplyMessage);

    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource active;
    active.observation = Ownership::ActiveWindowObservation{
        .window = Ownership::WindowIdentity{
            .windowId = windowId,
            .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
        .focusGeneration = 5};
    FakeRegistrarWindowIdSource resolver;
    resolver.expectedWindow = windowId;
    resolver.registrarId = 77;
    GlobalMenuAppletAccess applet;
    applet.attachRenderer();
    Composition::GlobalMenuTransportCoordinator coordinator(
        shellBus, active, resolver, *registrar.registry(), applet);
    QSignalSpy hosted(&coordinator,
                      &Composition::GlobalMenuTransportCoordinator::hostedMenuChanged);
    coordinator.refreshFocus();
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(hosted.size(), 1, 5'000);
    QCOMPARE(hosted.constFirst().at(2).toBool(), true);
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("File"));
    QVERIFY(coordinator.publishedTree().has_value());
    const Protocol::MenuTree initialTree = *coordinator.publishedTree();

    QSignalSpy observed(&exporter, &Test::FakeDbusMenuExporter::eventObserved);
    applet.activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(observed.size(), 1, 5'000);
    QTest::qWait(100);
    QCOMPARE(exporter.eventCount(), 1);

    exporter.setLayout(1, Test::menuLayout(QStringLiteral("_Spoofed")));
    exporter.announceLayout(2);
    QTest::qWait(100);
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("File"));

    exporter.setLayout(2, Test::menuLayout(QStringLiteral("_Edit")));
    exporter.announceLayout(2);
    QTRY_COMPARE_WITH_TIMEOUT(
        applet.items().first().toMap().value(QStringLiteral("text")).toString(),
        QStringLiteral("Edit"), 5'000);

    Test::FakeDbusMenuExporter replacementExporter;
    replacementExporter.setLayout(1, Test::menuLayout(QStringLiteral("_View")));
    QVERIFY(providerBus.registerObject(
        QStringLiteral("/ReplacementMenu"), &replacementExporter,
        QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
            | QDBusConnection::ExportScriptableProperties));
    auto *retiredClient = coordinator.findChild<DbusMenu::DbusMenuClient *>();
    QVERIFY(retiredClient != nullptr);
    QVERIFY(QMetaObject::invokeMethod(retiredClient, "unavailable",
                                      Qt::QueuedConnection));
    // Replace synchronously after queuing the old client's failure. The late
    // callback must be fenced by its captured client generation and endpoint,
    // leaving the replacement binding intact.
    QCOMPARE(registrar.registry()
                 ->registerWindow(77, providerBus.baseService(),
                                  QDBusObjectPath(QStringLiteral("/ReplacementMenu")))
                 .outcome,
             Registrar::RegistrationOutcome::Updated);
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("View"));
    QVERIFY(coordinator.publishedTree().has_value());
    QCOMPARE(coordinator.publishedTree()->epoch, initialTree.epoch);
    QVERIFY(coordinator.publishedTree()->revision > initialTree.revision);

    const qsizetype hostedBeforeFocusRetirement = hosted.size();
    active.observation.reset();
    coordinator.refreshFocus();
    // Authority is revoked synchronously at the selector: the invocation guard
    // admits nothing while no authenticated focus exists, so no Event crosses.
    // The facade itself stays live — a transient withdrawal must not close an
    // open popup or disable delegates mid-click.
    QVERIFY(applet.available());
    // Ordinary focus retirement retains the proven endpoint so the inactive
    // application's content height does not jump.
    QCOMPARE(hosted.size(), hostedBeforeFocusRetirement);
    // The last presentation is retained through the bounded grace window
    // instead of collapsing the panel slot.
    QVERIFY(!applet.items().isEmpty());
    const QString retainedGeneration =
        applet.items().first().toMap().value(QStringLiteral("generation")).toString();
    applet.activate(QStringLiteral("1"));
    QTest::qWait(100);
    QCOMPARE(exporter.eventCount(), 1);

    active.observation = Ownership::ActiveWindowObservation{
        .window = Ownership::WindowIdentity{
            .windowId = windowId,
            .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
        .focusGeneration = 7};
    coordinator.refreshFocus();
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    // The same provider re-proved itself within the grace window: the retained
    // acknowledgment and projection continue; nothing is re-announced or
    // rebuilt.
    QCOMPARE(hosted.size(), hostedBeforeFocusRetirement);
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("View"));
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("generation")).toString(),
             retainedGeneration);
    QDBusConnection::disconnectFromBus(providerName);
    providerBus = QDBusConnection(QStringLiteral("qindaqt-retired-global-menu-provider"));
    QTRY_VERIFY_WITH_TIMEOUT(!applet.available(), 5'000);
    QTRY_VERIFY_WITH_TIMEOUT(applet.items().isEmpty(), 5'000);
    // Provider loss withdraws the acknowledgment exactly once per observer
    // path that notices it (registrar owner loss and the client's owner-loss
    // signal race; the registrar deduplicates the wire signal).
    QTRY_COMPARE_WITH_TIMEOUT(hosted.size(), hostedBeforeFocusRetirement + 2, 5'000);
    QCOMPARE(hosted.constLast().at(0).toString(),
             hosted.constFirst().at(0).toString());
    QCOMPARE(hosted.constLast().at(1).toString(),
             QStringLiteral("/ReplacementMenu"));
    QCOMPARE(hosted.constLast().at(2).toBool(), false);
    QTRY_VERIFY_WITH_TIMEOUT(!registrar.registry()->registrationFor(77).has_value(), 5'000);

    coordinator.stop();
    registrar.stop();
    QDBusConnection::disconnectFromBus(shellName);
    QDBusConnection::disconnectFromBus(registrarName);
}

void GlobalMenuTransportRegistrationTest::
announcedNativeAddressUsesExactOwnerAndClearsOnLoss()
{
    DbusMenu::registerDbusMenuWireTypes();
    const QString providerName = QStringLiteral("qindaqt-native-menu-provider");
    const QString shellName = QStringLiteral("qindaqt-native-menu-shell");
    const QString announcedService = QStringLiteral("org.qindaqt.TestNativeMenu");
    auto providerBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, providerName);
    auto shellBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, shellName);
    QVERIFY(providerBus.isConnected());
    QVERIFY(shellBus.isConnected());
    QVERIFY(providerBus.registerService(announcedService));

    Test::FakeDbusMenuExporter exporter;
    exporter.setLayout(1, Test::menuLayout(QStringLiteral("_Native")));
    QVERIFY(providerBus.registerObject(
        QStringLiteral("/NativeMenu"), &exporter,
        QDBusConnection::ExportScriptableSlots
            | QDBusConnection::ExportScriptableSignals
            | QDBusConnection::ExportScriptableProperties));

    const QUuid windowId = QUuid::createUuid();
    FakeActiveWindowSource active;
    active.observation = Ownership::ActiveWindowObservation{
        .window = Ownership::WindowIdentity{
            .windowId = windowId,
            .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
        .focusGeneration = 9};
    FakeRegistrarWindowIdSource registrarIds;
    FakeAnnouncedMenuSource announced;
    announced.expectedWindow = windowId;
    announced.address = Composition::AnnouncedMenuAddress{
        .serviceName = announcedService,
        .objectPath = QStringLiteral("/NativeMenu")};
    Registrar::RegistrarRegistry unusedRegistry;
    GlobalMenuAppletAccess applet;
    applet.attachRenderer();
    Composition::GlobalMenuTransportCoordinator coordinator(
        shellBus, active, registrarIds, announced, unusedRegistry, applet);
    QSignalSpy hosted(&coordinator,
                      &Composition::GlobalMenuTransportCoordinator::hostedMenuChanged);
    coordinator.refreshFocus();

    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(hosted.size(), 1, 5'000);
    QCOMPARE(hosted.constFirst().at(2).toBool(), true);
    QCOMPARE(applet.items().constFirst().toMap().value(QStringLiteral("text")),
             QStringLiteral("Native"));
    QSignalSpy observed(&exporter, &Test::FakeDbusMenuExporter::eventObserved);
    applet.activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(observed.size(), 1, 5'000);

    QDBusConnection::disconnectFromBus(providerName);
    providerBus = QDBusConnection(QStringLiteral("qindaqt-retired-native-provider"));
    QTRY_VERIFY_WITH_TIMEOUT(!applet.available(), 5'000);
    // Both the announced-service owner change and the client's owner loss
    // observe the same teardown and each withdraws the bound endpoint; the
    // registrar deduplicates the wire signal, so the application sees one.
    QTRY_COMPARE_WITH_TIMEOUT(hosted.size(), 3, 5'000);
    QCOMPARE(hosted.constLast().at(0).toString(),
             hosted.constFirst().at(0).toString());
    QCOMPARE(hosted.constLast().at(1).toString(), QStringLiteral("/NativeMenu"));
    QCOMPARE(hosted.constLast().at(2).toBool(), false);
    QVERIFY(applet.items().isEmpty());

    coordinator.stop();
    QDBusConnection::disconnectFromBus(shellName);
}


QTEST_GUILESS_MAIN(GlobalMenuTransportRegistrationTest)
#include "tst_global_menu_transport_registration.moc"
