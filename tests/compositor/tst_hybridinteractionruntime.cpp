// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridinteractionruntime_testfixtures.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::Compositor::KWinIntegration::Test;
namespace Hybrid = QindaQt::Hybrid;
namespace HybridInput = QindaQt::HybridInput;

class HybridInteractionRuntimeTest final : public QObject
{
    Q_OBJECT

private slots:
    void validatesAndReconcilesLifecycle();
    void forwardsPreviewsWithoutMutation();
    void delegatesNarrowDirectInteractions();
    void reportsUnavailableAndRejectedContainerResizeDelegates();
    void activatesTabsAndResizesDividersAtomically();
    void selectsFirstActivePageSplitDeterministically();
    void releasesOneRequestedContainer();
    void releasesEveryContainerInStableOrder();
    void continuesReleaseAfterSceneFailure();
};

void HybridInteractionRuntimeTest::validatesAndReconcilesLifecycle()
{
    RecordingSceneFactory invalidScene;
    HybridInteractionRuntime invalid({QStringLiteral("duplicate"),
                                      QStringLiteral("duplicate")},
                                     invalidScene);
    QVERIFY(!invalid.ready());
    QVERIFY(invalid.initializationError().contains(QStringLiteral("duplicate")));
    QCOMPARE(invalid.addWindow(QStringLiteral("new")).status,
             HybridRuntimeStatus::Rejected);
    QCOMPARE(invalidScene.created, 0);

    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({}, scene);
    QVERIFY(runtime.ready());
    const auto addedA = runtime.addWindow(QStringLiteral("window-a"));
    const auto addedB = runtime.addWindow(QStringLiteral("window-b"));
    QVERIFY(addedA.topologyChanged());
    QVERIFY(addedB.topologyChanged());
    QCOMPARE(runtime.topology().revision(), quint64{2});
    QCOMPARE(runtime.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-a"), QStringLiteral("window-b")}));

    const auto duplicate = runtime.addWindow(QStringLiteral("window-a"));
    QCOMPARE(duplicate.status, HybridRuntimeStatus::Rejected);
    QCOMPARE(runtime.topology().revision(), quint64{2});
    const auto forgotten = runtime.forgetWindow(QStringLiteral("window-b"));
    QVERIFY(forgotten.topologyChanged());
    QCOMPARE(runtime.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-a")}));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::ForgetWindow);
}

void HybridInteractionRuntimeTest::forwardsPreviewsWithoutMutation()
{
    QVector<HybridInput::IntentPhase> phases;
    HybridRuntimeCallbacks callbacks;
    callbacks.preview = [&](const auto &intent) { phases.append(intent.phase); };
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a")}, scene, callbacks);

    for (const auto phase : {HybridInput::IntentPhase::Begin,
                             HybridInput::IntentPhase::Update,
                             HybridInput::IntentPhase::Cancel}) {
        HybridInput::InteractionIntent intent;
        intent.kind = HybridInput::InteractionKind::MemberDock;
        intent.phase = phase;
        intent.source = {HybridInput::HitKind::MemberTitle,
                         {},
                         QStringLiteral("window-a"),
                         {}};
        QCOMPARE(runtime.handleIntent(intent).status, HybridRuntimeStatus::PreviewOnly);
    }

    QCOMPARE(phases,
             QVector<HybridInput::IntentPhase>({HybridInput::IntentPhase::Begin,
                                                HybridInput::IntentPhase::Update,
                                                HybridInput::IntentPhase::Cancel}));
    QCOMPARE(runtime.topology().revision(), quint64{0});
    QCOMPARE(scene.created, 0);
}

void HybridInteractionRuntimeTest::delegatesNarrowDirectInteractions()
{
    QVector<HybridInput::IntentPhase> movePhases;
    QVector<HybridInput::IntentPhase> resizePhases;
    HybridRuntimeCallbacks callbacks;
    callbacks.containerMove = [&](const auto &intent) {
        movePhases.append(intent.phase);
        return DirectInteractionResult::handled();
    };
    callbacks.containerResize = [&](const auto &intent) {
        resizePhases.append(intent.phase);
        return DirectInteractionResult::handled();
    };
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({}, scene, callbacks);

    HybridInput::InteractionIntent move;
    move.kind = HybridInput::InteractionKind::ContainerMove;
    move.phase = HybridInput::IntentPhase::Update;
    move.source = {HybridInput::HitKind::OuterTitle,
                   QStringLiteral("container"),
                   {},
                   {}};
    QCOMPARE(runtime.handleIntent(move).status, HybridRuntimeStatus::Delegated);
    move.phase = HybridInput::IntentPhase::Commit;
    QCOMPARE(runtime.handleIntent(move).status, HybridRuntimeStatus::Delegated);
    QCOMPARE(movePhases,
             QVector<HybridInput::IntentPhase>({HybridInput::IntentPhase::Update,
                                                HybridInput::IntentPhase::Commit}));

    HybridInput::InteractionIntent resize;
    resize.kind = HybridInput::InteractionKind::ContainerResize;
    resize.phase = HybridInput::IntentPhase::Begin;
    resize.source = {HybridInput::HitKind::OuterResize,
                     QStringLiteral("container"), {}, {},
                     Qt::RightEdge | Qt::BottomEdge};
    QCOMPARE(runtime.handleIntent(resize).status, HybridRuntimeStatus::Delegated);
    resize.phase = HybridInput::IntentPhase::Commit;
    QCOMPARE(runtime.handleIntent(resize).status, HybridRuntimeStatus::Delegated);
    QCOMPARE(resizePhases,
             QVector<HybridInput::IntentPhase>({HybridInput::IntentPhase::Begin,
                                                HybridInput::IntentPhase::Commit}));

    HybridInput::InteractionIntent divider;
    divider.kind = HybridInput::InteractionKind::DividerResize;
    divider.phase = HybridInput::IntentPhase::Commit;
    QCOMPARE(runtime.handleIntent(divider).status, HybridRuntimeStatus::NeedsGeometry);
    QCOMPARE(runtime.topology().revision(), quint64{0});
    QCOMPARE(scene.created, 0);
}

void HybridInteractionRuntimeTest::reportsUnavailableAndRejectedContainerResizeDelegates()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime unavailable({}, scene);
    HybridInput::InteractionIntent resize;
    resize.kind = HybridInput::InteractionKind::ContainerResize;
    resize.phase = HybridInput::IntentPhase::Begin;
    QCOMPARE(unavailable.handleIntent(resize).status,
             HybridRuntimeStatus::Unsupported);

    HybridRuntimeCallbacks callbacks;
    callbacks.containerResize = [](const auto &) {
        return DirectInteractionResult::rejected(QStringLiteral("resize sentinel"));
    };
    HybridInteractionRuntime rejected({}, scene, callbacks);
    const auto result = rejected.handleIntent(resize);
    QCOMPARE(result.status, HybridRuntimeStatus::Rejected);
    QCOMPARE(result.message, QStringLiteral("resize sentinel"));
    QCOMPARE(rejected.topology().revision(), quint64{0});
    QCOMPARE(scene.created, 0);
}

void HybridInteractionRuntimeTest::activatesTabsAndResizesDividersAtomically()
{
    RecordingSceneFactory scene;
    HybridRuntimeCallbacks callbacks;
    callbacks.dividerResize = [](const auto &) {
        return DividerGeometryResult::resolved(0.75);
    };
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c"),
                                      QStringLiteral("window-d")},
                                     scene,
                                     callbacks);
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
    QVERIFY(runtime.handleIntent(memberCommit(
                 QStringLiteral("window-a"),
                 QStringLiteral("hybrid-r1-container"),
                 HybridInput::HitKind::MemberTitle,
                 QStringLiteral("window-c"),
                 QStringLiteral("hybrid-r2-container"),
                 HybridInput::DockZone::Tab))
                .topologyChanged());

    const auto activated = runtime.activatePage(QStringLiteral("hybrid-r2-container"),
                                                QStringLiteral("hybrid-r3-page"));
    QVERIFY2(activated.topologyChanged(), qPrintable(activated.message));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::ActivatePage);
    QCOMPARE(runtime.topology()
                 .container(QStringLiteral("hybrid-r2-container"))
                 ->activePageId(),
             QStringLiteral("hybrid-r3-page"));

    const auto resized = runtime.resizeSplit(QStringLiteral("hybrid-r2-container"),
                                             QStringLiteral("hybrid-r2-split"),
                                             0.65);
    QVERIFY2(resized.topologyChanged(), qPrintable(resized.message));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::ResizeSplit);
    QCOMPARE(runtime.topology()
                 .container(QStringLiteral("hybrid-r2-container"))
                 ->findNode(QStringLiteral("hybrid-r2-split"))
                 ->ratio(),
             std::optional(0.65));

    QCOMPARE(runtime.activatePage(QStringLiteral("hybrid-r2-container"),
                                  QStringLiteral("hybrid-r3-page"))
                 .status,
             HybridRuntimeStatus::Rejected);
    QCOMPARE(runtime.resizeSplit(QStringLiteral("hybrid-r2-container"),
                                 QStringLiteral("hybrid-r2-split"),
                                 0.65)
                 .status,
             HybridRuntimeStatus::Rejected);

    HybridInput::InteractionIntent divider;
    divider.kind = HybridInput::InteractionKind::DividerResize;
    divider.phase = HybridInput::IntentPhase::Commit;
    divider.source = {HybridInput::HitKind::Divider,
                      QStringLiteral("hybrid-r2-container"),
                      {},
                      QStringLiteral("hybrid-r2-split")};
    const auto intentResize = runtime.handleIntent(divider);
    QVERIFY2(intentResize.topologyChanged(), qPrintable(intentResize.message));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::ResizeSplit);
    QCOMPARE(runtime.topology()
                 .container(QStringLiteral("hybrid-r2-container"))
                 ->findNode(QStringLiteral("hybrid-r2-split"))
                 ->ratio(),
             std::optional(0.75));
}

