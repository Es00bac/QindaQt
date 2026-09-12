// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridshadecontroller.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

class FakeShadeMemberPlatform final : public HybridShadeMemberPlatform
{
public:
    QStringList hiddenMembers;
    QStringList hiddenAnchorContents;
    QStringList failingWindowIds;
    // Fails only hideAnchorContent, so a member can be shaded plainly but
    // refuse a later anchor promotion.
    QStringList failingAnchorWindowIds;
    // Every platform call in order, as "<method>:<windowId>".
    QStringList calls;

    bool hideMember(const QString &windowId, QString *error) override
    {
        calls.append(QStringLiteral("hideMember:%1").arg(windowId));
        if (failingWindowIds.contains(windowId)) {
            if (error) {
                *error = QStringLiteral("sentinel hide failure for %1").arg(windowId);
            }
            return false;
        }
        if (!hiddenMembers.contains(windowId)) {
            hiddenMembers.append(windowId);
        }
        return true;
    }

    bool showMember(const QString &windowId, QString *error) override
    {
        calls.append(QStringLiteral("showMember:%1").arg(windowId));
        if (failingWindowIds.contains(windowId)) {
            if (error) {
                *error = QStringLiteral("sentinel show failure for %1").arg(windowId);
            }
            return false;
        }
        hiddenMembers.removeAll(windowId);
        return true;
    }

    bool hideAnchorContent(const QString &windowId, QString *error) override
    {
        calls.append(QStringLiteral("hideAnchorContent:%1").arg(windowId));
        if (failingWindowIds.contains(windowId)
            || failingAnchorWindowIds.contains(windowId)) {
            if (error) {
                *error = QStringLiteral("sentinel anchor-hide failure for %1").arg(windowId);
            }
            return false;
        }
        if (!hiddenAnchorContents.contains(windowId)) {
            hiddenAnchorContents.append(windowId);
        }
        return true;
    }

    bool showAnchorContent(const QString &windowId, QString *error) override
    {
        Q_UNUSED(error)
        calls.append(QStringLiteral("showAnchorContent:%1").arg(windowId));
        // Undoes both treatments, matching the promotion contract.
        hiddenAnchorContents.removeAll(windowId);
        hiddenMembers.removeAll(windowId);
        return true;
    }
};

QStringList ids(std::initializer_list<const char *> values)
{
    QStringList result;
    for (const auto *value : values) {
        result.append(QString::fromLatin1(value));
    }
    return result;
}

} // namespace

class HybridShadeControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void shadesEveryMemberAndOnlyTheAnchorGetsContentHiding();
    void unshadesExactlyWhatWasHiddenRegardlessOfCurrentTopology();
    void isIdempotentAndRejectsShadingAnAlreadyShadedContainer();
    void rejectsAnAnchorThatIsNotAMemberOrAnEmptyMemberList();
    void rollsBackEveryMemberIfOneFailsDuringShade();
    void reportsFailureButStillForgetsTheContainerIfOneMemberFailsDuringUnshade();
    void forgetContainerDropsShadeStateWithoutTouchingThePlatform();

    void locatesTheShadedContainerOfAMember();
    void reassertReappliesTheOriginalTreatmentToARevealedMember();
    void reassertIgnoresWindowsThatAreNotShadedMembers();
    void reassertReportsPlatformFailureAndKeepsTheContainerShaded();
    void reassertContainerReappliesEveryMemberEvenAfterAFailure();
    void releaseSuppressesReassertUntilCancelledOrRestored();
    void closingAPlainMemberDropsItFromRestore();
    void closingTheAnchorPromotesTheLastSurvivor();
    void anchorPromotionFallsBackToAnEarlierSurvivor();
    void anchorLossWithoutAPromotableSurvivorIsReported();
    void closingTheLastMemberForgetsTheContainer();
    void repeatedRollAndUnrollLeavesNoResidualState();
    void remembersTheFocusedMemberUntilItClosesOrTheGroupUnrolls();
};

