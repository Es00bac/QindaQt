// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridscene_testfixture.h"

#include "qindaqt/hybrid/topologycoordinator.h"

#include <QTest>

using namespace QindaQt;
using namespace QindaQt::Compositor;

namespace {

Hybrid::DockIndependentWindows dockCommand(const QString &containerId,
                                           const QString &pageId,
                                           const QString &firstWindowId,
                                           const QString &secondWindowId)
{
    return {
        .containerId = containerId,
        .pageId = pageId,
        .firstWindowId = firstWindowId,
        .firstLeafNodeId = pageId + QStringLiteral("-first"),
        .secondWindowId = secondWindowId,
        .secondLeafNodeId = pageId + QStringLiteral("-second"),
        .splitNodeId = pageId + QStringLiteral("-split"),
        .orientation = Core::SplitOrientation::Horizontal,
        .ratio = 0.5,
        .secondPosition = Core::InsertPosition::Second,
    };
}

Hybrid::WindowTopology independentTopology(const QStringList &windowIds)
{
    QString error;
    const auto topology = Hybrid::WindowTopology::create(windowIds, {}, 0, &error);
    Q_ASSERT_X(topology.has_value(), "hybrid page scene fixture", qPrintable(error));
    return *topology;
}

void addWindow(Test::FakeHybridScenePlatform &platform,
               QHash<QString, HybridConstraints::WindowRestoreState> &original,
               const QString &windowId,
               int x,
               bool focused = false)
{
    const QRectF frame(x, 40, 280, 220);
    auto state = Test::richState(frame, QStringLiteral("output-a"), focused);
    original.insert(windowId, state);
    platform.addWindow(windowId, Test::fakeWindow(state, frame));
}

} // namespace

class KWinHybridPagesTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void moveAndDetachSplitPagePreservesTreeAndRestoreState();
    void detachLeafPageNormalizesToIndependentWindows();
    void regroupSplitPageWithIndependentRestoresExactly();
    void memberToPageKeepsOwnershipAndLeafIdentity();
};

void KWinHybridPagesTest::moveAndDetachSplitPagePreservesTreeAndRestoreState()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> original;
    const QStringList ids{QStringLiteral("a"), QStringLiteral("b"),
                          QStringLiteral("c"), QStringLiteral("d"),
                          QStringLiteral("e"), QStringLiteral("outside")};
    for (qsizetype index = 0; index < ids.size(); ++index) {
        addWindow(platform, original, ids[index], static_cast<int>(index) * 320,
                  ids[index] == QStringLiteral("outside"));
    }

    Hybrid::TopologyRepository repository(independentTopology(ids));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    QVERIFY(coordinator.execute(dockCommand(QStringLiteral("source"),
                                             QStringLiteral("page-ab"),
                                             QStringLiteral("a"),
                                             QStringLiteral("b")))
                .committed());
    QVERIFY(coordinator.execute(Hybrid::InsertIndependentWindow{
        .targetContainerId = QStringLiteral("source"),
        .windowId = QStringLiteral("c"),
        .leafNodeId = QStringLiteral("leaf-c"),
        .destination = Hybrid::MoveAsPage{QStringLiteral("page-c"), 1},
    }).committed());
    QVERIFY(coordinator.execute(dockCommand(QStringLiteral("target"),
                                             QStringLiteral("page-de"),
                                             QStringLiteral("d"),
                                             QStringLiteral("e")))
                .committed());
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));

    const int finalizeCalls = platform.finalizeCalls;
    const auto moved = coordinator.execute(Hybrid::MovePage{
        .sourceContainerId = QStringLiteral("source"),
        .targetContainerId = QStringLiteral("target"),
        .pageId = QStringLiteral("page-ab"),
        .destinationPageIndex = 0,
    });
    QVERIFY2(moved.committed(), qPrintable(moved.message));
    QCOMPARE(platform.finalizeCalls, finalizeCalls + 1);
    QVERIFY(!repository.topology().container(QStringLiteral("source")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("c")));
    QCOMPARE(platform.windows.value(QStringLiteral("c")).state,
             original.value(QStringLiteral("c")));

    const auto *target = repository.topology().container(QStringLiteral("target"));
    QVERIFY(target);
    QCOMPARE(target->pages().constFirst().id(), QStringLiteral("page-ab"));
    QCOMPARE(target->pages().constFirst().root().id(), QStringLiteral("page-ab-split"));
    QCOMPARE(target->pages().constFirst().root().firstChild()->id(),
             QStringLiteral("page-ab-first"));
    QCOMPARE(target->pages().constFirst().root().secondChild()->id(),
             QStringLiteral("page-ab-second"));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("a")),
             std::optional<QString>(QStringLiteral("target")));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("b")),
             std::optional<QString>(QStringLiteral("target")));

    // AGENT-CONTRACT: A tab detaches its complete page. A split page therefore
    // becomes one movable container and retains every structural identifier.
    const auto detached = coordinator.execute(Hybrid::DetachPage{
        .sourceContainerId = QStringLiteral("target"),
        .pageId = QStringLiteral("page-ab"),
        .newContainerId = QStringLiteral("detached"),
    });
    QVERIFY2(detached.committed(), qPrintable(detached.message));
    const auto *detachedContainer =
        repository.topology().container(QStringLiteral("detached"));
    QVERIFY(detachedContainer);
    QCOMPARE(detachedContainer->pages().constFirst().root().id(),
             QStringLiteral("page-ab-split"));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("a")),
             std::optional<QString>(QStringLiteral("detached")));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("b")),
             std::optional<QString>(QStringLiteral("detached")));
    QVERIFY(scene.committedLayout(QStringLiteral("target")).has_value());
    QVERIFY(scene.committedLayout(QStringLiteral("detached")).has_value());
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));

    QVERIFY(coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("detached")}).committed());
    QVERIFY(coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("target")}).committed());
    for (const auto &id : ids) {
        QCOMPARE(platform.windows.value(id).state, original.value(id));
        QVERIFY(platform.owners.value(id).isEmpty());
    }
}

