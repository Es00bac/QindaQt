// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>
#include <qindaqt/shell/global_menu/registrar/appmenu_registrar.h>

#include "fake_dbusmenu_exporter.h"

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Shell::GlobalMenu;

namespace
{

class FakeActiveWindowSource final : public Ownership::ActiveWindowSource
{
public:
    std::optional<Ownership::ActiveWindowObservation> observation;

    [[nodiscard]] std::optional<Ownership::ActiveWindowObservation> activeWindow() const override
    {
        return observation;
    }
};

class FakeRegistrarWindowIdSource final : public Composition::RegistrarWindowIdSource
{
public:
    QUuid expectedWindow;
    quint32 registrarId = 0;

    [[nodiscard]] std::optional<quint32> registrarWindowIdFor(
        const Ownership::WindowIdentity &window) const override
    {
        return window.windowId == expectedWindow && registrarId != 0
            ? std::optional<quint32>(registrarId)
            : std::nullopt;
    }
};

class FakeAnnouncedMenuSource final
    : public Composition::AnnouncedMenuAddressSource
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

QDBusMessage registrarCall(const QDBusConnection &connection, const QString &method,
                           const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(Registrar::kRegistrarServiceName),
        QString::fromLatin1(Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Registrar::kRegistrarInterface), method);
    message.setArguments(arguments);
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        (void)finished.wait(5'000);
    }
    return watcher.reply();
}

} // namespace

class GlobalMenuTransportCompositionTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void focusedRegistrationPublishesAndActivatesExactlyOnce();
    void announcedNativeAddressUsesExactOwnerAndClearsOnLoss();
    void transientIdentityWithdrawalRetainsPresentation();
    void unansweredWithdrawalClearsPresentationAfterGrace();
    void sameWindowGenerationMoveRenewsWithoutRepublish();
};

void GlobalMenuTransportCompositionTest::focusedRegistrationPublishesAndActivatesExactlyOnce()
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
    // Authority is revoked synchronously: nothing is activatable while no
    // authenticated focus exists.
    QVERIFY(!applet.available());
    // Ordinary focus retirement retains the proven endpoint so the inactive
    // application's content height does not jump.
    QCOMPARE(hosted.size(), hostedBeforeFocusRetirement);
    // The last presentation is retained as an inert placeholder through the
    // bounded grace window instead of collapsing the panel slot.
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

void GlobalMenuTransportCompositionTest::
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

namespace
{

// Shared private-bus binding for the flicker-regression rows: one registrar,
// one fake provider owning window 77's menu at /Menu, one focused observation.
struct BoundFixture {
    QString registrarName;
    QString providerName;
    QString shellName;
    // QDBusConnection has no default constructor; a named-but-unconnected
    // placeholder is overwritten by connectToBus in bindFixture.
    QDBusConnection registrarBus = QDBusConnection(QStringLiteral("placeholder-r"));
    QDBusConnection providerBus = QDBusConnection(QStringLiteral("placeholder-p"));
    QDBusConnection shellBus = QDBusConnection(QStringLiteral("placeholder-s"));
    std::unique_ptr<Test::FakeDbusMenuExporter> exporter;
    std::unique_ptr<Registrar::AppMenuRegistrar> registrar;
    QUuid windowId;
    FakeActiveWindowSource active;
    FakeRegistrarWindowIdSource resolver;
    quint64 generation = 5;
};

bool bindFixture(BoundFixture &fixture, const QString &tag)
{
    DbusMenu::registerDbusMenuWireTypes();
    Registrar::registerRegistrarWireTypes();
    fixture.registrarName = QStringLiteral("qindaqt-flicker-registrar-%1").arg(tag);
    fixture.providerName = QStringLiteral("qindaqt-flicker-provider-%1").arg(tag);
    fixture.shellName = QStringLiteral("qindaqt-flicker-shell-%1").arg(tag);
    fixture.registrarBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, fixture.registrarName);
    fixture.providerBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, fixture.providerName);
    fixture.shellBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, fixture.shellName);
    if (!fixture.registrarBus.isConnected() || !fixture.providerBus.isConnected()
        || !fixture.shellBus.isConnected()) {
        return false;
    }
    fixture.exporter = std::make_unique<Test::FakeDbusMenuExporter>();
    fixture.exporter->setLayout(1, Test::menuLayout());
    if (!fixture.providerBus.registerObject(
            QStringLiteral("/Menu"), fixture.exporter.get(),
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
                | QDBusConnection::ExportScriptableProperties)) {
        return false;
    }
    fixture.registrar = std::make_unique<Registrar::AppMenuRegistrar>(fixture.registrarBus);
    if (fixture.registrar->start() != Registrar::RegistrarStartStatus::Started) {
        return false;
    }
    if (registrarCall(fixture.providerBus, QStringLiteral("RegisterWindow"),
                      {QVariant::fromValue(quint32{77}),
                       QVariant::fromValue(QDBusObjectPath(QStringLiteral("/Menu")))})
            .type()
        != QDBusMessage::ReplyMessage) {
        return false;
    }
    fixture.windowId = QUuid::createUuid();
    fixture.active.observation = Ownership::ActiveWindowObservation{
        .window = Ownership::WindowIdentity{
            .windowId = fixture.windowId,
            .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
        .focusGeneration = fixture.generation};
    fixture.resolver.expectedWindow = fixture.windowId;
    fixture.resolver.registrarId = 77;
    return true;
}

