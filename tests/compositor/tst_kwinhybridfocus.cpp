// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene_testfixture.h"

#include "hybridinteractionruntime.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Compositor;

namespace {

HybridInput::InteractionIntent dockIntent(const QString &source,
                                          const QString &target)
{
    HybridInput::InteractionIntent intent;
    intent.kind = HybridInput::InteractionKind::MemberDock;
    intent.phase = HybridInput::IntentPhase::Commit;
    intent.source.kind = HybridInput::HitKind::MemberTitle;
    intent.source.memberId = source;
    intent.target.memberId = target;
    intent.target.zone = HybridInput::DockZone::Right;
    return intent;
}

Test::FakeHybridScenePlatform::Window windowAt(int x, bool focused = false)
{
    const QRectF frame(x, 40, 280, 220);
    return Test::fakeWindow(
        Test::richState(frame, QStringLiteral("output-a"), focused), frame);
}

} // namespace

class KWinHybridFocusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void lifecycleCommandsPreserveUntouchedIndependentFocus();
    void lifecycleCommandsPreserveNonTopologyDialogFocus();
    void failedCoordinatorRollbackRestoresNonTopologyDialogFocus();
    void failedDirectReflowRollbackRestoresNonTopologyDialogFocus();
    void multiGroupReleaseAllPreservesOutsideFocus();
};

void KWinHybridFocusTest::lifecycleCommandsPreserveUntouchedIndependentFocus()
{
    Test::FakeHybridScenePlatform platform;
    platform.addWindow(QStringLiteral("a"), windowAt(0));
    platform.addWindow(QStringLiteral("b"), windowAt(320));
    platform.addWindow(QStringLiteral("outside"), windowAt(640, true));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    KWinIntegration::HybridInteractionRuntime runtime(
        {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("outside")},
        scene);

    const auto grouped = runtime.handleIntent(
        dockIntent(QStringLiteral("b"), QStringLiteral("a")));
    QVERIFY2(grouped.topologyChanged(), qPrintable(grouped.message));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));

    platform.addWindow(QStringLiteral("new"), windowAt(960));
    const auto added = runtime.addWindow(QStringLiteral("new"));
    QVERIFY2(added.topologyChanged(), qPrintable(added.message));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));

    platform.removeWindow(QStringLiteral("new"));
    const auto independentForgotten = runtime.forgetWindow(QStringLiteral("new"));
    QVERIFY2(independentForgotten.topologyChanged(),
             qPrintable(independentForgotten.message));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));

    platform.removeWindow(QStringLiteral("b"));
    const auto groupedForgotten = runtime.forgetWindow(QStringLiteral("b"));
    QVERIFY2(groupedForgotten.topologyChanged(), qPrintable(groupedForgotten.message));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));
    QVERIFY(runtime.topology().containerIds().isEmpty());
    QVERIFY(runtime.topology().isIndependent(QStringLiteral("a")));
}

void KWinHybridFocusTest::lifecycleCommandsPreserveNonTopologyDialogFocus()
{
    Test::FakeHybridScenePlatform platform;
    platform.addWindow(QStringLiteral("a"), windowAt(0));
    platform.addWindow(QStringLiteral("b"), windowAt(320));
    // This models a live transient/dialog known to KWin but intentionally
    // omitted from ManagedWindowRegistry and Hybrid WindowTopology.
    platform.addFocusOnlyWindow(QStringLiteral("dialog"), true);
    QVERIFY(!platform.windowExists(QStringLiteral("dialog")));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    KWinIntegration::HybridInteractionRuntime runtime(
        {QStringLiteral("a"), QStringLiteral("b")}, scene);

    QVERIFY(runtime.handleIntent(dockIntent(QStringLiteral("b"),
                                            QStringLiteral("a")))
                .topologyChanged());
    QCOMPARE(platform.activeWindowId(), QStringLiteral("dialog"));

    platform.addWindow(QStringLiteral("new"), windowAt(960));
    QVERIFY(runtime.addWindow(QStringLiteral("new")).topologyChanged());
    QCOMPARE(platform.activeWindowId(), QStringLiteral("dialog"));

    platform.removeWindow(QStringLiteral("new"));
    QVERIFY(runtime.forgetWindow(QStringLiteral("new")).topologyChanged());
    QCOMPARE(platform.activeWindowId(), QStringLiteral("dialog"));

    const auto released = runtime.releaseAll();
    QVERIFY2(released.complete, qPrintable(released.message));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("dialog"));
}