void KWinHybridPagesTest::detachLeafPageNormalizesToIndependentWindows()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> original;
    addWindow(platform, original, QStringLiteral("a"), 0);
    addWindow(platform, original, QStringLiteral("b"), 320);
    addWindow(platform, original, QStringLiteral("outside"), 640, true);
    Hybrid::TopologyRepository repository(independentTopology(
        {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("outside")}));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    QVERIFY(coordinator.execute(Hybrid::GroupIndependentWindowsAsPages{
        .containerId = QStringLiteral("tabs"),
        .firstWindowId = QStringLiteral("a"),
        .firstPageId = QStringLiteral("page-a"),
        .firstLeafNodeId = QStringLiteral("leaf-a"),
        .secondWindowId = QStringLiteral("b"),
        .secondPageId = QStringLiteral("page-b"),
        .secondLeafNodeId = QStringLiteral("leaf-b"),
    }).committed());

    const auto detached = coordinator.execute(Hybrid::DetachPage{
        .sourceContainerId = QStringLiteral("tabs"),
        .pageId = QStringLiteral("page-b"),
        .newContainerId = QStringLiteral("unused-for-leaf"),
    });
    QVERIFY2(detached.committed(), qPrintable(detached.message));
    QVERIFY(repository.topology().containerIds().isEmpty());
    QVERIFY(repository.topology().isIndependent(QStringLiteral("a")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("b")));
    QCOMPARE(platform.windows.value(QStringLiteral("a")).state,
             original.value(QStringLiteral("a")));
    QCOMPARE(platform.windows.value(QStringLiteral("b")).state,
             original.value(QStringLiteral("b")));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));
}

