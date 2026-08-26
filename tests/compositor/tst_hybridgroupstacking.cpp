// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridgroupstackingpolicy.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class HybridGroupStackingTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compactsAtTopmostMemberWithoutRaisingPastUnrelatedWindows();
    void memberActivationRaisesTheWholeBlockAndSelectsItsAnchor();
    void placesTransientSubtreesAboveTheCompleteMemberBlock();
    void preservesAnUnrelatedWindowAboveTheTransientBlock();
    void plansMultipleGroupsWithoutChangingOutsideOrder();
    void rejectsMissingDuplicateAndMultiplyOwnedMembers();
};

void HybridGroupStackingTests::compactsAtTopmostMemberWithoutRaisingPastUnrelatedWindows()
{
    const auto plan = planHybridGroupStacking(
        {QStringLiteral("member-a"), QStringLiteral("outside-low"),
         QStringLiteral("member-b"), QStringLiteral("outside-high")},
        {{QStringLiteral("group"),
          {.activeMembers = {QStringLiteral("member-b"),
                             QStringLiteral("member-a")},
           .associatedTransients = {},
           .transientOwnerById = {}}}});
    QVERIFY(plan);
    QCOMPARE(plan->windowsBottomToTop,
             QStringList({QStringLiteral("outside-low"),
                          QStringLiteral("member-a"),
                          QStringLiteral("member-b"),
                          QStringLiteral("outside-high")}));
    QCOMPARE(plan->blocksBottomToTop.size(), 1);
    QCOMPARE(plan->blocksBottomToTop.constFirst().membersBottomToTop,
             QStringList({QStringLiteral("member-a"),
                          QStringLiteral("member-b")}));
    QCOMPARE(plan->blocksBottomToTop.constFirst().topMemberId(),
             QStringLiteral("member-b"));
}

void HybridGroupStackingTests::memberActivationRaisesTheWholeBlockAndSelectsItsAnchor()
{
    // KWin has just raised member-a after a native click. Its position is now
    // the group's topmost rank, so compaction raises member-b with it while
    // retaining member-a as the scene chrome anchor.
    const auto plan = planHybridGroupStacking(
        {QStringLiteral("outside"), QStringLiteral("member-b"),
         QStringLiteral("member-a")},
        {{QStringLiteral("group"),
          {.activeMembers = {QStringLiteral("member-a"),
                             QStringLiteral("member-b")},
           .associatedTransients = {},
           .transientOwnerById = {}}}});
    QVERIFY(plan);
    QCOMPARE(plan->windowsBottomToTop,
             QStringList({QStringLiteral("outside"),
                          QStringLiteral("member-b"),
                          QStringLiteral("member-a")}));
    QCOMPARE(plan->blocksBottomToTop.constFirst().topMemberId(),
             QStringLiteral("member-a"));

    const auto covered = planHybridGroupStacking(
        {QStringLiteral("member-b"), QStringLiteral("member-a"),
         QStringLiteral("outside")},
        {{QStringLiteral("group"),
          {.activeMembers = {QStringLiteral("member-a"),
                             QStringLiteral("member-b")},
           .associatedTransients = {},
           .transientOwnerById = {}}}});
    QVERIFY(covered);
    QCOMPARE(covered->windowsBottomToTop.constLast(), QStringLiteral("outside"));
}

void HybridGroupStackingTests::placesTransientSubtreesAboveTheCompleteMemberBlock()
{
    const auto plan = planHybridGroupStacking(
        {QStringLiteral("member-a"), QStringLiteral("dialog-child"),
         QStringLiteral("outside"), QStringLiteral("member-b"),
         QStringLiteral("dialog-parent")},
        {{QStringLiteral("group"),
          {.activeMembers = {QStringLiteral("member-a"),
                             QStringLiteral("member-b")},
           .associatedTransients = {QStringLiteral("dialog-parent"),
                                    QStringLiteral("dialog-child")},
           .transientOwnerById = {
               {QStringLiteral("dialog-parent"), QStringLiteral("member-a")},
               {QStringLiteral("dialog-child"), QStringLiteral("member-a")},
           }}}});
    QVERIFY(plan);
    QCOMPARE(plan->windowsBottomToTop,
             QStringList({QStringLiteral("outside"),
                          QStringLiteral("member-b"),
                          QStringLiteral("member-a"),
                          QStringLiteral("dialog-child"),
                          QStringLiteral("dialog-parent")}));
    const auto &block = plan->blocksBottomToTop.constFirst();
    QCOMPARE(block.topMemberId(), QStringLiteral("member-a"));
    QCOMPARE(block.transientsBottomToTop,
             QStringList({QStringLiteral("dialog-child"),
                          QStringLiteral("dialog-parent")}));
}

