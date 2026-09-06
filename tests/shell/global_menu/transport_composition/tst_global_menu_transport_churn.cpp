// SPDX-License-Identifier: GPL-3.0-or-later
#include "transport_composition_fixture.h"

#include <QtDBus/QDBusContext>
#include <QtTest/QTest>

using namespace QindaQt::Shell::GlobalMenu;
using namespace QindaQt::Shell::GlobalMenu::TestSupport;

class GlobalMenuTransportChurnTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void transientIdentityWithdrawalRetainsPresentation();
    void repeatedChurnKeepsOpenMenuInteractive();
    void unansweredWithdrawalClearsPresentationAfterGrace();
    void sameWindowGenerationMoveRenewsWithoutRepublish();
    void identicalContentReplacementEndsTransition();
    void replacementEndpointWithRejectedFirstLayoutClearsRetainedProjection();
};

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

void GlobalMenuTransportChurnTest::transientIdentityWithdrawalRetainsPresentation()
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

void GlobalMenuTransportChurnTest::repeatedChurnKeepsOpenMenuInteractive()
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

void GlobalMenuTransportChurnTest::unansweredWithdrawalClearsPresentationAfterGrace()
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

void GlobalMenuTransportChurnTest::sameWindowGenerationMoveRenewsWithoutRepublish()
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

void GlobalMenuTransportChurnTest::identicalContentReplacementEndsTransition()
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

void GlobalMenuTransportChurnTest::
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


QTEST_GUILESS_MAIN(GlobalMenuTransportChurnTest)
#include "tst_global_menu_transport_churn.moc"