void HybridShadeControllerTests::shadesEveryMemberAndOnlyTheAnchorGetsContentHiding()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY2(controller.shadeMembers(
                 QStringLiteral("group"),
                 {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
                 QStringLiteral("b"), &error),
             qPrintable(error));
    QVERIFY(controller.isShaded(QStringLiteral("group")));
    QCOMPARE(platform.hiddenMembers,
             (QStringList{QStringLiteral("a"), QStringLiteral("c")}));
    QCOMPARE(platform.hiddenAnchorContents, QStringList{QStringLiteral("b")});
    // Group stacking reads this to exempt the shaded, non-anchor members
    // from its live-stack contiguity/layer checks (see
    // KWinHybridGroupStacking::synchronize's shadedAnchors parameter).
    QCOMPARE(controller.anchorWindowId(QStringLiteral("group")),
             QStringLiteral("b"));
}

void HybridShadeControllerTests::unshadesExactlyWhatWasHiddenRegardlessOfCurrentTopology()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("a"), &error));
    QVERIFY(controller.unshadeMembers(QStringLiteral("group"), &error));
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
    QVERIFY(platform.hiddenMembers.isEmpty());
    QVERIFY(platform.hiddenAnchorContents.isEmpty());
    QVERIFY(controller.anchorWindowId(QStringLiteral("group")).isEmpty());

    // Unshading a container that was never shaded fails cleanly.
    QVERIFY(!controller.unshadeMembers(QStringLiteral("group"), &error));
    QVERIFY(!error.isEmpty());
}

void HybridShadeControllerTests::isIdempotentAndRejectsShadingAnAlreadyShadedContainer()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("a"), &error));
    QVERIFY(!controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("a"), &error));
    QVERIFY(!error.isEmpty());
    // The original hidden set is untouched by the rejected second call.
    QCOMPARE(platform.hiddenMembers, QStringList{QStringLiteral("b")});
}

void HybridShadeControllerTests::rejectsAnAnchorThatIsNotAMemberOrAnEmptyMemberList()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(!controller.shadeMembers(QStringLiteral("group"), {}, {}, &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("not-a-member"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(platform.hiddenMembers.isEmpty());
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
}

void HybridShadeControllerTests::rollsBackEveryMemberIfOneFailsDuringShade()
{
    FakeShadeMemberPlatform platform;
    platform.failingWindowIds.append(QStringLiteral("c"));
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(!controller.shadeMembers(
        QStringLiteral("group"),
        {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
        QStringLiteral("a"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
    // "a" (anchor) and "b" were hidden before "c" failed; both must be
    // restored so a rejected shade never leaves a half-hidden group.
    QVERIFY(platform.hiddenMembers.isEmpty());
    QVERIFY(platform.hiddenAnchorContents.isEmpty());
    QCOMPARE(platform.calls,
             ids({"hideAnchorContent:a", "hideMember:b", "hideMember:c",
                  "showMember:b", "showAnchorContent:a"}));
    // A rejected shade leaves nothing to enforce.
    QCOMPARE(controller.reassertMember(QStringLiteral("a")),
             ShadeReassertResult::NotEnforced);
}

void HybridShadeControllerTests::reportsFailureButStillForgetsTheContainerIfOneMemberFailsDuringUnshade()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(
        QStringLiteral("group"),
        {QStringLiteral("a"), QStringLiteral("b")}, QStringLiteral("a"), &error));

    // A member closed while shaded: its show call now fails.
    platform.failingWindowIds.append(QStringLiteral("b"));
    QVERIFY(!controller.unshadeMembers(QStringLiteral("group"), &error));
    QVERIFY(!error.isEmpty());
    // The container is no longer tracked as shaded even though one member's
    // restore failed; a stuck container would block every future shade.
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
}

void HybridShadeControllerTests::forgetContainerDropsShadeStateWithoutTouchingThePlatform()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(
        QStringLiteral("group"), {QStringLiteral("a"), QStringLiteral("b")},
        QStringLiteral("a"), &error));
    controller.beginRelease(QStringLiteral("group"));
    controller.forgetContainer(QStringLiteral("group"));
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
    QVERIFY(!controller.isReleasing(QStringLiteral("group")));
    QCOMPARE(controller.shadedContainerIds(), QStringList{});
    QVERIFY(controller.anchorWindowId(QStringLiteral("group")).isEmpty());
    // forgetContainer is a bookkeeping-only drop; a real teardown path is
    // expected to call unshadeMembers itself first if it wants members shown
    // again (see KWinHybridSession::forgetShadedContainer).
    QCOMPARE(platform.hiddenMembers, QStringList{QStringLiteral("b")});
}

void HybridShadeControllerTests::locatesTheShadedContainerOfAMember()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("left"), ids({"a", "b"}),
                                    QStringLiteral("a"), &error));
    QVERIFY(controller.shadeMembers(QStringLiteral("right"), ids({"c", "d"}),
                                    QStringLiteral("d"), &error));
    QCOMPARE(controller.containerOfMember(QStringLiteral("b")), QStringLiteral("left"));
    QCOMPARE(controller.containerOfMember(QStringLiteral("d")), QStringLiteral("right"));
    QVERIFY(controller.containerOfMember(QStringLiteral("unrelated")).isEmpty());
    QVERIFY(controller.unshadeMembers(QStringLiteral("left"), &error));
    QVERIFY(controller.containerOfMember(QStringLiteral("b")).isEmpty());
}

