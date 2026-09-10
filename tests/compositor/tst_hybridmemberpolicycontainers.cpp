// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridmemberpolicy_testfixtures.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::Compositor::KWinIntegration::Test;

// Focus presentation with more than one live container. The single-container
// lifecycle lives in tst_hybridmemberpolicy.cpp.
class HybridMemberPolicyContainersTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void containersOwnIndependentFocusPresentation();
    void topologySynchronizationRestoresOnlyInvalidatedContainers();
    void nativeDetachRestoresOtherContainersBeforeTransaction();
};

void HybridMemberPolicyContainersTest::containersOwnIndependentFocusPresentation()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto first = group();
    const auto second = secondGroup();
    QVERIFY(policy.synchronize({first, second}));

    // Each container may present one member alone. The second request is
    // accepted on its own container instead of being rejected against the
    // first container's owner.
    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    QVERIFY(policy.fullscreenChanged(QStringLiteral("second-right"), true));
    QCOMPARE(platform.calls.size(), 2);
    QCOMPARE(platform.calls[0].kind, CallKind::Enter);
    QCOMPARE(platform.calls[1].kind, CallKind::Enter);
    QCOMPARE(platform.calls[1].baseline, second);
    QCOMPARE(policy.focusState(QStringLiteral("group")),
             std::optional<MemberFocusState>({QStringLiteral("group"),
                                              QStringLiteral("left"),
                                              MemberFocusMode::Maximized}));
    QCOMPARE(policy.focusState(QStringLiteral("second")),
             std::optional<MemberFocusState>({QStringLiteral("second"),
                                              QStringLiteral("second-right"),
                                              MemberFocusMode::Fullscreen}));
    QCOMPARE(policy.focusStates().size(), qsizetype{2});
    QCOMPARE(policy.focusBaseline(QStringLiteral("second")), std::optional(second));

    // A competing request is judged only against its own container.
    QVERIFY(!policy.maximizedChanged(QStringLiteral("second-left"), true));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Reject);
    QCOMPARE(platform.calls.constLast().baseline, second);
    QCOMPARE(platform.calls.constLast().windowId, QStringLiteral("second-left"));
    QCOMPARE(platform.calls.constLast().focusOwnerWindowId,
             QStringLiteral("second-right"));
    QCOMPARE(policy.focusState(QStringLiteral("group"))->windowId, QStringLiteral("left"));

    // A placement-only action on one container (shared-chrome maximize,
    // minimize, restore, shade) leaves the other container untouched.
    QVERIFY(policy.restoreForContainerAction(QStringLiteral("second")));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().baseline, second);
    QCOMPARE(platform.calls.constLast().activation,
             MemberRestoreActivation::RestoreBaseline);
    QVERIFY(!policy.focusState(QStringLiteral("second")));
    QCOMPARE(policy.focusState(QStringLiteral("group"))->windowId, QStringLiteral("left"));
    const auto callCount = platform.calls.size();
    QVERIFY(policy.restoreForContainerAction(QStringLiteral("second")));
    QVERIFY(policy.restoreForContainerAction(QStringLiteral("missing")));
    QCOMPARE(platform.calls.size(), callCount);

    // Native fullscreen exit and minimize address the container whose owner
    // they name, never the other one.
    QVERIFY(policy.fullscreenChanged(QStringLiteral("second-left"), true));
    QVERIFY(policy.minimizedChanged(QStringLiteral("left"), true));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().baseline, first);
    QCOMPARE(platform.calls.constLast().windowId, QStringLiteral("left"));
    QVERIFY(!policy.focusState(QStringLiteral("group")));
    QCOMPARE(policy.focusState(QStringLiteral("second"))->windowId,
             QStringLiteral("second-left"));
    QVERIFY(policy.fullscreenChanged(QStringLiteral("second-left"), false));
    QCOMPARE(platform.calls.constLast().baseline, second);
    QCOMPARE(platform.calls.constLast().activation,
             MemberRestoreActivation::PreserveCurrent);
    QVERIFY(policy.focusStates().isEmpty());

    // Closing a hidden peer restores only the container that contains it.
    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    QVERIFY(policy.maximizedChanged(QStringLiteral("second-left"), true));
    QVERIFY(policy.memberClosed(QStringLiteral("second-right")));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().baseline, second);
    QCOMPARE(platform.calls.constLast().missing,
             QSet<QString>{QStringLiteral("second-right")});
    QCOMPARE(policy.focusState(QStringLiteral("group"))->windowId, QStringLiteral("left"));
    QVERIFY(!policy.focusState(QStringLiteral("second")));
}