void KWinHybridPagesTest::regroupSplitPageWithIndependentRestoresExactly()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> original;
    const QStringList ids{QStringLiteral("a"), QStringLiteral("b"),
                          QStringLiteral("c"), QStringLiteral("d"),
                          QStringLiteral("outside")};
    for (qsizetype index = 0; index < ids.size(); ++index) {
        addWindow(platform, original, ids[index], static_cast<int>(index) * 320,
                  ids[index] == QStringLiteral("outside"));
    }
    Hybrid::TopologyRepository repository(independentTopology(ids));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    QVERIFY(coordinator.execute(dockCommand(QStringLiteral("source"),
                                             QStringLiteral("page-ab"),
                                             QStringLiteral("a"),
                                             QStringLiteral("b")))
                .committed());
    QVERIFY(coordinator.execute(Hybrid::InsertIndependentWindow{
        .targetContainerId = QStringLiteral("source"),
        .windowId = QStringLiteral("c"),
        .leafNodeId = QStringLiteral("leaf-c"),
        .destination = Hybrid::MoveAsPage{QStringLiteral("page-c"), 1},
    }).committed());
    const int finalizeCalls = platform.finalizeCalls;

    const auto regrouped = coordinator.execute(Hybrid::RegroupPageWithIndependent{
        .sourceContainerId = QStringLiteral("source"),
        .pageId = QStringLiteral("page-ab"),
        .independentWindowId = QStringLiteral("d"),
        .newContainerId = QStringLiteral("regrouped"),
        .independentPageId = QStringLiteral("page-d"),
        .independentLeafNodeId = QStringLiteral("leaf-d"),
    });
    QVERIFY2(regrouped.committed(), qPrintable(regrouped.message));
    QCOMPARE(platform.finalizeCalls, finalizeCalls + 1);
    QVERIFY(repository.topology().isIndependent(QStringLiteral("c")));
    const auto *group = repository.topology().container(QStringLiteral("regrouped"));
    QVERIFY(group);
    QCOMPARE(group->pages()[1].root().id(), QStringLiteral("page-ab-split"));
    QCOMPARE(group->pages()[1].root().firstChild()->id(),
             QStringLiteral("page-ab-first"));
    QCOMPARE(group->pages()[1].root().secondChild()->id(),
             QStringLiteral("page-ab-second"));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));

    QVERIFY(coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("regrouped")}).committed());
    for (const auto &id : ids) {
        QCOMPARE(platform.windows.value(id).state, original.value(id));
        QVERIFY(platform.owners.value(id).isEmpty());
    }
}

void KWinHybridPagesTest::memberToPageKeepsOwnershipAndLeafIdentity()
{
    Test::FakeHybridScenePlatform platform;
    QHash<QString, HybridConstraints::WindowRestoreState> original;
    const QStringList ids{QStringLiteral("a"), QStringLiteral("b"),
                          QStringLiteral("c"), QStringLiteral("outside")};
    for (qsizetype index = 0; index < ids.size(); ++index) {
        addWindow(platform, original, ids[index], static_cast<int>(index) * 320,
                  ids[index] == QStringLiteral("outside"));
    }
    Hybrid::TopologyRepository repository(independentTopology(ids));
    KWinIntegration::KWinHybridSceneFactory scene(platform);
    Hybrid::TopologyCoordinator coordinator(repository, scene);
    QVERIFY(coordinator.execute(dockCommand(QStringLiteral("group"),
                                             QStringLiteral("page-ab"),
                                             QStringLiteral("a"),
                                             QStringLiteral("b")))
                .committed());
    QVERIFY(coordinator.execute(Hybrid::InsertIndependentWindow{
        .targetContainerId = QStringLiteral("group"),
        .windowId = QStringLiteral("c"),
        .leafNodeId = QStringLiteral("leaf-c"),
        .destination = Hybrid::MoveAsPage{QStringLiteral("page-c"), 1},
    }).committed());
    const int finalizeCalls = platform.finalizeCalls;

    const auto moved = coordinator.execute(Hybrid::MoveMemberToPage{
        .containerId = QStringLiteral("group"),
        .windowId = QStringLiteral("a"),
        .newPageId = QStringLiteral("page-a"),
        .targetPageId = QStringLiteral("page-c"),
    });
    QVERIFY2(moved.committed(), qPrintable(moved.message));
    QCOMPARE(platform.finalizeCalls, finalizeCalls + 1);
    const auto *group = repository.topology().container(QStringLiteral("group"));
    QVERIFY(group);
    QCOMPARE(group->pages()[0].root().windowId(), QStringLiteral("b"));
    QCOMPARE(group->pages()[2].root().windowId(), QStringLiteral("a"));
    QCOMPARE(group->pages()[2].root().id(), QStringLiteral("page-ab-first"));
    QCOMPARE(platform.owners.value(QStringLiteral("a")), QStringLiteral("group"));
    QCOMPARE(platform.activeWindowId(), QStringLiteral("outside"));

    QVERIFY(coordinator.execute(
        Hybrid::ReleaseContainer{QStringLiteral("group")}).committed());
    for (const auto &id : ids) {
        QCOMPARE(platform.windows.value(id).state, original.value(id));
        QVERIFY(platform.owners.value(id).isEmpty());
    }
}

QTEST_GUILESS_MAIN(KWinHybridPagesTest)

#include "tst_kwinhybridpages.moc"
