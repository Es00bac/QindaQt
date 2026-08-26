// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene_testfixture.h"

#include "qindaqt/hybrid/topologycoordinator.h"

#include <QTest>

using namespace QindaQt;
using namespace QindaQt::Compositor;

namespace {

Hybrid::DockIndependentWindows dock(QString container,
                                    QString first,
                                    QString second)
{
    const auto pageId = QStringLiteral("page-") + first;
    const auto firstLeafId = QStringLiteral("leaf-first-") + first;
    const auto secondLeafId = QStringLiteral("leaf-second-") + second;
    const auto splitId = QStringLiteral("split-") + first + second;
    return {
        .containerId = std::move(container),
        .pageId = pageId,
        .firstWindowId = std::move(first),
        .firstLeafNodeId = firstLeafId,
        .secondWindowId = std::move(second),
        .secondLeafNodeId = secondLeafId,
        .splitNodeId = splitId,
        .orientation = Core::SplitOrientation::Horizontal,
        .ratio = 0.5,
        .secondPosition = Core::InsertPosition::Second,
    };
}

Hybrid::WindowTopology topology(QStringList independent)
{
    QString error;
    const auto result = Hybrid::WindowTopology::create(std::move(independent), {}, 0, &error);
    Q_ASSERT_X(result.has_value(), "topology fixture", qPrintable(error));
    return *result;
}

} // namespace

class KWinHybridSceneTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dockingAndReleaseRestoreEveryStateField();
    void prepareFailurePreservesExistingFocus();
    void failedOwnerFinalizeRollsBackAllState();
    void failedOwnerFinalizeRestoresUnfocusedWorkspace();
    void forgetToleratesDeadMemberAndRestoresSurvivor();
    void reportsConstraintOverflow();
    void committedLayoutIsCopiedAndRemovedTransactionally();
    void crossContainerMoveUsesOneOwnerFinalize();
    void reflowUpdatesMembersAndCommittedLayout();
    void reflowRejectsInvalidOrStaleSnapshotWithoutMutation();
    void reflowFinalizeFailureRollsBackStateFocusAndLayout();
    void reflowFailureRestoresUnfocusedWorkspace();
    void pageActivationMinimizesOldPageAndMovesFocus();
};

void KWinHybridSceneTest::prepareFailurePreservesExistingFocus()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 700, 500),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(800, 30, 600, 500),
                                        QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);

    // Simulate a close that raced ahead of the lifecycle observer. Preparation
    // must reject the stale topology without treating an uncaptured empty focus
    // token as an instruction to reset the still-active surviving window.
    platform.removeWindow(QStringLiteral("b"));
    const auto result = coordinator.execute(dock(QStringLiteral("c"),
                                                 QStringLiteral("a"),
                                                 QStringLiteral("b")));

    QCOMPARE(result.error, Hybrid::TopologyCommandError::ScenePrepareFailed);
    QCOMPARE(platform.activeWindowId(), QStringLiteral("a"));
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, first);
    QCOMPARE(repository.topology().revision(), quint64{0});
}

