// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridinteractionruntime_testfixtures.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::Compositor::KWinIntegration::Test;
namespace Core = QindaQt::Core;
namespace Hybrid = QindaQt::Hybrid;
namespace HybridInput = QindaQt::HybridInput;

class HybridInteractionTranslationTest final : public QObject
{
    Q_OBJECT

private slots:
    void mapsEveryEdgeZoneToSplitPlacement();
    void detachesGroupedMemberOnEmptyDrop();
    void movesMembersAndPagesAcrossContainers();
    void detachesTabsAsWholePages();
    void insertsIndependentIntoExistingContainer();
    void groupsIndependentWindowsAsTabs();
    void regroupsMemberWithIndependentTarget();
    void reordersPagesAndMembersWithinContainer();
    void rejectsStaleOwnershipAndPlacementNoOps();
    void sceneFailureRetryReusesDeterministicIds();
};

void HybridInteractionTranslationTest::mapsEveryEdgeZoneToSplitPlacement()
{
    struct Case final
    {
        HybridInput::DockZone zone;
        Core::SplitOrientation orientation;
        Core::InsertPosition sourcePosition;
    };
    const QVector<Case> cases{
        {HybridInput::DockZone::Left,
         Core::SplitOrientation::Horizontal,
         Core::InsertPosition::First},
        {HybridInput::DockZone::Right,
         Core::SplitOrientation::Horizontal,
         Core::InsertPosition::Second},
        {HybridInput::DockZone::Top,
         Core::SplitOrientation::Vertical,
         Core::InsertPosition::First},
        {HybridInput::DockZone::Bottom,
         Core::SplitOrientation::Vertical,
         Core::InsertPosition::Second},
    };

    for (const auto &testCase : cases) {
        RecordingSceneFactory scene;
        HybridInteractionRuntime runtime({QStringLiteral("source"),
                                          QStringLiteral("target")},
                                         scene);
        const auto result = dock(runtime,
                                 QStringLiteral("source"),
                                 QStringLiteral("target"),
                                 testCase.zone);
        QVERIFY2(result.topologyChanged(), qPrintable(result.message));
        const auto *container = runtime.topology().container(
            QStringLiteral("hybrid-r1-container"));
        QVERIFY(container);
        const auto &root = container->pages().constFirst().root();
        QCOMPARE(root.orientation(), std::optional(testCase.orientation));
        const auto *sourceNode = testCase.sourcePosition == Core::InsertPosition::First
            ? root.firstChild()
            : root.secondChild();
        const auto *targetNode = testCase.sourcePosition == Core::InsertPosition::First
            ? root.secondChild()
            : root.firstChild();
        QCOMPARE(sourceNode->windowId(), QStringLiteral("source"));
        QCOMPARE(targetNode->windowId(), QStringLiteral("target"));
        QCOMPARE(scene.kinds.constFirst(),
                 Hybrid::TopologyCommandKind::DockIndependentWindows);
    }
}

void HybridInteractionTranslationTest::detachesGroupedMemberOnEmptyDrop()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b")},
                                     scene);
    QVERIFY(dock(runtime,
                 QStringLiteral("window-a"),
                 QStringLiteral("window-b"),
                 HybridInput::DockZone::Right)
                .topologyChanged());

    const auto detached = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle,
        {},
        {},
        HybridInput::DockZone::None));
    QVERIFY2(detached.topologyChanged(), qPrintable(detached.message));
    QVERIFY(runtime.topology().containerIds().isEmpty());
    QCOMPARE(runtime.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-a"), QStringLiteral("window-b")}));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::DetachMember);
}

