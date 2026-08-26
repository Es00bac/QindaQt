// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene_testfixture.h"

#include "qindaqt/hybrid/topologycoordinator.h"

#include <QTest>

using namespace QindaQt;
using namespace QindaQt::Compositor;

namespace {

Hybrid::WindowTopology independentTopology(const QStringList &ids)
{
    QString error;
    const auto topology = Hybrid::WindowTopology::create(ids, {}, 0, &error);
    Q_ASSERT_X(topology.has_value(), "task identity fixture", qPrintable(error));
    return *topology;
}

Hybrid::GroupIndependentWindowsAsPages pages(const QString &containerId,
                                             const QString &first,
                                             const QString &second)
{
    return {
        .containerId = containerId,
        .firstWindowId = first,
        .firstPageId = QStringLiteral("page-") + first,
        .firstLeafNodeId = QStringLiteral("leaf-") + first,
        .secondWindowId = second,
        .secondPageId = QStringLiteral("page-") + second,
        .secondLeafNodeId = QStringLiteral("leaf-") + second,
    };
}

Hybrid::DockIndependentWindows split(const QString &containerId,
                                     const QString &first,
                                     const QString &second)
{
    return {
        .containerId = containerId,
        .pageId = QStringLiteral("page-") + containerId,
        .firstWindowId = first,
        .firstLeafNodeId = QStringLiteral("leaf-") + first,
        .secondWindowId = second,
        .secondLeafNodeId = QStringLiteral("leaf-") + second,
        .splitNodeId = QStringLiteral("split-") + containerId,
    };
}

HybridConstraints::WindowRestoreState independentState(
    int x, bool focused, bool skipTaskbar, bool skipSwitcher)
{
    auto state = Test::richState(QRectF(x, 40, 300, 220),
                                 QStringLiteral("output-a"), focused);
    state.skipTaskbar = skipTaskbar;
    state.skipSwitcher = skipSwitcher;
    return state;
}

void add(Test::FakeHybridScenePlatform &platform,
         QHash<QString, HybridConstraints::WindowRestoreState> *originals,
         const QString &id,
         int x,
         bool focused,
         bool skipTaskbar,
         bool skipSwitcher)
{
    const auto state = independentState(x, focused, skipTaskbar, skipSwitcher);
    originals->insert(id, state);
    platform.addWindow(id, Test::fakeWindow(state, state.geometry));
}

void verifyPrimary(const Test::FakeHybridScenePlatform &platform,
                   const QStringList &memberIds,
                   const QString &primaryId)
{
    qsizetype taskEntries = 0;
    qsizetype switcherEntries = 0;
    for (const auto &id : memberIds) {
        const auto &state = platform.windows.value(id).state;
        taskEntries += state.skipTaskbar ? 0 : 1;
        switcherEntries += state.skipSwitcher ? 0 : 1;
        if (id == primaryId) {
            QVERIFY(!state.skipTaskbar);
            QVERIFY(!state.skipSwitcher);
        } else {
            QVERIFY(state.skipTaskbar);
            QVERIFY(state.skipSwitcher);
        }
    }
    QCOMPARE(taskEntries, qsizetype(1));
    QCOMPARE(switcherEntries, qsizetype(1));
}

} // namespace

class KWinHybridTaskIdentityTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pageActivationAndReleaseRestoreIndependentBits();
    void failedActivationRollsBackGroupedIdentity();
    void primaryCloseNormalizesAndRestoresSurvivor();
    void pageMoveKeepsInactiveExcludedAndRestoresEveryBaseline();
};

void KWinHybridTaskIdentityTest::pageActivationAndReleaseRestoreIndependentBits()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> originals;
    add(platform, &originals, QStringLiteral("a"), 0, true, true, false);
    add(platform, &originals, QStringLiteral("b"), 400, false, false, true);
    originals[QStringLiteral("b")].minimized = true;
    platform.windows[QStringLiteral("b")].state.minimized = true;
    Hybrid::TopologyRepository repository(independentTopology({
        QStringLiteral("a"), QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    Hybrid::TopologyCoordinator coordinator(repository, scene);

    QVERIFY(coordinator.execute(pages(QStringLiteral("group"),
                                      QStringLiteral("a"),
                                      QStringLiteral("b"))).committed());
    verifyPrimary(platform, {QStringLiteral("a"), QStringLiteral("b")},
                  QStringLiteral("a"));
    QVERIFY(!platform.windows.value(QStringLiteral("a")).state.minimized);
    QVERIFY(platform.windows.value(QStringLiteral("b")).state.minimized);

    const auto activated = coordinator.execute(Hybrid::ActivatePage{
        QStringLiteral("group"), QStringLiteral("page-b")});
    QVERIFY2(activated.committed(), qPrintable(activated.message));
    verifyPrimary(platform, {QStringLiteral("a"), QStringLiteral("b")},
                  QStringLiteral("b"));
    QVERIFY(platform.windows.value(QStringLiteral("a")).state.minimized);
    QVERIFY(!platform.windows.value(QStringLiteral("b")).state.minimized);

    QVERIFY(coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("group")}).committed());
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state,
             originals.value(QStringLiteral("a")));
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state,
             originals.value(QStringLiteral("b")));
}