void KWinHybridSceneTest::dockingAndReleaseRestoreEveryStateField()
{
    Test::FakeHybridScenePlatform platform;
    auto first = Test::richState(QRectF(100, 120, 640, 480), QStringLiteral("output-a"), true);
    first.maximizedAxes = HybridConstraints::MaximizeAxis::Horizontal;
    first.fullscreen = true;
    first.keepAbove = true;
    auto second = Test::richState(QRectF(900, 80, 500, 700), QStringLiteral("output-b"));
    second.quickTileEdges = HybridConstraints::QuickTileEdge::Left
        | HybridConstraints::QuickTileEdge::Top;
    second.minimized = true;
    second.desktopIds = {QStringLiteral("desktop-b")};
    second.activityIds = {QStringLiteral("activity-b")};
    second.keepBelow = true;
    platform.addWindow(QStringLiteral("a"),
                       Test::fakeWindow(first, QRectF(20, 30, 800, 600)));
    platform.addWindow(QStringLiteral("b"),
                       Test::fakeWindow(second, QRectF(900, 30, 500, 600)));

    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform,
                                                     {.contentInsets = QMargins(5, 25, 5, 5),
                                                      .dividerThickness = 4});
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    auto dockCommand = dock(QStringLiteral("c"), QStringLiteral("a"),
                            QStringLiteral("b"));
    dockCommand.secondPosition = Core::InsertPosition::First;
    const auto grouped = coordinator.execute(dockCommand);
    QVERIFY2(grouped.committed(), qPrintable(grouped.message));
    QCOMPARE(platform.owners.value(QStringLiteral("a")), QStringLiteral("c"));
    QCOMPARE(platform.owners.value(QStringLiteral("b")), QStringLiteral("c"));
    // HybridInteractionRuntime encodes the stationary target as firstWindowId.
    QCOMPARE(factory.committedLayout(QStringLiteral("c"))->outerFrame,
             QRect(20, 30, 800, 600));
    QVERIFY(!platform.windows.value(QStringLiteral("a")).state.fullscreen);
    QVERIFY(!platform.windows.value(QStringLiteral("b")).state.isQuickTiled());
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state.outputId,
             QStringLiteral("output-a"));

    const auto released = coordinator.execute(Hybrid::ReleaseContainer{QStringLiteral("c")});
    QVERIFY2(released.committed(), qPrintable(released.message));
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, first);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state, second);
    QVERIFY(platform.owners.value(QStringLiteral("a")).isEmpty());
    QVERIFY(platform.owners.value(QStringLiteral("b")).isEmpty());
}

void KWinHybridSceneTest::failedOwnerFinalizeRollsBackAllState()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 700, 500),
                                       QStringLiteral("output-a"), true);
    auto second = Test::richState(QRectF(800, 30, 600, 500), QStringLiteral("output-b"));
    second.quickTileEdges = HybridConstraints::QuickTileEdge::Right;
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    platform.failFinalize = true;

    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    const auto result = coordinator.execute(dock(QStringLiteral("c"),
                                                 QStringLiteral("a"),
                                                 QStringLiteral("b")));

    QCOMPARE(result.error, Hybrid::TopologyCommandError::SceneCommitFailed);
    QCOMPARE(repository.topology().revision(), quint64(0));
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, first);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state, second);
    QVERIFY(platform.owners.value(QStringLiteral("a")).isEmpty());
    QVERIFY(platform.owners.value(QStringLiteral("b")).isEmpty());
}

void KWinHybridSceneTest::failedOwnerFinalizeRestoresUnfocusedWorkspace()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 700, 500),
                                       QStringLiteral("output-a"));
    const auto second = Test::richState(QRectF(800, 30, 600, 500),
                                        QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    platform.failFinalize = true;

    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    const auto result = coordinator.execute(dock(QStringLiteral("c"),
                                                 QStringLiteral("a"),
                                                 QStringLiteral("b")));

    QCOMPARE(result.error, Hybrid::TopologyCommandError::SceneCommitFailed);
    QVERIFY(platform.activeWindowId().isEmpty());
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, first);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state, second);
}

void KWinHybridSceneTest::forgetToleratesDeadMemberAndRestoresSurvivor()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 700, 500),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(800, 30, 600, 500),
                                        QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dock(QStringLiteral("c"), QStringLiteral("a"),
                                     QStringLiteral("b"))).committed());

    platform.removeWindow(QStringLiteral("b"));
    const auto forgotten = coordinator.execute(Hybrid::ForgetWindow{QStringLiteral("b")});
    QVERIFY2(forgotten.committed(), qPrintable(forgotten.message));
    QCOMPARE(repository.topology().independentWindowIds(), QStringList{QStringLiteral("a")});
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, first);
    QCOMPARE(platform.lastAllowedMissing, QSet<QString>{QStringLiteral("b")});
}

