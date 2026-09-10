// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridmemberpolicy_testfixtures.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::Compositor::KWinIntegration::Test;

class HybridMemberPolicyTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsInvalidAndAmbiguousBaselines();
    void detachesOnlyGroupedNativeTitleMoves();
    void vetoesInteractiveResizeOnOwnedMembersOnly();
    void redockedWindowCanDetachAgainAfterSynchronousRefresh();
    void focusedDetachOwnsSynchronousRefreshAndKeepsCallbackValuesAlive();
    void maximizeTogglesFocusAndRestoresExactBaseline();
    void fullscreenEntersAndExitsWithoutChangingLayout();
    void rejectedCompetingPresentationRollsBackWithoutChangingFocus();
    void focusedMinimizeAndCloseRestoreCoherently();
    void pageSwitchRestoresFocusBeforeTopologySynchronization();
    void crossContainerMoveRestoresFocusBeforeTopologySynchronization();
    void unrelatedLifecycleMutationRestoresFocusBeforeSceneReplan();
    void failedPlatformCallsDoNotPublishPolicyState();
    void shutdownRestoresFocusExactlyAndIsIdempotent();
    void completedPresentationRestoreCannotReplayGroupedFrame();
};

void HybridMemberPolicyTest::rejectsInvalidAndAmbiguousBaselines()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    QString error;

    auto invalid = group();
    invalid.members[0].frame = {};
    QVERIFY(!policy.synchronize({invalid}, &error));
    QVERIFY(error.contains(QStringLiteral("valid frame")));

    auto duplicate = group();
    auto second = group();
    second.containerId = QStringLiteral("second");
    QVERIFY(!policy.synchronize({duplicate, second}, &error));
    QVERIFY(error.contains(QStringLiteral("multiple groups")));

    auto noActivePage = group();
    for (auto &member : noActivePage.members) {
        member.activePage = false;
    }
    QVERIFY(!policy.synchronize({noActivePage}, &error));
    QVERIFY(error.contains(QStringLiteral("active-page")));
}

void HybridMemberPolicyTest::detachesOnlyGroupedNativeTitleMoves()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    QVERIFY(policy.synchronize({group()}));

    QVERIFY(!policy.interactiveMoveStarted(QStringLiteral("independent"), true));
    QCOMPARE(platform.calls.size(), 0);

    QVERIFY(policy.interactiveMoveStarted(QStringLiteral("left"), true));
    QCOMPARE(platform.calls.size(), 1);
    QCOMPARE(platform.calls[0].kind, CallKind::Detach);
    QCOMPARE(platform.calls[0].baseline.containerId, QStringLiteral("group"));
    QCOMPARE(platform.calls[0].windowId, QStringLiteral("left"));
    QVERIFY(!policy.interactiveMoveStarted(QStringLiteral("left"), true));
    QCOMPARE(platform.calls.size(), 1);
}

void HybridMemberPolicyTest::vetoesInteractiveResizeOnOwnedMembersOnly()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    QVERIFY(policy.synchronize({group()}));

    // The adapter cancels native resize when this predicate holds, so an
    // owned member's frame can only change through container reflow. Active-
    // and inactive-page members are owned alike; non-members stay untouched.
    QVERIFY(policy.blocksInteractiveResize(QStringLiteral("left")));
    QVERIFY(policy.blocksInteractiveResize(QStringLiteral("other-page")));
    QVERIFY(!policy.blocksInteractiveResize(QStringLiteral("independent")));
    QVERIFY(!policy.blocksInteractiveResize(QStringLiteral("")));
    QCOMPARE(platform.calls.size(), 0);

    // A mid-detach member is already owned by the native detach transaction
    // and must not be vetoed: KWin continues that move to the drop.
    QVERIFY(policy.interactiveMoveStarted(QStringLiteral("left"), true));
    QVERIFY(!policy.blocksInteractiveResize(QStringLiteral("left")));

    // Once the committed refresh drops it, it is simply not a member anymore.
    QVERIFY(policy.synchronize({}));
    QVERIFY(!policy.blocksInteractiveResize(QStringLiteral("left")));
    QCOMPARE(platform.calls.size(), 1);
}

