// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridinteractionruntime_testfixtures.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::Compositor::KWinIntegration::Test;
namespace Core = QindaQt::Core;
namespace Hybrid = QindaQt::Hybrid;
namespace HybridInput = QindaQt::HybridInput;

// Container-edge drops: a dock target that names a container but no member
// wraps the target's active page root instead of splitting one member tile.
class HybridInteractionContainerEdgesTest final : public QObject
{
    Q_OBJECT

private slots:
    void docksContainerEdgesIntoActivePageRoot();
};

void HybridInteractionContainerEdgesTest::docksContainerEdgesIntoActivePageRoot()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c"),
                                      QStringLiteral("window-d"),
                                      QStringLiteral("window-e")},
                                     scene);
    QVERIFY(dock(runtime,
                 QStringLiteral("window-a"),
                 QStringLiteral("window-b"),
                 HybridInput::DockZone::Right)
                .topologyChanged());

    // A target with a container but no member anchor is a container-edge
    // drop. The independent window takes the whole left side of the page
    // instead of splitting the member tile nearest the pointer.
    const auto insert = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"),
        {},
        HybridInput::HitKind::MemberTitle,
        {},
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Left));
    QVERIFY2(insert.topologyChanged(), qPrintable(insert.message));
    QCOMPARE(scene.kinds.constLast(),
             Hybrid::TopologyCommandKind::InsertIndependentWindow);
    const auto *container = runtime.topology().container(
        QStringLiteral("hybrid-r1-container"));
    QVERIFY(container);
    const auto *root = &container->page(container->activePageId())->root();
    QCOMPARE(root->id(), QStringLiteral("hybrid-r2-split"));
    QCOMPARE(root->orientation(), std::optional(Core::SplitOrientation::Horizontal));
    QCOMPARE(root->firstChild()->windowId(), QStringLiteral("window-c"));
    QCOMPARE(root->firstChild()->id(), QStringLiteral("hybrid-r2-source-leaf"));
    QCOMPARE(root->secondChild()->id(), QStringLiteral("hybrid-r1-split"));

    // A grouped member from another container joins the bottom of the whole
    // page; its two-member source unwraps as usual.
    QVERIFY(dock(runtime,
                 QStringLiteral("window-d"),
                 QStringLiteral("window-e"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    const auto move = runtime.handleIntent(memberCommit(
        QStringLiteral("window-e"),
        QStringLiteral("hybrid-r3-container"),
        HybridInput::HitKind::MemberTitle,
        {},
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Bottom));
    QVERIFY2(move.topologyChanged(), qPrintable(move.message));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::MoveMember);
    container = runtime.topology().container(QStringLiteral("hybrid-r1-container"));
    root = &container->page(container->activePageId())->root();
    QCOMPARE(root->id(), QStringLiteral("hybrid-r4-split"));
    QCOMPARE(root->orientation(), std::optional(Core::SplitOrientation::Vertical));
    QCOMPARE(root->firstChild()->id(), QStringLiteral("hybrid-r2-split"));
    QCOMPARE(root->secondChild()->windowId(), QStringLiteral("window-e"));
    QVERIFY(!runtime.topology().container(QStringLiteral("hybrid-r3-container")));
    QVERIFY(runtime.topology().isIndependent(QStringLiteral("window-d")));

    // Within its own container a member dragged to the top band wraps the
    // remaining tree, keeping its leaf ID.
    const auto reparent = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle,
        {},
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Top));
    QVERIFY2(reparent.topologyChanged(), qPrintable(reparent.message));
    QCOMPARE(scene.kinds.constLast(),
             Hybrid::TopologyCommandKind::ReparentMemberToPageRoot);
    container = runtime.topology().container(QStringLiteral("hybrid-r1-container"));
    root = &container->page(container->activePageId())->root();
    QCOMPARE(root->id(), QStringLiteral("hybrid-r5-split"));
    QCOMPARE(root->orientation(), std::optional(Core::SplitOrientation::Vertical));
    QCOMPARE(root->firstChild()->id(), QStringLiteral("hybrid-r2-source-leaf"));
    QCOMPARE(root->firstChild()->windowId(), QStringLiteral("window-c"));
    QCOMPARE(root->secondChild()->id(), QStringLiteral("hybrid-r4-split"));
    QVERIFY(!container->findNode(QStringLiteral("hybrid-r2-split")));

    // Repeating an already-effective root placement rejects without a
    // revision, a tab never docks to an edge, and a member-less tab drop
    // inside its own container remains a no-op.
    const auto repeat = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle,
        {},
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Top));
    QCOMPARE(repeat.status, HybridRuntimeStatus::Rejected);
    const auto tab = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::Tab,
        {},
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Left,
        container->activePageId()));
    QCOMPARE(tab.status, HybridRuntimeStatus::Rejected);
    QVERIFY(tab.message.contains(QStringLiteral("tab destination")));
    const auto anchorless = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle,
        {},
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Tab));
    QCOMPARE(anchorless.status, HybridRuntimeStatus::NoChange);
    QCOMPARE(runtime.topology().revision(), quint64{5});
}

QTEST_APPLESS_MAIN(HybridInteractionContainerEdgesTest)

#include "tst_hybridinteractioncontaineredges.moc"
