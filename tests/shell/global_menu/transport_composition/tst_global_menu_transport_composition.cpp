// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>
#include <qindaqt/shell/global_menu/registrar/appmenu_registrar.h>

#include "fake_dbusmenu_exporter.h"

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusContext>
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
    void repeatedChurnKeepsOpenMenuInteractive();
    void unansweredWithdrawalClearsPresentationAfterGrace();
    void sameWindowGenerationMoveRenewsWithoutRepublish();
    void identicalContentReplacementEndsTransition();
    void replacementEndpointWithRejectedFirstLayoutClearsRetainedProjection();
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

// Endpoint that errors every GetLayout while counting attempts: proves a
// rejected first layout neither hangs the retained placeholder nor spins
// rebinds against an unservable path. (File scope: moc cannot see Q_OBJECT
// classes inside an anonymous namespace.)
class FailingDbusMenuObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
    Q_PROPERTY(quint32 Version READ version SCRIPTABLE true)
    Q_PROPERTY(QString Status READ status SCRIPTABLE true)
    Q_PROPERTY(QString TextDirection READ textDirection SCRIPTABLE true)

public:
    [[nodiscard]] quint32 version() const noexcept { return 4; }
    [[nodiscard]] QString status() const { return QStringLiteral("normal"); }
    [[nodiscard]] QString textDirection() const { return QStringLiteral("ltr"); }
    [[nodiscard]] int layoutCallCount() const noexcept { return m_layoutCallCount; }

public Q_SLOTS:
    Q_SCRIPTABLE void GetLayout(qint32, qint32, const QStringList &, quint32 &,
                                DbusMenu::LayoutItem &)
    {
        ++m_layoutCallCount;
        sendErrorReply(QStringLiteral("org.qindaqt.test.MissingMenu"),
                       QStringLiteral("no menu exported at this path"));
    }

private:
    int m_layoutCallCount = 0;
};

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
    QSignalSpy availability(&applet, &GlobalMenuAppletAccess::availableChanged);
    QSignalSpy rejected(&coordinator,
                        &Composition::GlobalMenuTransportCoordinator::activationRejected);

    // The compositor identity channel invalidates and republishes on
    // visibility changes that are not focus moves; between withdrawal and
    // reread the source observes no identity at all. The facade must stay
    // fully live — dropping availability here closes an open popup and
    // disables delegates between press and release.
    fixture.active.observation.reset();
    coordinator.refreshFocus();
    QVERIFY(applet.available());
    QCOMPARE(applet.phase(), QStringLiteral("ready"));
    QVERIFY(!applet.items().isEmpty());
    // Execution authority is still revoked synchronously: the selector is
    // cleared, so the invocation guard rejects and no Event crosses.
    applet.activate(QStringLiteral("1"));
    QTest::qWait(100);
    QCOMPARE(fixture.exporter->eventCount(), 0);
    QCOMPARE(rejected.size(), 1);
    QCOMPARE(rejected.constFirst().constFirst().toString(),
             QStringLiteral("no-active-provider"));

    fixture.generation = 6;
    fixture.active.observation = Ownership::ActiveWindowObservation{
        .window = Ownership::WindowIdentity{
            .windowId = fixture.windowId,
            .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
        .focusGeneration = fixture.generation};
    coordinator.refreshFocus();
    QVERIFY(applet.available());
    // The binding resumed in place with zero facade signals: the panel keeps
    // its extent, every delegate survives, and an open popup is never closed
    // by a same-provider churn cycle.
    QCOMPARE(republished.size(), 0);
    QCOMPARE(availability.size(), 0);
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("generation")).toString(),
             generation);
    applet.activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(fixture.exporter->eventCount(), 1, 5'000);

    coordinator.stop();
    teardownFixture(fixture);
}