void HybridMemberPolicyTest::redockedWindowCanDetachAgainAfterSynchronousRefresh()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    QVERIFY(policy.synchronize({group()}));

    // The production adapter commits detach and calls synchronizeChrome()
    // before returning to interactiveMoveStarted(). Model that re-entrant
    // topology refresh so the regression catches post-callback marker writes.
    platform.onDetach = [&policy] {
        QVERIFY(policy.synchronize({}));
    };
    QVERIFY(policy.interactiveMoveStarted(QStringLiteral("left"), true));

    QVERIFY(policy.synchronize({group()}));
    QVERIFY(policy.interactiveMoveStarted(QStringLiteral("left"), true));
    QCOMPARE(platform.calls.size(), 2);
    QCOMPARE(platform.calls[0].windowId, QStringLiteral("left"));
    QCOMPARE(platform.calls[1].windowId, QStringLiteral("left"));
}

void HybridMemberPolicyTest::focusedDetachOwnsSynchronousRefreshAndKeepsCallbackValuesAlive()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto original = group();
    QVERIFY(policy.synchronize({original}));
    QVERIFY(policy.fullscreenChanged(QStringLiteral("left"), true));

    bool transitionOwnedDuringRefresh = false;
    platform.onDetach = [&] {
        QVERIFY(policy.synchronize({}));
        transitionOwnedDuringRefresh = policy.ownsTransition();
        // The outer detach owns cleanup; synchronize must not nest a restore
        // that clears its state or drops the applying guard prematurely.
        QCOMPARE(platform.calls.size(), 2);
        QCOMPARE(platform.calls.constLast().kind, CallKind::Detach);
    };
    QVERIFY(policy.interactiveMoveStarted(QStringLiteral("left"), true));

    QVERIFY(transitionOwnedDuringRefresh);
    QVERIFY(!policy.ownsTransition());
    QVERIFY(!policy.focusState(QStringLiteral("group")));
    QCOMPARE(platform.postDetachContainerId, QStringLiteral("group"));
    QCOMPARE(platform.postDetachFocusBaseline, std::optional(original));
}

void HybridMemberPolicyTest::maximizeTogglesFocusAndRestoresExactBaseline()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto original = group();
    QVERIFY(policy.synchronize({original}));

    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    QCOMPARE(policy.focusState(QStringLiteral("group")),
             std::optional<MemberFocusState>({QStringLiteral("group"),
                                              QStringLiteral("left"),
                                              MemberFocusMode::Maximized}));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Enter);
    QCOMPARE(platform.calls.constLast().baseline, original);

    // Temporary KWin geometry/visibility notifications may trigger a session
    // resync. Restore must still use the pre-interaction committed copy.
    auto focusedPresentation = original;
    focusedPresentation.members[0].frame = original.outerFrame;
    focusedPresentation.members[1].hidden = true;
    QVERIFY(policy.synchronize({focusedPresentation}));
    QVERIFY(!policy.maximizedChanged(QStringLiteral("left"), false));
    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    QVERIFY(!policy.focusState(QStringLiteral("group")));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().baseline, original);
    QVERIFY(platform.calls.constLast().windowId.isEmpty());
}

void HybridMemberPolicyTest::fullscreenEntersAndExitsWithoutChangingLayout()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto original = group();
    QVERIFY(policy.synchronize({original}));

    QVERIFY(policy.fullscreenChanged(QStringLiteral("right"), true));
    QCOMPARE(policy.focusState(QStringLiteral("group"))->mode, MemberFocusMode::Fullscreen);
    QCOMPARE(platform.calls.constLast().mode,
             std::optional<MemberFocusMode>(MemberFocusMode::Fullscreen));
    QVERIFY(policy.fullscreenChanged(QStringLiteral("right"), false));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().baseline, original);
    QCOMPARE(platform.calls.constLast().activation,
             MemberRestoreActivation::PreserveCurrent);
    QVERIFY(!policy.focusState(QStringLiteral("group")));
}