void HybridInteractionTranslationTest::movesMembersAndPagesAcrossContainers()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c"),
                                      QStringLiteral("window-d"),
                                      QStringLiteral("window-e"),
                                      QStringLiteral("window-f"),
                                      QStringLiteral("window-g"),
                                      QStringLiteral("window-h")},
                                     scene);
    QVERIFY(dock(runtime,
                 QStringLiteral("window-a"),
                 QStringLiteral("window-b"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    QVERIFY(dock(runtime,
                 QStringLiteral("window-c"),
                 QStringLiteral("window-d"),
                 HybridInput::DockZone::Right)
                .topologyChanged());

    const auto moved = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r2-container"),
        HybridInput::DockZone::Top));
    QVERIFY2(moved.topologyChanged(), qPrintable(moved.message));
    QVERIFY(!runtime.topology().container(QStringLiteral("hybrid-r1-container")));
    QVERIFY(runtime.topology().isIndependent(QStringLiteral("window-b")));
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-a")),
             std::optional<QString>(QStringLiteral("hybrid-r2-container")));
    const auto *newSplit = runtime.topology()
                               .container(QStringLiteral("hybrid-r2-container"))
                               ->findNode(QStringLiteral("hybrid-r3-split"));
    QVERIFY(newSplit);
    QCOMPARE(newSplit->orientation(),
             std::optional(Core::SplitOrientation::Vertical));
    QCOMPARE(newSplit->firstChild()->windowId(), QStringLiteral("window-a"));

    QVERIFY(dock(runtime,
                 QStringLiteral("window-e"),
                 QStringLiteral("window-f"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    QVERIFY(dock(runtime,
                 QStringLiteral("window-g"),
                 QStringLiteral("window-h"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    const auto seededSecondPage = runtime.handleIntent(memberCommit(
        QStringLiteral("window-g"),
        QStringLiteral("hybrid-r5-container"),
        HybridInput::HitKind::Tab,
        QStringLiteral("window-e"),
        QStringLiteral("hybrid-r4-container"),
        HybridInput::DockZone::Tab,
        QStringLiteral("hybrid-r5-page")));
    QVERIFY2(seededSecondPage.topologyChanged(), qPrintable(seededSecondPage.message));
    QCOMPARE(runtime.topology()
                 .container(QStringLiteral("hybrid-r4-container"))
                 ->pages().size(),
             qsizetype{2});

    const auto movedPage = runtime.handleIntent(memberCommit(
        QStringLiteral("window-e"),
        QStringLiteral("hybrid-r4-container"),
        HybridInput::HitKind::Tab,
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r2-container"),
        HybridInput::DockZone::Tab,
        QStringLiteral("hybrid-r4-page")));
    QVERIFY2(movedPage.topologyChanged(), qPrintable(movedPage.message));
    const auto *source = runtime.topology().container(
        QStringLiteral("hybrid-r4-container"));
    QVERIFY(source);
    QCOMPARE(source->pages().size(), qsizetype{1});
    QCOMPARE(source->pages().constFirst().id(), QStringLiteral("hybrid-r5-page"));
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-g")),
             std::optional<QString>(QStringLiteral("hybrid-r4-container")));
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-h")),
             std::optional<QString>(QStringLiteral("hybrid-r4-container")));
    const auto *target = runtime.topology().container(
        QStringLiteral("hybrid-r2-container"));
    QVERIFY(target);
    QCOMPARE(target->pages().size(), qsizetype{2});
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-f")),
             std::optional<QString>(QStringLiteral("hybrid-r2-container")));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::MovePage);
}

void HybridInteractionTranslationTest::detachesTabsAsWholePages()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c"),
                                      QStringLiteral("window-d")},
                                     scene);
    QVERIFY(dock(runtime, QStringLiteral("window-a"), QStringLiteral("window-b"),
                 HybridInput::DockZone::Right).topologyChanged());
    QVERIFY(dock(runtime, QStringLiteral("window-c"), QStringLiteral("window-d"),
                 HybridInput::DockZone::Tab).topologyChanged());
    const auto movedPage = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"), QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::Tab, QStringLiteral("window-c"),
        QStringLiteral("hybrid-r2-container"), HybridInput::DockZone::Tab,
        QStringLiteral("hybrid-r1-page")));
    QVERIFY2(movedPage.topologyChanged(), qPrintable(movedPage.message));

    const auto detachedSplitPage = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"), QStringLiteral("hybrid-r2-container"),
        HybridInput::HitKind::Tab, {}, {}, HybridInput::DockZone::None,
        QStringLiteral("hybrid-r1-page")));
    QVERIFY2(detachedSplitPage.topologyChanged(),
             qPrintable(detachedSplitPage.message));
    const auto *separated = runtime.topology().container(
        QStringLiteral("hybrid-r4-container"));
    QVERIFY(separated);
    QCOMPARE(separated->pages().constFirst().id(), QStringLiteral("hybrid-r1-page"));
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-a")),
             std::optional<QString>(QStringLiteral("hybrid-r4-container")));
    QCOMPARE(runtime.topology().ownerOf(QStringLiteral("window-b")),
             std::optional<QString>(QStringLiteral("hybrid-r4-container")));

    const auto detachedSinglePage = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"), QStringLiteral("hybrid-r2-container"),
        HybridInput::HitKind::Tab, {}, {}, HybridInput::DockZone::None,
        QStringLiteral("hybrid-r2-source-page")));
    QVERIFY2(detachedSinglePage.topologyChanged(),
             qPrintable(detachedSinglePage.message));
    QVERIFY(!runtime.topology().container(QStringLiteral("hybrid-r2-container")));
    QVERIFY(runtime.topology().isIndependent(QStringLiteral("window-c")));
    QVERIFY(runtime.topology().isIndependent(QStringLiteral("window-d")));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::DetachPage);
}

