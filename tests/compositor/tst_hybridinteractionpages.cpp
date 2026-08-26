// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridinteractionruntime_testfixtures.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::Compositor::KWinIntegration::Test;
namespace Hybrid = QindaQt::Hybrid;
namespace HybridInput = QindaQt::HybridInput;

class HybridInteractionPagesTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void memberTitleDropExtractsOnlyThatMemberAsPage();
    void tabDropOnIndependentMovesCompletePageTree();
};

void HybridInteractionPagesTest::memberTitleDropExtractsOnlyThatMemberAsPage()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c")},
                                     scene);
    QVERIFY(dock(runtime, QStringLiteral("window-a"), QStringLiteral("window-b"),
                 HybridInput::DockZone::Right).topologyChanged());
    QVERIFY(runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"), {}, HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-b"), QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Tab)).topologyChanged());

    const auto extracted = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"), QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle, QStringLiteral("window-c"),
        QStringLiteral("hybrid-r1-container"), HybridInput::DockZone::Tab));
    QVERIFY2(extracted.topologyChanged(), qPrintable(extracted.message));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::MoveMemberToPage);

    const auto *container = runtime.topology().container(
        QStringLiteral("hybrid-r1-container"));
    QVERIFY(container);
    QCOMPARE(container->pages().size(), qsizetype{3});
    QCOMPARE(container->pages()[0].root().windowId(), QStringLiteral("window-b"));
    QCOMPARE(container->pages()[1].id(), QStringLiteral("hybrid-r2-page"));
    QCOMPARE(container->pages()[1].root().windowId(), QStringLiteral("window-c"));
    QCOMPARE(container->pages()[2].id(), QStringLiteral("hybrid-r3-page"));
    QCOMPARE(container->pages()[2].root().windowId(), QStringLiteral("window-a"));
    QCOMPARE(container->pages()[2].root().id(),
             QStringLiteral("hybrid-r1-source-leaf"));
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-a")),
             std::optional<QString>(QStringLiteral("hybrid-r1-container")));
}

void HybridInteractionPagesTest::tabDropOnIndependentMovesCompletePageTree()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c"),
                                      QStringLiteral("window-d")},
                                     scene);
    QVERIFY(dock(runtime, QStringLiteral("window-a"), QStringLiteral("window-b"),
                 HybridInput::DockZone::Right).topologyChanged());
    QVERIFY(runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"), {}, HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-b"), QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Tab)).topologyChanged());

    const auto regrouped = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"), QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::Tab, QStringLiteral("window-d"), {},
        HybridInput::DockZone::Tab, QStringLiteral("hybrid-r1-page")));
    QVERIFY2(regrouped.topologyChanged(), qPrintable(regrouped.message));
    QCOMPARE(scene.kinds.constLast(),
             Hybrid::TopologyCommandKind::RegroupPageWithIndependent);
    QVERIFY(!runtime.topology().container(QStringLiteral("hybrid-r1-container")));
    QVERIFY(runtime.topology().isIndependent(QStringLiteral("window-c")));

    const auto *group = runtime.topology().container(
        QStringLiteral("hybrid-r3-container"));
    QVERIFY(group);
    QCOMPARE(group->pages().size(), qsizetype{2});
    QCOMPARE(group->pages()[0].id(), QStringLiteral("hybrid-r3-target-page"));
    QCOMPARE(group->pages()[0].root().windowId(), QStringLiteral("window-d"));
    QCOMPARE(group->pages()[1].id(), QStringLiteral("hybrid-r1-page"));
    QCOMPARE(group->pages()[1].root().id(), QStringLiteral("hybrid-r1-split"));
    QCOMPARE(group->pages()[1].root().firstChild()->id(),
             QStringLiteral("hybrid-r1-target-leaf"));
    QCOMPARE(group->pages()[1].root().secondChild()->id(),
             QStringLiteral("hybrid-r1-source-leaf"));
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-a")),
             std::optional<QString>(QStringLiteral("hybrid-r3-container")));
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-b")),
             std::optional<QString>(QStringLiteral("hybrid-r3-container")));

    // Whole-page-to-split geometry is intentionally not inferred from a tab
    // representative. Reject it until a typed subtree-as-split command exists.
    const quint64 revision = runtime.topology().revision();
    const auto rejected = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"), QStringLiteral("hybrid-r3-container"),
        HybridInput::HitKind::Tab, QStringLiteral("window-c"), {},
        HybridInput::DockZone::Left, QStringLiteral("hybrid-r1-page")));
    QCOMPARE(rejected.status, HybridRuntimeStatus::Rejected);
    QCOMPARE(runtime.topology().revision(), revision);
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-b")),
             std::optional<QString>(QStringLiteral("hybrid-r3-container")));
}

QTEST_APPLESS_MAIN(HybridInteractionPagesTest)

#include "tst_hybridinteractionpages.moc"