void HybridInteractionRuntimeTest::selectsFirstActivePageSplitDeterministically()
{
    RecordingSceneFactory scene;
    HybridInteractionRuntime runtime({QStringLiteral("window-a"),
                                      QStringLiteral("window-b"),
                                      QStringLiteral("window-c")}, scene);
    QVERIFY(dock(runtime,
                 QStringLiteral("window-a"),
                 QStringLiteral("window-b"),
                 HybridInput::DockZone::Right)
                .topologyChanged());
    QVERIFY(runtime.handleIntent(memberCommit(
                 QStringLiteral("window-c"), {}, HybridInput::HitKind::MemberTitle,
                 QStringLiteral("window-b"), QStringLiteral("hybrid-r1-container"),
                 HybridInput::DockZone::Bottom))
                .topologyChanged());
    QVERIFY(runtime.topology()
                .container(QStringLiteral("hybrid-r1-container"))
                ->findNode(QStringLiteral("hybrid-r2-split")));
    QCOMPARE(runtime.activePageFirstSplitId(QStringLiteral("hybrid-r1-container")),
             QStringLiteral("hybrid-r1-split"));
    QVERIFY(runtime.activePageFirstSplitId(QStringLiteral("missing")).isEmpty());
}

void HybridInteractionRuntimeTest::releasesEveryContainerInStableOrder()
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
                 HybridInput::DockZone::Bottom)
                .topologyChanged());

    const auto released = runtime.releaseAll();
    QVERIFY2(released.complete, qPrintable(released.message));
    QCOMPARE(released.commands.size(), qsizetype{2});
    QCOMPARE(released.commands[0].kind, Hybrid::TopologyCommandKind::ReleaseContainer);
    QCOMPARE(released.commands[0].previousRevision, quint64{2});
    QCOMPARE(released.commands[1].previousRevision, quint64{3});
    QVERIFY(runtime.topology().containerIds().isEmpty());
    QCOMPARE(runtime.topology().independentWindowIds().size(), qsizetype{4});
}