void HybridMemberPolicyTest::rejectedCompetingPresentationRollsBackWithoutChangingFocus()
{
    const auto original = group();

    for (const auto mode : {MemberFocusMode::Maximized,
                            MemberFocusMode::Fullscreen}) {
        FakePlatform platform;
        HybridMemberPolicy policy(platform);
        QVERIFY(policy.synchronize({original}));

        bool rollbackSignalsSuppressed = false;
        platform.onReject = [&] {
            rollbackSignalsSuppressed = policy.ownsTransition()
                && !policy.maximizedChanged(QStringLiteral("right"), true)
                && !policy.fullscreenChanged(QStringLiteral("right"), true);
        };

        if (mode == MemberFocusMode::Maximized) {
            QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
            QVERIFY(!policy.maximizedChanged(QStringLiteral("right"), true));
        } else {
            QVERIFY(policy.fullscreenChanged(QStringLiteral("left"), true));
            QVERIFY(!policy.fullscreenChanged(QStringLiteral("right"), true));
        }

        QCOMPARE(platform.calls.size(), 2);
        QCOMPARE(platform.calls.constLast().kind, CallKind::Reject);
        QCOMPARE(platform.calls.constLast().baseline, original);
        QCOMPARE(platform.calls.constLast().windowId, QStringLiteral("right"));
        QCOMPARE(platform.calls.constLast().focusOwnerWindowId, QStringLiteral("left"));
        QCOMPARE(platform.calls.constLast().mode, std::optional(mode));
        QVERIFY(rollbackSignalsSuppressed);
        QCOMPARE(policy.focusState(QStringLiteral("group")),
                 std::optional<MemberFocusState>({QStringLiteral("group"),
                                                  QStringLiteral("left"), mode}));
    }
}

void HybridMemberPolicyTest::focusedMinimizeAndCloseRestoreCoherently()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto original = group();
    QVERIFY(policy.synchronize({original}));

    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    QVERIFY(policy.minimizedChanged(QStringLiteral("left"), true));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().windowId, QStringLiteral("left"));
    QCOMPARE(platform.calls.constLast().baseline, original);

    QVERIFY(policy.maximizedChanged(QStringLiteral("right"), true));
    QVERIFY(policy.memberClosed(QStringLiteral("right")));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().missing, QSet<QString>{QStringLiteral("right")});
    QCOMPARE(platform.calls.constLast().baseline, original);
    QCOMPARE(platform.calls.constLast().activation,
             MemberRestoreActivation::PreserveCurrent);
    QVERIFY(!policy.focusState(QStringLiteral("group")));

    QVERIFY(!policy.memberClosed(QStringLiteral("independent")));
}

void HybridMemberPolicyTest::pageSwitchRestoresFocusBeforeTopologySynchronization()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto original = group();
    QVERIFY(policy.synchronize({original}));
    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));

    // Semantic shortcuts, accessible tabs, task activation, and pointer tabs
    // all use this gate before committing ActivatePage. The restore must see
    // the old active-page baseline, never the already-switched scene.
    QVERIFY(policy.restoreForTopologyMutation());
    auto switched = original;
    switched.members[0].activePage = false;
    switched.members[1].activePage = false;
    switched.members[2].activePage = true;
    QVERIFY(policy.synchronize({switched}));

    QVERIFY(!policy.focusState(QStringLiteral("group")));
    QCOMPARE(platform.calls[1].kind, CallKind::Restore);
    QCOMPARE(platform.calls[1].baseline, original);
    QCOMPARE(platform.calls[1].activation,
             MemberRestoreActivation::RestoreBaseline);
}

void HybridMemberPolicyTest::crossContainerMoveRestoresFocusBeforeTopologySynchronization()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto original = group();
    QVERIFY(policy.synchronize({original}));
    QVERIFY(policy.fullscreenChanged(QStringLiteral("left"), true));
    QVERIFY(policy.restoreForTopologyMutation());

    auto source = original;
    source.members.removeFirst();
    source.members[0].activePage = true;
    source.members[1].activePage = true;
    source.members[1].minimized = false;
    MemberGroupBaseline destination{
        .containerId = QStringLiteral("destination"),
        .outerFrame = QRectF(1050.0, 20.0, 900.0, 700.0),
        .members = {
            {.windowId = QStringLiteral("left"),
             .frame = QRectF(1051.0, 89.0, 448.0, 630.0),
             .activePage = true},
            {.windowId = QStringLiteral("destination-peer"),
             .frame = QRectF(1501.0, 89.0, 448.0, 630.0),
             .activePage = true},
        },
    };
    QVERIFY(policy.synchronize({source, destination}));

    QVERIFY(!policy.focusState(QStringLiteral("group")));
    QCOMPARE(platform.calls[1].kind, CallKind::Restore);
    QCOMPARE(platform.calls[1].baseline, original);
    QCOMPARE(platform.calls[1].activation,
             MemberRestoreActivation::RestoreBaseline);
}