void HybridInteractionTranslationTest::insertsIndependentIntoExistingContainer()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c"),
                                      QStringLiteral("window-d")},
                                     scene);
    QVERIFY(dock(runtime,
                 QStringLiteral("window-a"),
                 QStringLiteral("window-b"),
                 HybridInput::DockZone::Right)
                .topologyChanged());

    const auto split = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"),
        {},
        HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-a"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Bottom));
    QVERIFY2(split.topologyChanged(), qPrintable(split.message));
    const auto *container = runtime.topology().container(
        QStringLiteral("hybrid-r1-container"));
    QVERIFY(container);
    QCOMPARE(container->findWindow(QStringLiteral("window-c"))->id(),
             QStringLiteral("hybrid-r2-source-leaf"));
    const auto *newSplit = container->findNode(QStringLiteral("hybrid-r2-split"));
    QVERIFY(newSplit);
    QCOMPARE(newSplit->orientation(),
             std::optional(Core::SplitOrientation::Vertical));
    QCOMPARE(newSplit->secondChild()->windowId(), QStringLiteral("window-c"));
    QCOMPARE(scene.kinds.constLast(),
             Hybrid::TopologyCommandKind::InsertIndependentWindow);

    const auto page = runtime.handleIntent(memberCommit(
        QStringLiteral("window-d"),
        {},
        HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-b"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Tab));
    QVERIFY2(page.topologyChanged(), qPrintable(page.message));
    container = runtime.topology().container(QStringLiteral("hybrid-r1-container"));
    QCOMPARE(container->pages().size(), qsizetype{2});
    QCOMPARE(container->pages()[1].id(), QStringLiteral("hybrid-r3-page"));
    QCOMPARE(container->pages()[1].root().windowId(), QStringLiteral("window-d"));
    QCOMPARE(scene.kinds.constLast(),
             Hybrid::TopologyCommandKind::InsertIndependentWindow);
}

void HybridInteractionTranslationTest::groupsIndependentWindowsAsTabs()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("source"),
                                      QStringLiteral("target")},
                                     scene);
    const auto grouped = dock(runtime,
                              QStringLiteral("source"),
                              QStringLiteral("target"),
                              HybridInput::DockZone::Tab);
    QVERIFY2(grouped.topologyChanged(), qPrintable(grouped.message));
    const auto *container = runtime.topology().container(
        QStringLiteral("hybrid-r1-container"));
    QVERIFY(container);
    QCOMPARE(container->pages().size(), qsizetype{2});
    QCOMPARE(container->pages()[0].id(), QStringLiteral("hybrid-r1-target-page"));
    QCOMPARE(container->pages()[0].root().windowId(), QStringLiteral("target"));
    QCOMPARE(container->pages()[1].id(), QStringLiteral("hybrid-r1-source-page"));
    QCOMPARE(container->pages()[1].root().windowId(), QStringLiteral("source"));
    QCOMPARE(container->activePageId(), QStringLiteral("hybrid-r1-target-page"));
    QCOMPARE(scene.kinds.constLast(),
             Hybrid::TopologyCommandKind::GroupIndependentWindowsAsPages);
}