void KWinHybridFocusTest::failedCoordinatorRollbackRestoresNonTopologyDialogFocus()
{
    Test::FakeHybridScenePlatform platform;
    platform.addWindow(QStringLiteral("a"), windowAt(0));
    platform.addWindow(QStringLiteral("b"), windowAt(320));
    platform.addFocusOnlyWindow(QStringLiteral("dialog"), true);
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    KWinIntegration::HybridInteractionRuntime runtime(
        {QStringLiteral("a"), QStringLiteral("b")}, scene);
    platform.failFinalize = true;

    const auto failed = runtime.handleIntent(
        dockIntent(QStringLiteral("b"), QStringLiteral("a")));

    QVERIFY(!failed.topologyChanged());
    QVERIFY(runtime.topology().containerIds().isEmpty());
    QVERIFY(!platform.windowExists(QStringLiteral("dialog")));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("dialog"));
}

void KWinHybridFocusTest::failedDirectReflowRollbackRestoresNonTopologyDialogFocus()
{
    Test::FakeHybridScenePlatform platform;
    platform.addWindow(QStringLiteral("a"), windowAt(0));
    platform.addWindow(QStringLiteral("b"), windowAt(320));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    KWinIntegration::HybridInteractionRuntime runtime(
        {QStringLiteral("a"), QStringLiteral("b")}, scene);
    QVERIFY(runtime.handleIntent(dockIntent(QStringLiteral("b"),
                                            QStringLiteral("a")))
                .topologyChanged());
    const auto containerId = runtime.topology().containerIds().constFirst();
    platform.addFocusOnlyWindow(QStringLiteral("dialog"), true);
    platform.failFinalize = true;

    const auto failed = scene.reflowContainer(
        *runtime.topology().container(containerId), QRect(40, 50, 800, 500));

    QVERIFY(!failed.succeeded);
    QVERIFY(!platform.windowExists(QStringLiteral("dialog")));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("dialog"));
}

void KWinHybridFocusTest::multiGroupReleaseAllPreservesOutsideFocus()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> original;
    const QStringList ids{QStringLiteral("a"), QStringLiteral("b"),
                          QStringLiteral("c"), QStringLiteral("d"),
                          QStringLiteral("outside")};
    for (qsizetype index = 0; index < ids.size(); ++index) {
        auto window = windowAt(static_cast<int>(index) * 320,
                               ids[index] == QStringLiteral("outside"));
        original.insert(ids[index], window.state);
        platform.addWindow(ids[index], std::move(window));
    }
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    KWinIntegration::HybridInteractionRuntime runtime(ids, scene);

    QVERIFY(runtime.handleIntent(dockIntent(QStringLiteral("b"),
                                            QStringLiteral("a")))
                .topologyChanged());
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));
    QVERIFY(runtime.handleIntent(dockIntent(QStringLiteral("d"),
                                            QStringLiteral("c")))
                .topologyChanged());
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));
    QCOMPARE(runtime.topology().containerIds().size(), qsizetype{2});

    const auto released = runtime.releaseAll();
    QVERIFY2(released.complete, qPrintable(released.message));
    QCOMPARE(released.commands.size(), qsizetype{2});
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));
    QVERIFY(runtime.topology().containerIds().isEmpty());
    for (const auto &id : ids) {
        QCOMPARE(platform.windows.value(id).state, original.value(id));
        QVERIFY(platform.owners.value(id).isEmpty());
    }
}

QTEST_GUILESS_MAIN(KWinHybridFocusTest)

#include "tst_kwinhybridfocus.moc"