void HybridMemberPolicyTest::unrelatedLifecycleMutationRestoresFocusBeforeSceneReplan()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto original = group();
    QVERIFY(policy.synchronize({original}));
    QVERIFY(policy.maximizedChanged(QStringLiteral("right"), true));

    // Add/Forget currently re-plan every committed group even when the changed
    // independent window is unrelated. Production therefore calls this gate
    // before either lifecycle command, while the focus baseline is still valid.
    QVERIFY(policy.restoreForLifecycleMutation());
    QVERIFY(policy.synchronize({original}));

    QVERIFY(!policy.focusState(QStringLiteral("group")));
    QCOMPARE(platform.calls[1].kind, CallKind::Restore);
    QCOMPARE(platform.calls[1].baseline, original);
    QCOMPARE(platform.calls[1].activation,
             MemberRestoreActivation::PreserveCurrent);
}

void HybridMemberPolicyTest::failedPlatformCallsDoNotPublishPolicyState()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    QVERIFY(policy.synchronize({group()}));
    QString error;

    platform.failNext = true;
    QVERIFY(!policy.maximizedChanged(QStringLiteral("left"), true, &error));
    QCOMPARE(error, QStringLiteral("injected platform failure"));
    QVERIFY(!policy.focusState(QStringLiteral("group")));

    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    platform.failNext = true;
    QVERIFY(!policy.maximizedChanged(QStringLiteral("left"), true, &error));
    QVERIFY(policy.focusState(QStringLiteral("group")));

    platform.failNext = true;
    QVERIFY(!policy.maximizedChanged(QStringLiteral("right"), true, &error));
    QCOMPARE(error, QStringLiteral("injected platform failure"));
    QCOMPARE(policy.focusState(QStringLiteral("group"))->windowId, QStringLiteral("left"));

    platform.failNext = true;
    QVERIFY(!policy.interactiveMoveStarted(QStringLiteral("right"), true, &error));
    QVERIFY(policy.focusState(QStringLiteral("group")));
}

void HybridMemberPolicyTest::shutdownRestoresFocusExactlyAndIsIdempotent()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto original = group();
    QVERIFY(policy.synchronize({original}));
    QVERIFY(policy.fullscreenChanged(QStringLiteral("left"), true));

    QString error;
    platform.failNext = true;
    QVERIFY(!policy.restoreForShutdown({}, &error));
    QCOMPARE(error, QStringLiteral("injected platform failure"));
    QVERIFY(policy.focusState(QStringLiteral("group")));

    QVERIFY(policy.restoreForShutdown());
    QVERIFY(!policy.focusState(QStringLiteral("group")));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().baseline, original);
    QCOMPARE(platform.calls.constLast().activation,
             MemberRestoreActivation::PreserveCurrent);
    const auto callCount = platform.calls.size();
    QVERIFY(policy.restoreForShutdown());
    QCOMPARE(platform.calls.size(), callCount);
}

void HybridMemberPolicyTest::completedPresentationRestoreCannotReplayGroupedFrame()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    QVERIFY(policy.synchronize({group()}));
    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    QVERIFY(policy.restoreForShutdown());
    const auto callCount = platform.calls.size();

    // The production session restores independent state between lifecycle
    // phases. A second-phase disconnect must therefore have no policy call
    // capable of replaying the obsolete grouped baseline.
    const QRectF independentlyRestored(1200.0, 700.0, 640.0, 480.0);
    QVERIFY(policy.restoreForShutdown());
    QCOMPARE(platform.calls.size(), callCount);
    QVERIFY(platform.calls.constLast().baseline.members.constFirst().frame
            != independentlyRestored);
}

QTEST_GUILESS_MAIN(HybridMemberPolicyTest)

#include "tst_hybridmemberpolicy.moc"