void HybridShadeControllerTests::reassertReappliesTheOriginalTreatmentToARevealedMember()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b", "c"}),
                                    QStringLiteral("b"), &error));
    platform.calls.clear();

    // KWin's activateWindow() cleared Window::isHidden() on the anchor and
    // on a plain member; each must get back exactly its own treatment, never
    // the other one (a plain member must not become force-visible).
    QCOMPARE(controller.reassertMember(QStringLiteral("b"), &error),
             ShadeReassertResult::Reapplied);
    QCOMPARE(controller.reassertMember(QStringLiteral("c"), &error),
             ShadeReassertResult::Reapplied);
    QCOMPARE(platform.calls, ids({"hideAnchorContent:b", "hideMember:c"}));
    QVERIFY(controller.isShaded(QStringLiteral("group")));
    QCOMPARE(controller.anchorWindowId(QStringLiteral("group")), QStringLiteral("b"));
}

void HybridShadeControllerTests::reassertIgnoresWindowsThatAreNotShadedMembers()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QCOMPARE(controller.reassertMember(QStringLiteral("a"), &error),
             ShadeReassertResult::NotEnforced);
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b"}),
                                    QStringLiteral("a"), &error));
    QVERIFY(controller.unshadeMembers(QStringLiteral("group"), &error));
    platform.calls.clear();
    QCOMPARE(controller.reassertMember(QStringLiteral("a"), &error),
             ShadeReassertResult::NotEnforced);
    QCOMPARE(controller.reassertMember(QStringLiteral("outsider"), &error),
             ShadeReassertResult::NotEnforced);
    QVERIFY(platform.calls.isEmpty());
}

void HybridShadeControllerTests::reassertReportsPlatformFailureAndKeepsTheContainerShaded()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b"}),
                                    QStringLiteral("a"), &error));
    platform.failingWindowIds.append(QStringLiteral("b"));
    error.clear();
    QCOMPARE(controller.reassertMember(QStringLiteral("b"), &error),
             ShadeReassertResult::Failed);
    QVERIFY(!error.isEmpty());
    QVERIFY(controller.isShaded(QStringLiteral("group")));
    QCOMPARE(controller.containerOfMember(QStringLiteral("b")), QStringLiteral("group"));
}