void teardownFixture(BoundFixture &fixture)
{
    fixture.registrar->stop();
    QDBusConnection::disconnectFromBus(fixture.providerName);
    QDBusConnection::disconnectFromBus(fixture.shellName);
    QDBusConnection::disconnectFromBus(fixture.registrarName);
}

} // namespace

void GlobalMenuTransportCompositionTest::transientIdentityWithdrawalRetainsPresentation()
{
    BoundFixture fixture;
    QVERIFY(bindFixture(fixture, QStringLiteral("transient")));
    GlobalMenuAppletAccess applet;
    applet.attachRenderer();
    Composition::GlobalMenuTransportCoordinator coordinator(
        fixture.shellBus, fixture.active, fixture.resolver, *fixture.registrar->registry(),
        applet);
    coordinator.refreshFocus();
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    const QString generation =
        applet.items().first().toMap().value(QStringLiteral("generation")).toString();
    QSignalSpy republished(&applet, &GlobalMenuAppletAccess::itemsChanged);

    // The compositor identity channel invalidates and republishes on
    // visibility changes that are not focus moves; between withdrawal and
    // reread the source observes no identity at all.
    fixture.active.observation.reset();
    coordinator.refreshFocus();
    QVERIFY(!applet.available());
    QVERIFY(!applet.items().isEmpty());
    applet.activate(QStringLiteral("1"));
    QTest::qWait(100);
    QCOMPARE(fixture.exporter->eventCount(), 0);

    fixture.generation = 6;
    fixture.active.observation = Ownership::ActiveWindowObservation{
        .window = Ownership::WindowIdentity{
            .windowId = fixture.windowId,
            .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
        .focusGeneration = fixture.generation};
    coordinator.refreshFocus();
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    // The placeholder resumed in place: no projection replacement means the
    // panel keeps its extent and every delegate survives the cycle.
    QCOMPARE(republished.size(), 0);
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("generation")).toString(),
             generation);
    applet.activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(fixture.exporter->eventCount(), 1, 5'000);

    coordinator.stop();
    teardownFixture(fixture);
}

void GlobalMenuTransportCompositionTest::unansweredWithdrawalClearsPresentationAfterGrace()
{
    BoundFixture fixture;
    QVERIFY(bindFixture(fixture, QStringLiteral("grace")));
    GlobalMenuAppletAccess applet;
    applet.attachRenderer();
    Composition::GlobalMenuTransportCoordinator coordinator(
        fixture.shellBus, fixture.active, fixture.resolver, *fixture.registrar->registry(),
        applet);
    coordinator.refreshFocus();
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);

    fixture.active.observation.reset();
    coordinator.refreshFocus();
    QVERIFY(!applet.available());
    QVERIFY(!applet.items().isEmpty());
    // No reread re-proves the provider: after the bounded grace the retained
    // placeholder must give way to the truthful unavailable state.
    QTRY_VERIFY_WITH_TIMEOUT(applet.items().isEmpty(), 5'000);
    QVERIFY(!applet.available());

    coordinator.stop();
    teardownFixture(fixture);
}

void GlobalMenuTransportCompositionTest::sameWindowGenerationMoveRenewsWithoutRepublish()
{
    BoundFixture fixture;
    QVERIFY(bindFixture(fixture, QStringLiteral("renewal")));
    GlobalMenuAppletAccess applet;
    applet.attachRenderer();
    Composition::GlobalMenuTransportCoordinator coordinator(
        fixture.shellBus, fixture.active, fixture.resolver, *fixture.registrar->registry(),
        applet);
    coordinator.refreshFocus();
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    QVERIFY(coordinator.publishedTree().has_value());
    const QUuid initialEpoch = coordinator.publishedTree()->epoch;
    const QString generation =
        applet.items().first().toMap().value(QStringLiteral("generation")).toString();
    QSignalSpy republished(&applet, &GlobalMenuAppletAccess::itemsChanged);

    // A visibility revision move with the same focused window is not a focus
    // change: lineage renews in place with no presentation churn at all.
    fixture.generation = 6;
    fixture.active.observation = Ownership::ActiveWindowObservation{
        .window = Ownership::WindowIdentity{
            .windowId = fixture.windowId,
            .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
        .focusGeneration = fixture.generation};
    coordinator.refreshFocus();
    QVERIFY(applet.available());
    QCOMPARE(republished.size(), 0);
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("generation")).toString(),
             generation);
    QVERIFY(coordinator.publishedTree().has_value());
    QCOMPARE(coordinator.publishedTree()->epoch, initialEpoch);
    applet.activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(fixture.exporter->eventCount(), 1, 5'000);

    coordinator.stop();
    teardownFixture(fixture);
}

QTEST_GUILESS_MAIN(GlobalMenuTransportCompositionTest)
#include "tst_global_menu_transport_composition.moc"