void HybridInteractionTranslationTest::regroupsMemberWithIndependentTarget()
{
    {
        RecordingSceneFactory scene;
        HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                          QStringLiteral("window-b"),
                                          QStringLiteral("window-c")},
                                         scene);
        QVERIFY(dock(runtime,
                     QStringLiteral("window-a"),
                     QStringLiteral("window-b"),
                     HybridInput::DockZone::Right)
                    .topologyChanged());
        const auto regrouped = runtime.handleIntent(memberCommit(
            QStringLiteral("window-a"),
            QStringLiteral("hybrid-r1-container"),
            HybridInput::HitKind::MemberTitle,
            QStringLiteral("window-c"),
            {},
            HybridInput::DockZone::Left));
        QVERIFY2(regrouped.topologyChanged(), qPrintable(regrouped.message));
        QVERIFY(!runtime.topology().container(QStringLiteral("hybrid-r1-container")));
        QVERIFY(runtime.topology().isIndependent(QStringLiteral("window-b")));
        const auto *group = runtime.topology().container(
            QStringLiteral("hybrid-r2-container"));
        QVERIFY(group);
        QCOMPARE(group->findWindow(QStringLiteral("window-a"))->id(),
                 QStringLiteral("hybrid-r1-source-leaf"));
        const auto *split = group->findNode(QStringLiteral("hybrid-r2-split"));
        QCOMPARE(split->firstChild()->windowId(), QStringLiteral("window-a"));
        QCOMPARE(split->secondChild()->windowId(), QStringLiteral("window-c"));
        QCOMPARE(scene.kinds.constLast(),
                 Hybrid::TopologyCommandKind::RegroupMemberWithIndependent);
    }

    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-x"),
                                      QStringLiteral("window-y"),
                                      QStringLiteral("window-z")},
                                     scene);
    QVERIFY(dock(runtime,
                 QStringLiteral("window-x"),
                 QStringLiteral("window-y"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    const auto tabbed = runtime.handleIntent(memberCommit(
        QStringLiteral("window-x"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-z"),
        {},
        HybridInput::DockZone::Tab));
    QVERIFY2(tabbed.topologyChanged(), qPrintable(tabbed.message));
    const auto *group = runtime.topology().container(QStringLiteral("hybrid-r2-container"));
    QVERIFY(group);
    QCOMPARE(group->pages().size(), qsizetype{2});
    QCOMPARE(group->pages()[0].root().windowId(), QStringLiteral("window-z"));
    QCOMPARE(group->pages()[1].root().windowId(), QStringLiteral("window-x"));
    QCOMPARE(group->pages()[1].root().id(), QStringLiteral("hybrid-r1-source-leaf"));
    QVERIFY(runtime.topology().isIndependent(QStringLiteral("window-y")));
}

void HybridInteractionTranslationTest::reordersPagesAndMembersWithinContainer()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c"),
                                      QStringLiteral("window-d")},
                                     scene);
    QVERIFY(dock(runtime,
                 QStringLiteral("window-a"),
                 QStringLiteral("window-b"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    QVERIFY(dock(runtime,
                 QStringLiteral("window-c"),
                 QStringLiteral("window-d"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    const auto movedPage = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r2-container"),
        HybridInput::DockZone::Tab));
    QVERIFY(movedPage.topologyChanged());

    const auto reordered = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r2-container"),
        HybridInput::HitKind::Tab,
        QStringLiteral("window-a"),
        QStringLiteral("hybrid-r2-container"),
        HybridInput::DockZone::Tab,
        QStringLiteral("hybrid-r2-page")));
    QVERIFY2(reordered.topologyChanged(), qPrintable(reordered.message));
    const auto *container = runtime.topology().container(
        QStringLiteral("hybrid-r2-container"));
    QVERIFY(container);
    QCOMPARE(container->pages()[0].id(), QStringLiteral("hybrid-r3-page"));
    QCOMPARE(container->pages()[1].id(), QStringLiteral("hybrid-r2-page"));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::ReorderPage);

    const QString sourceNode = container->findWindow(QStringLiteral("window-c"))->id();
    const QString targetNode = container->findWindow(QStringLiteral("window-a"))->id();
    const auto reparented = runtime.handleIntent(memberCommit(
        QStringLiteral("window-c"),
        QStringLiteral("hybrid-r2-container"),
        HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-a"),
        QStringLiteral("hybrid-r2-container"),
        HybridInput::DockZone::Left));
    QVERIFY2(reparented.topologyChanged(), qPrintable(reparented.message));
    container = runtime.topology().container(QStringLiteral("hybrid-r2-container"));
    QCOMPARE(container->findWindow(QStringLiteral("window-c"))->id(), sourceNode);
    QCOMPARE(container->findWindow(QStringLiteral("window-a"))->id(), targetNode);
    const auto *split = container->findNode(QStringLiteral("hybrid-r5-split"));
    QVERIFY(split);
    QCOMPARE(split->firstChild()->windowId(), QStringLiteral("window-c"));
    QCOMPARE(split->secondChild()->windowId(), QStringLiteral("window-a"));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::ReparentMember);
}