void HybridShadeControllerTests::reassertContainerReappliesEveryMemberEvenAfterAFailure()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b", "c"}),
                                    QStringLiteral("c"), &error));
    platform.calls.clear();
    platform.failingWindowIds.append(QStringLiteral("a"));
    error.clear();
    QVERIFY(!controller.reassertContainer(QStringLiteral("group"), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(platform.calls,
             ids({"hideMember:a", "hideMember:b", "hideAnchorContent:c"}));

    platform.failingWindowIds.clear();
    platform.calls.clear();
    QVERIFY2(controller.reassertContainer(QStringLiteral("group"), &error),
             qPrintable(error));
    QCOMPARE(platform.calls.size(), 3);
    QVERIFY(!controller.reassertContainer(QStringLiteral("unknown"), &error));
}

void HybridShadeControllerTests::releaseSuppressesReassertUntilCancelledOrRestored()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b"}),
                                    QStringLiteral("a"), &error));
    platform.calls.clear();

    // Unroll's own reflow activates a member before members are shown.
    controller.beginRelease(QStringLiteral("group"));
    QVERIFY(controller.isShaded(QStringLiteral("group")));
    QVERIFY(controller.isReleasing(QStringLiteral("group")));
    QCOMPARE(controller.reassertMember(QStringLiteral("b"), &error),
             ShadeReassertResult::NotEnforced);
    QVERIFY(!controller.reassertContainer(QStringLiteral("group"), &error));
    QVERIFY(platform.calls.isEmpty());

    // A rejected unroll re-enables enforcement.
    controller.cancelRelease(QStringLiteral("group"));
    QCOMPARE(controller.reassertMember(QStringLiteral("b"), &error),
             ShadeReassertResult::Reapplied);

    // A completed unroll clears the release mark with the record, so a later
    // shade of the same container id is enforced again.
    controller.beginRelease(QStringLiteral("group"));
    QVERIFY(controller.unshadeMembers(QStringLiteral("group"), &error));
    QVERIFY(!controller.isReleasing(QStringLiteral("group")));
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b"}),
                                    QStringLiteral("a"), &error));
    QCOMPARE(controller.reassertMember(QStringLiteral("b"), &error),
             ShadeReassertResult::Reapplied);

    // Releasing an unshaded container is a no-op.
    controller.beginRelease(QStringLiteral("never-shaded"));
    QVERIFY(!controller.isReleasing(QStringLiteral("never-shaded")));
}

void HybridShadeControllerTests::closingAPlainMemberDropsItFromRestore()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b", "c"}),
                                    QStringLiteral("a"), &error));
    platform.calls.clear();
    QVERIFY2(controller.memberClosed(QStringLiteral("b"), &error), qPrintable(error));
    QVERIFY(platform.calls.isEmpty());
    QCOMPARE(controller.anchorWindowId(QStringLiteral("group")), QStringLiteral("a"));
    QVERIFY(controller.containerOfMember(QStringLiteral("b")).isEmpty());
    QCOMPARE(controller.reassertMember(QStringLiteral("b"), &error),
             ShadeReassertResult::NotEnforced);

    QVERIFY(controller.unshadeMembers(QStringLiteral("group"), &error));
    QCOMPARE(platform.calls, ids({"showAnchorContent:a", "showMember:c"}));
}

void HybridShadeControllerTests::closingTheAnchorPromotesTheLastSurvivor()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b", "c"}),
                                    QStringLiteral("c"), &error));
    platform.calls.clear();

    QVERIFY2(controller.memberClosed(QStringLiteral("c"), &error), qPrintable(error));
    QCOMPARE(platform.calls, ids({"hideAnchorContent:b"}));
    QCOMPARE(controller.anchorWindowId(QStringLiteral("group")), QStringLiteral("b"));
    QVERIFY(controller.isShaded(QStringLiteral("group")));

    // Reveals after promotion use the promoted treatment.
    platform.calls.clear();
    QCOMPARE(controller.reassertMember(QStringLiteral("b"), &error),
             ShadeReassertResult::Reapplied);
    QCOMPARE(platform.calls, ids({"hideAnchorContent:b"}));

    // Restore undoes the promoted anchor once and never touches the closed one.
    platform.calls.clear();
    QVERIFY(controller.unshadeMembers(QStringLiteral("group"), &error));
    QCOMPARE(platform.calls, ids({"showMember:a", "showAnchorContent:b"}));
    QVERIFY(platform.hiddenMembers.isEmpty());
}

void HybridShadeControllerTests::anchorPromotionFallsBackToAnEarlierSurvivor()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b", "c"}),
                                    QStringLiteral("c"), &error));
    platform.failingAnchorWindowIds.append(QStringLiteral("b"));
    platform.calls.clear();

    QVERIFY2(controller.memberClosed(QStringLiteral("c"), &error), qPrintable(error));
    QCOMPARE(platform.calls, ids({"hideAnchorContent:b", "hideAnchorContent:a"}));
    QCOMPARE(controller.anchorWindowId(QStringLiteral("group")), QStringLiteral("a"));
}