void KWinHybridSceneTest::reportsConstraintOverflow()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(0, 0, 200, 100), QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(400, 0, 200, 100), QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"),
                       Test::fakeWindow(first, first.geometry, QSize(180, 40)));
    platform.addWindow(QStringLiteral("b"),
                       Test::fakeWindow(second, second.geometry, QSize(180, 40)));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform,
                                                     {.contentInsets = {},
                                                      .dividerThickness = 10});
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dock(QStringLiteral("c"), QStringLiteral("a"),
                                     QStringLiteral("b"))).committed());

    const auto overflow = factory.overflowReport(QStringLiteral("c"));
    QVERIFY(overflow.has_value());
    QCOMPARE(overflow->missingSize, QSize(170, 0));
    QCOMPARE(platform.lastTargetFrames.value(QStringLiteral("a")).width(), 95.0);
    QCOMPARE(platform.lastTargetFrames.value(QStringLiteral("b")).width(), 95.0);
}

void KWinHybridSceneTest::committedLayoutIsCopiedAndRemovedTransactionally()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(25, 30, 320, 180),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(500, 30, 200, 180),
                                        QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(
        platform, {.contentInsets = QMargins(4, 20, 4, 4), .dividerThickness = 6});
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dock(QStringLiteral("c"), QStringLiteral("a"),
                                     QStringLiteral("b"))).committed());

    auto copied = factory.committedLayout(QStringLiteral("c"));
    QVERIFY(copied.has_value());
    QCOMPARE(copied->outerFrame, first.geometry.toRect());
    QCOMPARE(copied->activePage.outerFrame, copied->outerFrame);
    QCOMPARE(copied->activePage.members.size(), 2);
    copied->outerFrame = QRect(1, 2, 3, 4);
    QCOMPARE(factory.committedLayout(QStringLiteral("c"))->outerFrame,
             first.geometry.toRect());

    platform.failFinalize = true;
    const auto failedRelease = coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("c")});
    QCOMPARE(failedRelease.error, Hybrid::TopologyCommandError::SceneCommitFailed);
    QCOMPARE(factory.committedLayout(QStringLiteral("c"))->outerFrame,
             first.geometry.toRect());
    platform.failFinalize = false;
    QVERIFY(coordinator.execute(Hybrid::ReleaseContainer{QStringLiteral("c")}).committed());
    QVERIFY(!factory.committedLayout(QStringLiteral("c")).has_value());
    QCOMPARE(copied->activePage.members.size(), 2);
}

void KWinHybridSceneTest::crossContainerMoveUsesOneOwnerFinalize()
{
    Test::FakeHybridScenePlatform platform;
    for (int index = 0; index < 4; ++index) {
        const QString id(QChar(char16_t(u'a' + index)));
        const auto state = Test::richState(QRectF(index * 300, 0, 280, 200),
                                           QStringLiteral("output-a"), index == 0);
        platform.addWindow(id, Test::fakeWindow(state, state.geometry));
    }
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b"),
                                                    QStringLiteral("c"),
                                                    QStringLiteral("d")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dock(QStringLiteral("left"), QStringLiteral("a"),
                                     QStringLiteral("b"))).committed());
    QVERIFY(coordinator.execute(dock(QStringLiteral("right"), QStringLiteral("c"),
                                     QStringLiteral("d"))).committed());
    const int beforeCalls = platform.finalizeCalls;

    const Hybrid::MoveMember move{
        .sourceContainerId = QStringLiteral("left"),
        .targetContainerId = QStringLiteral("right"),
        .windowId = QStringLiteral("b"),
        .destination = Hybrid::MoveAsPage{QStringLiteral("page-b"), 1},
    };
    const auto result = coordinator.execute(move);
    QVERIFY2(result.committed(), qPrintable(result.message));
    QCOMPARE(platform.finalizeCalls, beforeCalls + 1);
    QCOMPARE(platform.lastExpectedOwners.value(QStringLiteral("a")),
             QStringLiteral("left"));
    QCOMPARE(platform.lastExpectedOwners.value(QStringLiteral("b")),
             QStringLiteral("left"));
    QVERIFY(platform.lastCandidateOwners.value(QStringLiteral("a")).isEmpty());
    QCOMPARE(platform.lastCandidateOwners.value(QStringLiteral("b")),
             QStringLiteral("right"));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("b")),
             std::optional<QString>(QStringLiteral("right")));
}

