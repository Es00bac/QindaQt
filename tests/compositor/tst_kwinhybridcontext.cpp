// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene_testfixture.h"

#include "qindaqt/hybrid/topologycoordinator.h"

#include <QTest>

using namespace QindaQt;
using namespace QindaQt::Compositor;

namespace {

Hybrid::DockIndependentWindows dockCommand()
{
    return {
        .containerId = QStringLiteral("group"),
        .pageId = QStringLiteral("page"),
        .firstWindowId = QStringLiteral("a"),
        .firstLeafNodeId = QStringLiteral("leaf-a"),
        .secondWindowId = QStringLiteral("b"),
        .secondLeafNodeId = QStringLiteral("leaf-b"),
        .splitNodeId = QStringLiteral("split"),
        .orientation = Core::SplitOrientation::Horizontal,
        .ratio = 0.5,
        .secondPosition = Core::InsertPosition::Second,
    };
}

Hybrid::WindowTopology independentTopology()
{
    QString error;
    const auto result = Hybrid::WindowTopology::create(
        {QStringLiteral("a"), QStringLiteral("b")}, {}, 0, &error);
    Q_ASSERT_X(result.has_value(), "group context fixture", qPrintable(error));
    return *result;
}

KWinIntegration::HybridGroupContext contextOf(
    const HybridConstraints::WindowRestoreState &state)
{
    return {
        .outputId = state.outputId,
        .desktopIds = state.desktopIds,
        .activityIds = state.activityIds,
        .keepAbove = state.keepAbove,
        .keepBelow = state.keepBelow,
    };
}

struct Fixture final
{
    Fixture()
        : repository(independentTopology())
        , scene(platform, {.contentInsets = QMargins(4, 32, 4, 4),
                           .dividerThickness = 2})
        , coordinator(repository, scene)
    {
        platform.outputAreas.insert(QStringLiteral("left"),
                                    QRectF(0, 0, 1920, 1080));
        platform.outputAreas.insert(QStringLiteral("right"),
                                    QRectF(1920, 0, 2560, 1440));
        const auto first = Test::richState(
            QRectF(100, 100, 900, 600), QStringLiteral("left"), true);
        const auto second = Test::richState(
            QRectF(1040, 100, 700, 600), QStringLiteral("left"));
        originals.insert(QStringLiteral("a"), first);
        originals.insert(QStringLiteral("b"), second);
        platform.addWindow(QStringLiteral("a"),
                           Test::fakeWindow(first, first.geometry));
        platform.addWindow(QStringLiteral("b"),
                           Test::fakeWindow(second, second.geometry));
        const auto grouped = coordinator.execute(dockCommand());
        Q_ASSERT_X(grouped.committed(), "group context fixture",
                   qPrintable(grouped.message));
    }

    Test::FakeHybridScenePlatform platform;
    Hybrid::TopologyRepository repository;
    KWinIntegration::KWinHybridSceneFactory scene;
    Hybrid::TopologyCoordinator coordinator;
    QHash<QString, HybridConstraints::WindowRestoreState> originals;
};

} // namespace

class KWinHybridContextTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void memberWorkspaceAndLayerChangePropagatesAtomically();
    void memberOutputChangeMapsCompleteOuterFrame();
    void failedFinalizationRestoresUniformContextAndLayout();
    void planningFailureRestoresAlreadyChangedSource();
    void earlierApplyFailureStillRestoresLaterSource();
    void failedAdoptionAndReleaseRemainQuarantinedAfterReconciliation();
    void releaseAfterContextChangeRestoresIndependentStates();
};

void KWinHybridContextTest::memberWorkspaceAndLayerChangePropagatesAtomically()
{
    Fixture fixture;
    auto &source = fixture.platform.windows[QStringLiteral("b")].state;
    source.desktopIds = {QStringLiteral("desktop-b")};
    source.activityIds = {QStringLiteral("activity-b")};
    source.keepAbove = true;
    const auto expected = contextOf(source);

    const auto result = fixture.scene.recontextualizeContainer(
        *fixture.repository.topology().container(QStringLiteral("group")),
        QStringLiteral("b"));

    QVERIFY2(result.succeeded, qPrintable(result.message));
    QCOMPARE(contextOf(fixture.platform.windows.value(QStringLiteral("a")).state),
             expected);
    QCOMPARE(contextOf(fixture.platform.windows.value(QStringLiteral("b")).state),
             expected);
}

void KWinHybridContextTest::memberOutputChangeMapsCompleteOuterFrame()
{
    Fixture fixture;
    const auto before = fixture.scene.committedLayout(QStringLiteral("group"));
    QVERIFY(before.has_value());
    const auto expectedFrame = KWinIntegration::mapHybridGroupFrameToArea(
        before->outerFrame,
        fixture.platform.outputAreas.value(QStringLiteral("left")),
        fixture.platform.outputAreas.value(QStringLiteral("right")));
    QVERIFY(expectedFrame.has_value());

    auto &sourceWindow = fixture.platform.windows[QStringLiteral("b")];
    sourceWindow.state.outputId = QStringLiteral("right");
    // Model KWin's immediate per-window sendToOutput geometry while retaining
    // the registry's committed target frame for transaction rollback.
    sourceWindow.state.geometry.translate(1920, 0);
    const auto result = fixture.scene.recontextualizeContainer(
        *fixture.repository.topology().container(QStringLiteral("group")),
        QStringLiteral("b"));

    QVERIFY2(result.succeeded, qPrintable(result.message));
    QCOMPARE(fixture.scene.committedLayout(QStringLiteral("group"))->outerFrame,
             *expectedFrame);
    for (const auto &id : {QStringLiteral("a"), QStringLiteral("b")}) {
        QCOMPARE(fixture.platform.windows.value(id).state.outputId,
                 QStringLiteral("right"));
        QVERIFY(expectedFrame->contains(
            fixture.platform.windows.value(id).state.geometry.toAlignedRect()));
    }
}