void GlobalMenuTransportCompositionTest::repeatedChurnKeepsOpenMenuInteractive()
{
    BoundFixture fixture;
    QVERIFY(bindFixture(fixture, QStringLiteral("churn")));
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
    QSignalSpy availability(&applet, &GlobalMenuAppletAccess::availableChanged);
    QSignalSpy rejected(&coordinator,
                        &Composition::GlobalMenuTransportCoordinator::activationRejected);

    // Live-session regression: the compositor invalidates and republishes the
    // active-window identity on every visibility-affecting change — including
    // the menu popup's own surface appearing — so an open menu meets a
    // withdraw/reread cycle exactly when the user is about to click. Repeated
    // identical re-proofs must be interaction-neutral: no availability drop
    // (which closes the popup and disables delegates between press and
    // release), no projection rebuild, and activation works afterwards.
    for (int cycle = 0; cycle < 5; ++cycle) {
        fixture.active.observation.reset();
        coordinator.refreshFocus();
        QVERIFY(applet.available());
        QVERIFY(!applet.items().isEmpty());
        if (cycle == 2) {
            // A click landing inside an uncertain reread window is fenced by
            // the invocation guard — rejected, never executed.
            applet.activate(QStringLiteral("1"));
        }
        fixture.active.observation = Ownership::ActiveWindowObservation{
            .window = Ownership::WindowIdentity{
                .windowId = fixture.windowId,
                .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
            .focusGeneration = fixture.generation};
        coordinator.refreshFocus();
        QVERIFY(applet.available());
    }
    QTest::qWait(100);
    QCOMPARE(availability.size(), 0);
    QCOMPARE(republished.size(), 0);
    QCOMPARE(fixture.exporter->eventCount(), 0);
    QCOMPARE(rejected.size(), 1);
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("generation")).toString(),
             generation);

    applet.activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(fixture.exporter->eventCount(), 1, 5'000);
    QTest::qWait(100);
    QCOMPARE(fixture.exporter->eventCount(), 1);

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
    // Presentation and interactivity are retained through the grace window —
    // execution authority alone is revoked (selector cleared) — so an open
    // popup is not destroyed by a withdrawal that may still re-prove.
    QVERIFY(applet.available());
    QVERIFY(!applet.items().isEmpty());
    applet.activate(QStringLiteral("1"));
    QTest::qWait(100);
    QCOMPARE(fixture.exporter->eventCount(), 0);
    // No reread re-proves the provider: after the bounded grace the retained
    // presentation must give way to the truthful unavailable state.
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

void GlobalMenuTransportCompositionTest::identicalContentReplacementEndsTransition()
{
    BoundFixture fixture;
    QVERIFY(bindFixture(fixture, QStringLiteral("twin")));
    // A replacement endpoint serving byte-identical content: the bind opens an
    // inert transition, and the replacement's first accepted layout must end
    // it through publishTree even though nothing visibly changed — the
    // placeholder can never hang in loading.
    Test::FakeDbusMenuExporter twinExporter;
    twinExporter.setLayout(1, Test::menuLayout());
    QVERIFY(fixture.providerBus.registerObject(
        QStringLiteral("/TwinMenu"), &twinExporter,
        QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
            | QDBusConnection::ExportScriptableProperties));
    GlobalMenuAppletAccess applet;
    applet.attachRenderer();
    Composition::GlobalMenuTransportCoordinator coordinator(
        fixture.shellBus, fixture.active, fixture.resolver, *fixture.registrar->registry(),
        applet);
    coordinator.refreshFocus();
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    QVERIFY(coordinator.publishedTree().has_value());
    const QUuid initialEpoch = coordinator.publishedTree()->epoch;
    const quint64 initialRevision = coordinator.publishedTree()->revision;

    QCOMPARE(registrarCall(fixture.providerBus, QStringLiteral("RegisterWindow"),
                           {QVariant::fromValue(quint32{77}),
                            QVariant::fromValue(
                                QDBusObjectPath(QStringLiteral("/TwinMenu")))})
                 .type(),
             QDBusMessage::ReplyMessage);
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);
    QCOMPARE(applet.phase(), QStringLiteral("ready"));
    QCOMPARE(applet.items().first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("File"));
    QVERIFY(coordinator.publishedTree().has_value());
    QCOMPARE(coordinator.publishedTree()->epoch, initialEpoch);
    QVERIFY(coordinator.publishedTree()->revision > initialRevision);

    // Activation lands on the replacement endpoint exactly once; the retired
    // exporter observes nothing.
    applet.activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(twinExporter.eventCount(), 1, 5'000);
    QTest::qWait(100);
    QCOMPARE(fixture.exporter->eventCount(), 0);

    coordinator.stop();
    teardownFixture(fixture);
}

void GlobalMenuTransportCompositionTest::
    replacementEndpointWithRejectedFirstLayoutClearsRetainedProjection()
{
    BoundFixture fixture;
    QVERIFY(bindFixture(fixture, QStringLiteral("rejected")));
    FailingDbusMenuObject failing;
    QVERIFY(fixture.providerBus.registerObject(
        QStringLiteral("/MissingMenu"), &failing,
        QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableProperties));
    GlobalMenuAppletAccess applet;
    applet.attachRenderer();
    Composition::GlobalMenuTransportCoordinator coordinator(
        fixture.shellBus, fixture.active, fixture.resolver, *fixture.registrar->registry(),
        applet);
    coordinator.refreshFocus();
    QTRY_VERIFY_WITH_TIMEOUT(applet.available(), 5'000);

    // The registrar proves a replacement endpoint for the same window at a
    // path whose first GetLayout is rejected. The previous provider's
    // projection is retained only as an inert transition placeholder...
    QCOMPARE(registrarCall(fixture.providerBus, QStringLiteral("RegisterWindow"),
                           {QVariant::fromValue(quint32{77}),
                            QVariant::fromValue(
                                QDBusObjectPath(QStringLiteral("/MissingMenu")))})
                 .type(),
             QDBusMessage::ReplyMessage);
    QTRY_VERIFY_WITH_TIMEOUT(!applet.available(), 5'000);
    QTRY_VERIFY_WITH_TIMEOUT(!applet.items().isEmpty(), 5'000);
    // ...and once the replacement's first layout is rejected, the placeholder
    // clears to the truthful unavailable state instead of hanging in loading.
    QTRY_VERIFY_WITH_TIMEOUT(applet.items().isEmpty(), 5'000);
    QVERIFY(!applet.available());
    QCOMPARE(applet.phase(), QStringLiteral("unavailable"));
    QTRY_COMPARE_WITH_TIMEOUT(failing.layoutCallCount(), 1, 5'000);
    const int rejectedCalls = failing.layoutCallCount();

    // The clear does not rebind on its own: GetLayout attempts stay flat
    // instead of spinning against the unservable endpoint.
    QTest::qWait(700);
    QCOMPARE(failing.layoutCallCount(), rejectedCalls);
    QVERIFY(!applet.available());
    QVERIFY(applet.items().isEmpty());

    coordinator.stop();
    teardownFixture(fixture);
}

QTEST_GUILESS_MAIN(GlobalMenuTransportCompositionTest)
#include "tst_global_menu_transport_composition.moc"