void KWinHybridTaskIdentityTest::failedActivationRollsBackGroupedIdentity()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> originals;
    add(platform, &originals, QStringLiteral("a"), 0, true, false, true);
    add(platform, &originals, QStringLiteral("b"), 400, false, true, false);
    Hybrid::TopologyRepository repository(independentTopology({
        QStringLiteral("a"), QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    QVERIFY(coordinator.execute(pages(QStringLiteral("group"),
                                      QStringLiteral("a"),
                                      QStringLiteral("b"))).committed());
    const auto beforeA = platform.windows.value(QStringLiteral("a")).state;
    const auto beforeB = platform.windows.value(QStringLiteral("b")).state;
    const auto beforeRevision = repository.topology().revision();

    platform.failFinalize = true;
    const auto failed = coordinator.execute(Hybrid::ActivatePage{
        QStringLiteral("group"), QStringLiteral("page-b")});
    QCOMPARE(failed.error, Hybrid::TopologyCommandError::SceneCommitFailed);
    QCOMPARE(repository.topology().revision(), beforeRevision);
    QCOMPARE(repository.topology().container(QStringLiteral("group"))->activePageId(),
             QStringLiteral("page-a"));
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state, beforeA);
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state, beforeB);
    verifyPrimary(platform, {QStringLiteral("a"), QStringLiteral("b")},
                  QStringLiteral("a"));

    platform.failFinalize = false;
    QVERIFY(coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("group")}).committed());
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state,
             originals.value(QStringLiteral("a")));
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state,
             originals.value(QStringLiteral("b")));
}

void KWinHybridTaskIdentityTest::primaryCloseNormalizesAndRestoresSurvivor()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> originals;
    add(platform, &originals, QStringLiteral("a"), 0, true, true, true);
    add(platform, &originals, QStringLiteral("b"), 400, false, false, true);
    Hybrid::TopologyRepository repository(independentTopology({
        QStringLiteral("a"), QStringLiteral("b")}));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    QVERIFY(coordinator.execute(split(QStringLiteral("group"),
                                      QStringLiteral("a"),
                                      QStringLiteral("b"))).committed());
    verifyPrimary(platform, {QStringLiteral("a"), QStringLiteral("b")},
                  QStringLiteral("a"));

    platform.removeWindow(QStringLiteral("a"));
    const auto closed = coordinator.execute(Hybrid::ForgetWindow{QStringLiteral("a")});
    QVERIFY2(closed.committed(), qPrintable(closed.message));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("b")));
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state,
             originals.value(QStringLiteral("b")));
    QVERIFY(platform.owners.value(QStringLiteral("b")).isEmpty());
}

void KWinHybridTaskIdentityTest::pageMoveKeepsInactiveExcludedAndRestoresEveryBaseline()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> originals;
    add(platform, &originals, QStringLiteral("a"), 0, true, true, false);
    add(platform, &originals, QStringLiteral("b"), 350, false, false, true);
    add(platform, &originals, QStringLiteral("c"), 700, false, true, true);
    add(platform, &originals, QStringLiteral("d"), 1050, false, false, false);
    Hybrid::TopologyRepository repository(independentTopology({
        QStringLiteral("a"), QStringLiteral("b"),
        QStringLiteral("c"), QStringLiteral("d")}));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    QVERIFY(coordinator.execute(pages(QStringLiteral("source"),
                                      QStringLiteral("a"),
                                      QStringLiteral("b"))).committed());
    QVERIFY(coordinator.execute(split(QStringLiteral("target"),
                                      QStringLiteral("c"),
                                      QStringLiteral("d"))).committed());

    const auto moved = coordinator.execute(Hybrid::MovePage{
        .sourceContainerId = QStringLiteral("source"),
        .targetContainerId = QStringLiteral("target"),
        .pageId = QStringLiteral("page-b"),
        .destinationPageIndex = 1,
    });
    QVERIFY2(moved.committed(), qPrintable(moved.message));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("a")));
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state,
             originals.value(QStringLiteral("a")));
    verifyPrimary(platform,
                  {QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")},
                  QStringLiteral("c"));
    QVERIFY(platform.windows.value(QStringLiteral("b")).state.minimized);

    QVERIFY(coordinator.execute(Hybrid::ActivatePage{
        QStringLiteral("target"), QStringLiteral("page-b")}).committed());
    verifyPrimary(platform,
                  {QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")},
                  QStringLiteral("b"));
    QVERIFY(platform.windows.value(QStringLiteral("c")).state.minimized);
    QVERIFY(platform.windows.value(QStringLiteral("d")).state.minimized);

    QVERIFY(coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("target")}).committed());
    for (const auto &id : {QStringLiteral("b"), QStringLiteral("c"),
                           QStringLiteral("d")}) {
        QCOMPARE(platform.windows.value(id).state, originals.value(id));
    }
}

QTEST_GUILESS_MAIN(KWinHybridTaskIdentityTest)
#include "tst_kwinhybridtaskidentity.moc"