void HybridMemberPolicyContainersTest::topologySynchronizationRestoresOnlyInvalidatedContainers()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto first = group();
    const auto second = secondGroup();
    QVERIFY(policy.synchronize({first, second}));
    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    QVERIFY(policy.maximizedChanged(QStringLiteral("second-left"), true));

    // Switching pages in the second container invalidates only its own
    // presentation; the first container keeps presenting.
    auto switched = second;
    switched.members[0].activePage = false;
    switched.members[1].activePage = false;
    switched.members[2].activePage = true;
    QVERIFY(policy.synchronize({first, switched}));
    QCOMPARE(platform.calls.constLast().kind, CallKind::Restore);
    QCOMPARE(platform.calls.constLast().baseline, second);
    QVERIFY(!policy.focusState(QStringLiteral("second")));
    QCOMPARE(policy.focusState(QStringLiteral("group"))->windowId, QStringLiteral("left"));

    // The whole-session gate still clears every container before a scene
    // transaction, in stable container order.
    QVERIFY(policy.maximizedChanged(QStringLiteral("second-other-page"), true));
    QCOMPARE(policy.focusStates().size(), qsizetype{2});
    const auto before = platform.calls.size();
    QVERIFY(policy.restoreForTopologyMutation());
    QCOMPARE(platform.calls.size(), before + 2);
    QCOMPARE(platform.calls[before].kind, CallKind::Restore);
    QCOMPARE(platform.calls[before].baseline, first);
    QCOMPARE(platform.calls[before + 1].kind, CallKind::Restore);
    QCOMPARE(platform.calls[before + 1].baseline, switched);
    QVERIFY(policy.focusStates().isEmpty());

    // A failure part-way keeps the remaining container's presentation.
    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));
    QVERIFY(policy.maximizedChanged(QStringLiteral("second-other-page"), true));
    QString error;
    platform.failNext = true;
    QVERIFY(!policy.restoreForLifecycleMutation(&error));
    QCOMPARE(error, QStringLiteral("injected platform failure"));
    QCOMPARE(policy.focusStates().size(), qsizetype{2});
    QVERIFY(policy.restoreForShutdown());
    QVERIFY(policy.focusStates().isEmpty());
}

void HybridMemberPolicyContainersTest::nativeDetachRestoresOtherContainersBeforeTransaction()
{
    FakePlatform platform;
    HybridMemberPolicy policy(platform);
    const auto first = group();
    const auto second = secondGroup();
    QVERIFY(policy.synchronize({first, second}));
    QVERIFY(policy.maximizedChanged(QStringLiteral("left"), true));

    // Dragging a member out of the other container by its native title runs
    // a scene transaction that re-plans every group, so the first container's
    // presentation leaves first without stealing the dragged window's
    // activation; the dragged container itself had none to hand over.
    QVERIFY(policy.interactiveMoveStarted(QStringLiteral("second-right"), true));
    QCOMPARE(platform.calls.size(), 3);
    QCOMPARE(platform.calls[1].kind, CallKind::Restore);
    QCOMPARE(platform.calls[1].baseline, first);
    QCOMPARE(platform.calls[1].activation, MemberRestoreActivation::PreserveCurrent);
    QCOMPARE(platform.calls[2].kind, CallKind::Detach);
    QCOMPARE(platform.calls[2].baseline.containerId, QStringLiteral("second"));
    QVERIFY(platform.calls[2].baseline.members.isEmpty());
    QVERIFY(policy.focusStates().isEmpty());
}

QTEST_GUILESS_MAIN(HybridMemberPolicyContainersTest)

#include "tst_hybridmemberpolicycontainers.moc"