void KWinHybridSceneTest::reflowUpdatesMembersAndCommittedLayout()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 600, 400),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(700, 30, 500, 400),
                                        QStringLiteral("output-b"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(
        platform, {.contentInsets = QMargins(10, 20, 10, 10), .dividerThickness = 10});
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dock(QStringLiteral("c"), QStringLiteral("a"),
                                     QStringLiteral("b"))).committed());
    const int finalizeCalls = platform.finalizeCalls;
    const QRect requestedOuter(50, 60, 900, 400);

    const auto result = factory.reflowContainer(
        *repository.topology().container(QStringLiteral("c")), requestedOuter);
    QVERIFY2(result.succeeded, qPrintable(result.message));
    QCOMPARE(platform.finalizeCalls, finalizeCalls + 1);
    const auto committed = factory.committedLayout(QStringLiteral("c"));
    QVERIFY(committed.has_value());
    QCOMPARE(committed->outerFrame, requestedOuter);
    QCOMPARE(committed->activePage.outerFrame, requestedOuter);
    QCOMPARE(platform.lastTargetFrames.value(QStringLiteral("a")),
             QRectF(committed->activePage.members.value(QStringLiteral("a")).windowFrame));
    QCOMPARE(platform.lastTargetFrames.value(QStringLiteral("b")),
             QRectF(committed->activePage.members.value(QStringLiteral("b")).windowFrame));
    QCOMPARE(platform.owners.value(QStringLiteral("a")), QStringLiteral("c"));
    QCOMPARE(platform.owners.value(QStringLiteral("b")), QStringLiteral("c"));

    QVERIFY(coordinator.execute(Hybrid::ReleaseContainer{QStringLiteral("c")}).committed());
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, first);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state, second);
}

void KWinHybridSceneTest::reflowRejectsInvalidOrStaleSnapshotWithoutMutation()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 600, 400),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(700, 30, 500, 400),
                                        QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dock(QStringLiteral("c"), QStringLiteral("a"),
                                     QStringLiteral("b"))).committed());
    const auto beforeLayout = factory.committedLayout(QStringLiteral("c"));
    const auto beforeFirst = platform.windows.value(QStringLiteral("a")).state;
    const auto beforeSecond = platform.windows.value(QStringLiteral("b")).state;
    const int finalizeCalls = platform.finalizeCalls;

    QVERIFY(!factory.reflowContainer(
        *repository.topology().container(QStringLiteral("c")), QRect(10, 20, 0, 300))
                 .succeeded);
    auto stale = *repository.topology().container(QStringLiteral("c"));
    QString error;
    QVERIFY(stale.detachWindow(QStringLiteral("b"), &error).has_value());
    QVERIFY(!factory.reflowContainer(stale, QRect(10, 20, 700, 300)).succeeded);

    QCOMPARE(platform.finalizeCalls, finalizeCalls);
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, beforeFirst);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state, beforeSecond);
    QCOMPARE(factory.committedLayout(QStringLiteral("c")), beforeLayout);
}