void KWinHybridContextTest::failedFinalizationRestoresUniformContextAndLayout()
{
    Fixture fixture;
    const auto beforeLayout = fixture.scene.committedLayout(QStringLiteral("group"));
    const auto beforeA = fixture.platform.windows.value(QStringLiteral("a")).state;
    const auto beforeB = fixture.platform.windows.value(QStringLiteral("b")).state;
    fixture.platform.windows[QStringLiteral("b")].state.keepBelow = true;
    fixture.platform.failFinalize = true;

    const auto result = fixture.scene.recontextualizeContainer(
        *fixture.repository.topology().container(QStringLiteral("group")),
        QStringLiteral("b"));

    QVERIFY(!result.succeeded);
    QCOMPARE(fixture.scene.committedLayout(QStringLiteral("group")), beforeLayout);
    QCOMPARE(fixture.platform.windows.value(QStringLiteral("a")).state, beforeA);
    QCOMPARE(fixture.platform.windows.value(QStringLiteral("b")).state, beforeB);
}

void KWinHybridContextTest::planningFailureRestoresAlreadyChangedSource()
{
    Fixture fixture;
    const auto baseline = contextOf(
        fixture.platform.windows.value(QStringLiteral("a")).state);
    fixture.platform.windows[QStringLiteral("b")].state.outputId =
        QStringLiteral("missing-output");

    const auto result = fixture.scene.recontextualizeContainer(
        *fixture.repository.topology().container(QStringLiteral("group")),
        QStringLiteral("b"));

    QVERIFY(!result.succeeded);
    QCOMPARE(contextOf(fixture.platform.windows.value(QStringLiteral("a")).state),
             baseline);
    QCOMPARE(contextOf(fixture.platform.windows.value(QStringLiteral("b")).state),
             baseline);
}

void KWinHybridContextTest::earlierApplyFailureStillRestoresLaterSource()
{
    Fixture fixture;
    const auto baseline = contextOf(
        fixture.platform.windows.value(QStringLiteral("a")).state);
    fixture.platform.windows[QStringLiteral("b")].state.keepAbove = true;
    fixture.platform.failApplyOnce.insert(QStringLiteral("a"));

    const auto result = fixture.scene.recontextualizeContainer(
        *fixture.repository.topology().container(QStringLiteral("group")),
        QStringLiteral("b"));

    QVERIFY(!result.succeeded);
    QCOMPARE(contextOf(fixture.platform.windows.value(QStringLiteral("a")).state),
             baseline);
    QCOMPARE(contextOf(fixture.platform.windows.value(QStringLiteral("b")).state),
             baseline);
}

void KWinHybridContextTest::failedAdoptionAndReleaseRemainQuarantinedAfterReconciliation()
{
    Fixture fixture;
    KWinIntegration::HybridGroupContextQuarantine quarantine;
    fixture.platform.windows[QStringLiteral("b")].state.outputId =
        QStringLiteral("missing-output");
    fixture.platform.failFinalize = true;

    const auto recovery = KWinIntegration::recoverHybridGroupContext(
        QStringLiteral("group"),
        [&fixture](QString *error) {
            const auto adopted = fixture.scene.recontextualizeContainer(
                *fixture.repository.topology().container(QStringLiteral("group")),
                QStringLiteral("b"));
            if (error) {
                *error = adopted.message;
            }
            return adopted.succeeded;
        },
        [&fixture](QString *error) {
            const auto released = fixture.coordinator.execute(
                Hybrid::ReleaseContainer{QStringLiteral("group")});
            if (error) {
                *error = released.message;
            }
            return released.committed();
        },
        [&quarantine](const QString &containerId, bool coherent) {
            if (coherent) {
                quarantine.markCoherent(containerId);
            } else {
                quarantine.quarantine(containerId);
            }
        });

    QCOMPARE(recovery.status,
             KWinIntegration::HybridGroupContextRecoveryStatus::Quarantined);
    QVERIFY(!recovery.adoptionError.isEmpty());
    QVERIFY(!recovery.releaseError.isEmpty());
    QVERIFY(fixture.repository.topology().container(QStringLiteral("group")));
    QVERIFY(quarantine.contains(QStringLiteral("group")));

    quarantine.reconcilePublishedContainers(
        fixture.repository.topology().containerIds());
    QVERIFY(quarantine.contains(QStringLiteral("group")));

    fixture.platform.failFinalize = false;
    const auto released = fixture.coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("group")});
    QVERIFY2(released.committed(), qPrintable(released.message));
    quarantine.reconcilePublishedContainers(
        fixture.repository.topology().containerIds());
    QVERIFY(!quarantine.contains(QStringLiteral("group")));
}

void KWinHybridContextTest::releaseAfterContextChangeRestoresIndependentStates()
{
    Fixture fixture;
    fixture.platform.windows[QStringLiteral("b")].state.keepAbove = true;
    QVERIFY(fixture.scene.recontextualizeContainer(
                *fixture.repository.topology().container(QStringLiteral("group")),
                QStringLiteral("b"))
                .succeeded);

    const auto released = fixture.coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("group")});

    QVERIFY2(released.committed(), qPrintable(released.message));
    for (const auto &id : {QStringLiteral("a"), QStringLiteral("b")}) {
        QCOMPARE(fixture.platform.windows.value(id).state,
                 fixture.originals.value(id));
        QVERIFY(fixture.platform.owners.value(id).isEmpty());
    }
}

QTEST_GUILESS_MAIN(KWinHybridContextTest)

#include "tst_kwinhybridcontext.moc"
