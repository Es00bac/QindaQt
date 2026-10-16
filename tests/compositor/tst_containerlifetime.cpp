// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0384: every dissolution path forgets the dissolved container's state.
//
// Each path below dissolves a maximized, renamed container through the real
// HybridInteractionRuntime (the same command the session dispatches for that
// path), then publishes the new topology to the ContainerLifetimeLedger the
// session reconciles at the start of every synchronizeChrome(). Before the
// ledger, the paths that forgot nothing left the maximize record behind and
// every later work-area change logged "container '...' has no valid maximize
// area" (350 times for one dissolved container on 2026-10-16).

#include "containerlifetimeledger.h"
#include "hybridcontainerappearance.h"
#include "hybridcontainerplacement.h"
#include "hybridinteractionruntime_testfixtures.h"

#include <qindaqt/window_management/grouping_planner.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::Compositor::KWinIntegration::Test;

namespace {

const QString WindowA = QStringLiteral("window-a");
const QString WindowB = QStringLiteral("window-b");
const QString WindowC = QStringLiteral("window-c");
const QString WindowD = QStringLiteral("window-d");

CommittedContainerLayout layoutFor()
{
    HybridConstraints::ConstraintSolution solution{
        .outerFrame = QRect(100, 100, 800, 600),
        .contentFrame = QRect(101, 129, 798, 570),
        .requiredContentSize = {},
        .overflow = {},
        .members = {},
        .splits = {},
    };
    return {solution.outerFrame, std::move(solution)};
}

// The session's per-container state outside topology, wired to the runtime
// exactly the way KWinHybridSession wires it (placement and appearance; the
// shade controller and minimized set need KWin and follow the same callback).
class Session final
{
public:
    explicit Session(HybridInput::DockZone zone)
        : runtime({WindowA, WindowB, WindowC, WindowD}, scene)
        , placement(
              [this]() -> const Hybrid::WindowTopology & { return runtime.topology(); },
              [this](const QString &id) -> std::optional<CommittedContainerLayout> {
                  return runtime.topology().container(id)
                      ? std::optional<CommittedContainerLayout>(layoutFor())
                      : std::nullopt;
              },
              [](const Core::WindowContainer &, const QRect &) {
                  return Hybrid::SceneStepResult::ready();
              },
              // A dissolved container has no representative and so no area,
              // exactly like KWinHybridSession::workArea().
              [this](const QString &id) {
                  return runtime.topology().container(id) ? QRect(0, 0, 1920, 1040) : QRect();
              })
        , ledger([this](const QString &id) {
              placement.forgetContainer(id);
              appearance.forgetContainer(id);
              forgotten.append(id);
          })
    {
        if (!dock(runtime, WindowA, WindowB, zone).topologyChanged()) {
            qFatal("could not group the fixture windows");
        }
        containerId = runtime.topology().containerIds().constFirst();
        ledger.reconcile(runtime.topology().containerIds());
        QString error;
        if (!placement.maximize(containerId, &error)
            || !appearance.setName(containerId, QStringLiteral("Agents"), &error)) {
            qFatal("could not prepare the fixture container: %s", qPrintable(error));
        }
    }

    // What the session does after a topology change: publish, i.e. reconcile.
    void publish() { ledger.reconcile(runtime.topology().containerIds()); }

    [[nodiscard]] QString pageOf(const QString &windowId) const
    {
        const auto *container = runtime.topology().container(containerId);
        for (const auto &page : container->pages()) {
            if (page.root().findWindow(windowId)) {
                return page.id();
            }
        }
        return {};
    }

    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime;
    HybridContainerPlacementController placement;
    HybridContainerAppearanceStore appearance;
    QStringList forgotten;
    ContainerLifetimeLedger ledger;
    QString containerId;
};

void verifyForgotten(Session &session, const HybridRuntimeResult &dissolved)
{
    QVERIFY2(dissolved.topologyChanged(), qPrintable(dissolved.message));
    QVERIFY(!session.runtime.topology().container(session.containerId));
    // The symptom: stale maximize state fails on the next work-area change.
    QCOMPARE(session.placement.refreshMaximizedAreas(),
             QStringList{QStringLiteral("container '%1' has no valid maximize area")
                             .arg(session.containerId)});
    session.publish();
    QCOMPARE(session.forgotten, QStringList{session.containerId});
    QVERIFY(!session.placement.isMaximized(session.containerId));
    QVERIFY(session.placement.refreshMaximizedAreas().isEmpty());
    QVERIFY(!session.appearance.hasCustomName(session.containerId));
    // A later publication never forgets it twice.
    session.publish();
    QCOMPARE(session.forgotten.size(), 1);
}

} // namespace

class ContainerLifetimeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reconcileForgetsOnlyVanishedIdsOnce();
    void explicitForgetIsNotRepeated();
    void forgetsAfterClosingAMember();
    void forgetsAfterANativeTitleDragOut();
    void forgetsAfterTheContextMenuUngroup();
    void forgetsAfterASharedChromeTabDragOut();
    void forgetsAfterKeyboardDetach();
    void forgetsAfterKeyboardGroupingMovesTheLastSibling();
    void keepsStateOfASurvivingContainer();
};

void ContainerLifetimeTest::reconcileForgetsOnlyVanishedIdsOnce()
{
    QStringList forgotten;
    ContainerLifetimeLedger ledger([&](const QString &id) { forgotten.append(id); });
    QVERIFY(ledger.reconcile({QStringLiteral("b"), QStringLiteral("a")}).isEmpty());
    QVERIFY(ledger.knows(QStringLiteral("a")));
    QCOMPARE(ledger.reconcile({QStringLiteral("c")}),
             (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(forgotten, (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
    QVERIFY(ledger.reconcile({QStringLiteral("c")}).isEmpty());
    QCOMPARE(forgotten.size(), 2);
}

void ContainerLifetimeTest::explicitForgetIsNotRepeated()
{
    QStringList forgotten;
    ContainerLifetimeLedger ledger([&](const QString &id) { forgotten.append(id); });
    ledger.reconcile({QStringLiteral("a")});
    ledger.forget(QStringLiteral("a"));
    QVERIFY(ledger.reconcile({}).isEmpty());
    QCOMPARE(forgotten, QStringList{QStringLiteral("a")});
}

void ContainerLifetimeTest::forgetsAfterClosingAMember()
{
    Session session(HybridInput::DockZone::Right);
    verifyForgotten(session, session.runtime.forgetWindow(WindowB));
}

void ContainerLifetimeTest::forgetsAfterANativeTitleDragOut()
{
    // KWinHybridSession::detachNativeMember: MemberDock with no target.
    Session session(HybridInput::DockZone::Right);
    verifyForgotten(session, session.runtime.handleIntent(memberCommit(
                                 WindowA, session.containerId,
                                 HybridInput::HitKind::MemberTitle, {}, {},
                                 HybridInput::DockZone::None)));
}

void ContainerLifetimeTest::forgetsAfterTheContextMenuUngroup()
{
    Session session(HybridInput::DockZone::Right);
    verifyForgotten(session, session.runtime.releaseContainer(session.containerId));
}

void ContainerLifetimeTest::forgetsAfterASharedChromeTabDragOut()
{
    // The kwinhybridinteraction.cpp dispatchIntent path: a shared-chrome tab
    // dragged onto an independent window takes its page along and leaves the
    // source container with one window, which dissolves.
    Session session(HybridInput::DockZone::Tab);
    const QString page = session.pageOf(WindowA);
    QVERIFY(!page.isEmpty());
    verifyForgotten(session, session.runtime.handleIntent(memberCommit(
                                 WindowA, session.containerId, HybridInput::HitKind::Tab,
                                 WindowC, {}, HybridInput::DockZone::Tab, page)));
}

void ContainerLifetimeTest::forgetsAfterKeyboardDetach()
{
    // kwinhybridwindowmanagement.cpp Operation::Detach.
    Session session(HybridInput::DockZone::Right);
    verifyForgotten(session,
                    session.runtime.execute(Hybrid::DetachMember{session.containerId, WindowA}));
}

void ContainerLifetimeTest::forgetsAfterKeyboardGroupingMovesTheLastSibling()
{
    // kwinhybridwindowmanagement.cpp Operation::GroupTab/GroupTile: moving one
    // of two members into another group dissolves the source container.
    Session session(HybridInput::DockZone::Right);
    const auto plan = WindowManagement::planGrouping(
        session.runtime.topology(),
        {WindowA, WindowC, WindowManagement::GroupingMode::Tab,
         WindowManagement::GroupingDirection::Right, 0.5,
         QUuid::createUuid().toString(QUuid::WithoutBraces)});
    QVERIFY2(plan.command, qPrintable(plan.error));
    verifyForgotten(session, session.runtime.execute(*plan.command));
}

void ContainerLifetimeTest::keepsStateOfASurvivingContainer()
{
    Session session(HybridInput::DockZone::Right);
    // Adding a third member changes topology but dissolves nothing.
    QVERIFY(session.runtime.handleIntent(memberCommit(
                WindowC, {}, HybridInput::HitKind::MemberTitle, WindowB,
                session.containerId, HybridInput::DockZone::Bottom))
                .topologyChanged());
    session.publish();
    QVERIFY(session.forgotten.isEmpty());
    QVERIFY(session.placement.isMaximized(session.containerId));
    QVERIFY(session.appearance.hasCustomName(session.containerId));
}

QTEST_GUILESS_MAIN(ContainerLifetimeTest)
#include "tst_containerlifetime.moc"