void KWinHybridSceneTest::reflowFinalizeFailureRollsBackStateFocusAndLayout()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 600, 400),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(700, 30, 500, 400),
                                        QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dock(QStringLiteral("c"), QStringLiteral("a"),
                                     QStringLiteral("b"))).committed());
    const auto beforeLayout = factory.committedLayout(QStringLiteral("c"));
    const auto beforeFirst = platform.windows.value(QStringLiteral("a")).state;
    const auto beforeSecond = platform.windows.value(QStringLiteral("b")).state;
    platform.failFinalize = true;

    const auto result = factory.reflowContainer(
        *repository.topology().container(QStringLiteral("c")), QRect(90, 100, 1000, 500));
    QVERIFY(!result.succeeded);
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, beforeFirst);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state, beforeSecond);
    QCOMPARE(platform.activeWindowId(), QStringLiteral("a"));
    QCOMPARE(platform.owners.value(QStringLiteral("a")), QStringLiteral("c"));
    QCOMPARE(platform.owners.value(QStringLiteral("b")), QStringLiteral("c"));
    QCOMPARE(factory.committedLayout(QStringLiteral("c")), beforeLayout);
}

void KWinHybridSceneTest::reflowFailureRestoresUnfocusedWorkspace()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 600, 400),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(700, 30, 500, 400),
                                        QStringLiteral("output-a"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    QVERIFY(coordinator.execute(dock(QStringLiteral("c"), QStringLiteral("a"),
                                     QStringLiteral("b"))).committed());

    QVERIFY(platform.restoreFocus({}, nullptr));
    QVERIFY(platform.activeWindowId().isEmpty());
    platform.failFinalize = true;
    const auto result = factory.reflowContainer(
        *repository.topology().container(QStringLiteral("c")),
        QRect(90, 100, 1000, 500));

    QVERIFY(!result.succeeded);
    QVERIFY(platform.activeWindowId().isEmpty());
}

void KWinHybridSceneTest::pageActivationMinimizesOldPageAndMovesFocus()
{
    Test::FakeHybridScenePlatform platform;
    const auto first = Test::richState(QRectF(20, 30, 600, 400),
                                       QStringLiteral("output-a"), true);
    const auto second = Test::richState(QRectF(700, 30, 500, 400),
                                        QStringLiteral("output-b"));
    platform.addWindow(QStringLiteral("a"), Test::fakeWindow(first, first.geometry));
    platform.addWindow(QStringLiteral("b"), Test::fakeWindow(second, second.geometry));
    Hybrid::TopologyRepository repository(topology({QStringLiteral("a"),
                                                    QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory factory(platform);
    Hybrid::TopologyCoordinator coordinator(repository, factory);
    const Hybrid::GroupIndependentWindowsAsPages group{
        .containerId = QStringLiteral("c"),
        .firstWindowId = QStringLiteral("a"),
        .firstPageId = QStringLiteral("page-a"),
        .firstLeafNodeId = QStringLiteral("leaf-a"),
        .secondWindowId = QStringLiteral("b"),
        .secondPageId = QStringLiteral("page-b"),
        .secondLeafNodeId = QStringLiteral("leaf-b"),
    };
    QVERIFY(coordinator.execute(group).committed());
    QVERIFY(!platform.windows.value(QStringLiteral("a")).state.minimized);
    QVERIFY(platform.windows.value(QStringLiteral("b")).state.minimized);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state.outputId,
             QStringLiteral("output-a"));
    QCOMPARE(factory.committedLayout(QStringLiteral("c"))->outerFrame,
             first.geometry.toRect());

    const auto activated = coordinator.execute(
        Hybrid::ActivatePage{QStringLiteral("c"), QStringLiteral("page-b")});
    QVERIFY2(activated.committed(), qPrintable(activated.message));
    QVERIFY(platform.windows.value(QStringLiteral("a")).state.minimized);
    QVERIFY(!platform.windows.value(QStringLiteral("b")).state.minimized);
    QCOMPARE(platform.activeWindowId(), QStringLiteral("b"));
    const auto committed = factory.committedLayout(QStringLiteral("c"));
    QVERIFY(committed.has_value());
    QCOMPARE(committed->activePage.members.keys(), QStringList{QStringLiteral("b")});
}

QTEST_GUILESS_MAIN(KWinHybridSceneTest)
#include "tst_kwinhybridscene.moc"