void HybridInteractionRuntimeTest::releasesOneRequestedContainer()
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
                 HybridInput::DockZone::Bottom)
                .topologyChanged());

    const auto released =
        runtime.releaseContainer(QStringLiteral("hybrid-r1-container"));
    QVERIFY2(released.topologyChanged(), qPrintable(released.message));
    QCOMPARE(scene.kinds.constLast(), Hybrid::TopologyCommandKind::ReleaseContainer);
    QCOMPARE(runtime.topology().containerIds(),
             QStringList({QStringLiteral("hybrid-r2-container")}));
    QCOMPARE(runtime.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-a"), QStringLiteral("window-b")}));

    const auto unknown = runtime.releaseContainer(QStringLiteral("missing"));
    QCOMPARE(unknown.status, HybridRuntimeStatus::Rejected);
    QCOMPARE(runtime.topology().revision(), quint64{3});
}

void HybridInteractionRuntimeTest::continuesReleaseAfterSceneFailure()
{
    RecordingSceneFactory scene({true, true, false, true});
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

    const auto released = runtime.releaseAll();
    QVERIFY(!released.complete);
    QCOMPARE(released.commands.size(), qsizetype{2});
    QVERIFY(!released.commands[0].committed());
    QVERIFY(released.commands[1].committed());
    QCOMPARE(runtime.topology().containerIds(),
             QStringList({QStringLiteral("hybrid-r1-container")}));
    QCOMPARE(scene.rolledBack, 1);
}

QTEST_APPLESS_MAIN(HybridInteractionRuntimeTest)

#include "tst_hybridinteractionruntime.moc"