void HybridInteractionTranslationTest::rejectsStaleOwnershipAndPlacementNoOps()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b")},
                                     scene);
    QVERIFY(dock(runtime,
                 QStringLiteral("window-a"),
                 QStringLiteral("window-b"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    const quint64 beforeRevision = runtime.topology().revision();

    const auto noOp = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::HitKind::MemberTitle,
        QStringLiteral("window-b"),
        QStringLiteral("hybrid-r1-container"),
        HybridInput::DockZone::Right));
    QCOMPARE(noOp.status, HybridRuntimeStatus::Rejected);
    const auto stale = runtime.handleIntent(memberCommit(
        QStringLiteral("window-a"),
        QStringLiteral("stale-container"),
        HybridInput::HitKind::MemberTitle,
        {},
        {},
        HybridInput::DockZone::None));
    QCOMPARE(stale.status, HybridRuntimeStatus::Rejected);
    QCOMPARE(runtime.topology().revision(), beforeRevision);
}

void HybridInteractionTranslationTest::sceneFailureRetryReusesDeterministicIds()
{
    RecordingSceneFactory scene({false, true});
    HybridInteractionRuntime runtime({QStringLiteral("source"),
                                      QStringLiteral("target")},
                                     scene);
    const auto first = dock(runtime,
                            QStringLiteral("source"),
                            QStringLiteral("target"),
                            HybridInput::DockZone::Right);
    QCOMPARE(first.status, HybridRuntimeStatus::Rejected);
    QCOMPARE(runtime.topology().revision(), quint64{0});
    QVERIFY(runtime.topology().containerIds().isEmpty());

    const auto retry = dock(runtime,
                            QStringLiteral("source"),
                            QStringLiteral("target"),
                            HybridInput::DockZone::Right);
    QVERIFY2(retry.topologyChanged(), qPrintable(retry.message));
    QCOMPARE(runtime.topology().containerIds(),
             QStringList({QStringLiteral("hybrid-r1-container")}));
    QCOMPARE(scene.candidateRevisions, QVector<quint64>({1, 1}));
    QCOMPARE(scene.rolledBack, 1);
}

QTEST_APPLESS_MAIN(HybridInteractionTranslationTest)

#include "tst_hybridinteractiontranslation.moc"