void HybridGroupStackingTests::preservesAnUnrelatedWindowAboveTheTransientBlock()
{
    const auto plan = planHybridGroupStacking(
        {QStringLiteral("member-a"), QStringLiteral("member-b"),
         QStringLiteral("dialog"), QStringLiteral("outside-active")},
        {{QStringLiteral("group"),
          {.activeMembers = {QStringLiteral("member-a"),
                             QStringLiteral("member-b")},
           .associatedTransients = {QStringLiteral("dialog")},
           .transientOwnerById = {
               {QStringLiteral("dialog"), QStringLiteral("member-b")},
           }}}});
    QVERIFY(plan);
    QCOMPARE(plan->windowsBottomToTop,
             QStringList({QStringLiteral("member-a"),
                          QStringLiteral("member-b"),
                          QStringLiteral("dialog"),
                          QStringLiteral("outside-active")}));
}

void HybridGroupStackingTests::plansMultipleGroupsWithoutChangingOutsideOrder()
{
    const auto plan = planHybridGroupStacking(
        {QStringLiteral("a1"), QStringLiteral("outside-1"),
         QStringLiteral("b1"), QStringLiteral("a2"),
         QStringLiteral("outside-2"), QStringLiteral("b2")},
        {{QStringLiteral("alpha"),
          {.activeMembers = {QStringLiteral("a1"), QStringLiteral("a2")},
           .associatedTransients = {}, .transientOwnerById = {}}},
         {QStringLiteral("beta"),
          {.activeMembers = {QStringLiteral("b1"), QStringLiteral("b2")},
           .associatedTransients = {}, .transientOwnerById = {}}}});
    QVERIFY(plan);
    QCOMPARE(plan->windowsBottomToTop,
             QStringList({QStringLiteral("outside-1"),
                          QStringLiteral("a1"), QStringLiteral("a2"),
                          QStringLiteral("outside-2"),
                          QStringLiteral("b1"), QStringLiteral("b2")}));
    QCOMPARE(plan->blocksBottomToTop.size(), 2);
    QCOMPARE(plan->blocksBottomToTop[0].containerId, QStringLiteral("alpha"));
    QCOMPARE(plan->blocksBottomToTop[1].containerId, QStringLiteral("beta"));
}

void HybridGroupStackingTests::rejectsMissingDuplicateAndMultiplyOwnedMembers()
{
    QString error;
    QVERIFY(!planHybridGroupStacking(
        {QStringLiteral("a"), QStringLiteral("a")}, {}, &error));
    QVERIFY(error.contains(QStringLiteral("unique")));

    QVERIFY(!planHybridGroupStacking(
        {QStringLiteral("a")},
        {{QStringLiteral("group"),
          {.activeMembers = {QStringLiteral("missing")},
           .associatedTransients = {}, .transientOwnerById = {}}}}, &error));
    QVERIFY(error.contains(QStringLiteral("invalid")));

    QVERIFY(!planHybridGroupStacking(
        {QStringLiteral("a")},
        {{QStringLiteral("alpha"),
          {.activeMembers = {QStringLiteral("a")}, .associatedTransients = {},
           .transientOwnerById = {}}},
         {QStringLiteral("beta"),
          {.activeMembers = {QStringLiteral("a")}, .associatedTransients = {},
           .transientOwnerById = {}}}},
        &error));
    QVERIFY(error.contains(QStringLiteral("multiply-owned")));

    QVERIFY(!planHybridGroupStacking(
        {QStringLiteral("member"), QStringLiteral("dialog")},
        {{QStringLiteral("group"),
          {.activeMembers = {QStringLiteral("member")},
           .associatedTransients = {QStringLiteral("dialog")},
           .transientOwnerById = {}}}},
        &error));
    QVERIFY(error.contains(QStringLiteral("name their grouped owners")));

    QVERIFY(!planHybridGroupStacking(
        {QStringLiteral("member"), QStringLiteral("dialog")},
        {{QStringLiteral("group"),
          {.activeMembers = {QStringLiteral("member")},
           .associatedTransients = {QStringLiteral("dialog")},
           .transientOwnerById = {
               {QStringLiteral("dialog"), QStringLiteral("outside")},
           }}}},
        &error));
    QVERIFY(error.contains(QStringLiteral("invalid grouped owner")));
}

QTEST_GUILESS_MAIN(HybridGroupStackingTests)
#include "tst_hybridgroupstacking.moc"