void HybridShadeControllerTests::anchorLossWithoutAPromotableSurvivorIsReported()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b", "c"}),
                                    QStringLiteral("a"), &error));
    platform.failingAnchorWindowIds = ids({"b", "c"});
    error.clear();

    QVERIFY(!controller.memberClosed(QStringLiteral("a"), &error));
    QVERIFY(!error.isEmpty());
    // Still recorded so the caller's unroll restores both survivors plainly.
    QVERIFY(controller.isShaded(QStringLiteral("group")));
    QVERIFY(controller.anchorWindowId(QStringLiteral("group")).isEmpty());
    platform.calls.clear();
    QVERIFY(controller.unshadeMembers(QStringLiteral("group"), &error));
    QCOMPARE(platform.calls, ids({"showMember:b", "showMember:c"}));
}

void HybridShadeControllerTests::closingTheLastMemberForgetsTheContainer()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b"}),
                                    QStringLiteral("a"), &error));
    QVERIFY(controller.memberClosed(QStringLiteral("b"), &error));
    QVERIFY(controller.isShaded(QStringLiteral("group")));
    platform.calls.clear();
    QVERIFY(controller.memberClosed(QStringLiteral("a"), &error));
    QVERIFY(!controller.isShaded(QStringLiteral("group")));
    QVERIFY(platform.calls.isEmpty());
    // Unrelated windows closing are accepted without side effects.
    QVERIFY(controller.memberClosed(QStringLiteral("unrelated"), &error));
}

void HybridShadeControllerTests::repeatedRollAndUnrollLeavesNoResidualState()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    for (int cycle = 0; cycle < 5; ++cycle) {
        const QString anchor = cycle % 2 == 0 ? QStringLiteral("a") : QStringLiteral("b");
        QVERIFY2(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b"}),
                                         anchor, &error),
                 qPrintable(error));
        QCOMPARE(controller.reassertMember(QStringLiteral("a"), &error),
                 ShadeReassertResult::Reapplied);
        controller.beginRelease(QStringLiteral("group"));
        QVERIFY2(controller.unshadeMembers(QStringLiteral("group"), &error),
                 qPrintable(error));
        QVERIFY(platform.hiddenMembers.isEmpty());
        QVERIFY(platform.hiddenAnchorContents.isEmpty());
        QVERIFY(controller.shadedContainerIds().isEmpty());
        QVERIFY(!controller.isReleasing(QStringLiteral("group")));
    }
}

void HybridShadeControllerTests::remembersTheFocusedMemberUntilItClosesOrTheGroupUnrolls()
{
    FakeShadeMemberPlatform platform;
    HybridShadeController controller(platform);
    QString error;
    QVERIFY(controller.focusedMember(QStringLiteral("group")).isEmpty());
    QVERIFY(controller.shadeMembers(QStringLiteral("group"), ids({"a", "b", "c"}),
                                    QStringLiteral("c"), &error));
    QVERIFY(controller.focusedMember(QStringLiteral("group")).isEmpty());

    controller.recordFocusedMember(QStringLiteral("group"), QStringLiteral("b"));
    QCOMPARE(controller.focusedMember(QStringLiteral("group")), QStringLiteral("b"));
    // Focus outside the group, or an unknown group, is never recorded.
    controller.recordFocusedMember(QStringLiteral("group"), QStringLiteral("outsider"));
    controller.recordFocusedMember(QStringLiteral("unknown"), QStringLiteral("a"));
    QCOMPARE(controller.focusedMember(QStringLiteral("group")), QStringLiteral("b"));
    QVERIFY(controller.focusedMember(QStringLiteral("unknown")).isEmpty());

    // A closed focus owner cannot receive focus back.
    QVERIFY(controller.memberClosed(QStringLiteral("b"), &error));
    QVERIFY(controller.focusedMember(QStringLiteral("group")).isEmpty());

    controller.recordFocusedMember(QStringLiteral("group"), QStringLiteral("a"));
    QCOMPARE(controller.focusedMember(QStringLiteral("group")), QStringLiteral("a"));
    QVERIFY(controller.unshadeMembers(QStringLiteral("group"), &error));
    QVERIFY(controller.focusedMember(QStringLiteral("group")).isEmpty());
    QVERIFY(platform.calls.contains(QStringLiteral("showMember:a")));
}

QTEST_GUILESS_MAIN(HybridShadeControllerTests)
#include "tst_hybridshadecontroller.moc"
