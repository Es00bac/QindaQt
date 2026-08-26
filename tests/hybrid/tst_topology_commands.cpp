// SPDX-License-Identifier: GPL-3.0-or-later
#include "testfixtures.h"

#include <QtTest>

using namespace QindaQt::Hybrid;
using namespace QindaQt::Hybrid::Test;
namespace Core = QindaQt::Core;

class TopologyCommandsTest final : public QObject
{
    Q_OBJECT

private slots:
    void addsAndForgetsIndependentWindows();
    void forgetsGroupedMembersAndNormalizes();
    void docksReordersAndNormalizesDetach();
    void movesAcrossContainersAndMergesPages();
    void movesWholePageAcrossContainers();
    void detachesWholePagesWithoutDiscardingLayout();
    void movesMemberIntoSplitPreservingLeafId();
    void activatesPagesAndResizesSplitsAtomically();
    void rejectsDuplicateOwnershipAndMergeCollisions();
};

void TopologyCommandsTest::addsAndForgetsIndependentWindows()
{
    TopologyRepository repository;
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto addedB = coordinator.execute(
        AddIndependentWindow{QStringLiteral("window-b")});
    QVERIFY2(addedB.committed(), qPrintable(addedB.message));
    const auto addedA = coordinator.execute(
        AddIndependentWindow{QStringLiteral("window-a")});
    QVERIFY2(addedA.committed(), qPrintable(addedA.message));
    QCOMPARE(addedA.revision, quint64{2});
    QCOMPARE(repository.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-a"), QStringLiteral("window-b")}));

    const auto duplicate = coordinator.execute(
        AddIndependentWindow{QStringLiteral("window-a")});
    QCOMPARE(duplicate.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{2});

    const auto forgotten = coordinator.execute(ForgetWindow{QStringLiteral("window-a")});
    QVERIFY2(forgotten.committed(), qPrintable(forgotten.message));
    QCOMPARE(forgotten.revision, quint64{3});
    QCOMPARE(repository.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-b")}));

    const auto unknown = coordinator.execute(ForgetWindow{QStringLiteral("missing")});
    QCOMPARE(unknown.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{3});
}

void TopologyCommandsTest::forgetsGroupedMembersAndNormalizes()
{
    auto threeMembers = splitContainer(QStringLiteral("three"),
                                       QStringLiteral("three"),
                                       QStringLiteral("window-a"),
                                       QStringLiteral("window-b"));
    QString error;
    QVERIFY(threeMembers.splitWindow(
        {.targetWindowId = QStringLiteral("window-b"),
         .newWindowId = QStringLiteral("window-c"),
         .newLeafNodeId = QStringLiteral("three-leaf-c"),
         .splitNodeId = QStringLiteral("three-inner"),
         .orientation = Core::SplitOrientation::Vertical,
         .ratio = 0.5,
         .position = Core::InsertPosition::Second},
        &error));
    auto pair = splitContainer(QStringLiteral("pair"),
                               QStringLiteral("pair"),
                               QStringLiteral("window-d"),
                               QStringLiteral("window-e"));
    TopologyRepository repository(topology({}, {threeMembers, pair}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto pruned = coordinator.execute(ForgetWindow{QStringLiteral("window-c")});
    QVERIFY2(pruned.committed(), qPrintable(pruned.message));
    QCOMPARE(repository.topology().windowIds(QStringLiteral("three")),
             QStringList({QStringLiteral("window-a"), QStringLiteral("window-b")}));
    QVERIFY(!repository.topology().isIndependent(QStringLiteral("window-c")));
    QVERIFY(!repository.topology().ownerOf(QStringLiteral("window-c")));

    const auto normalized = coordinator.execute(ForgetWindow{QStringLiteral("window-d")});
    QVERIFY2(normalized.committed(), qPrintable(normalized.message));
    QVERIFY(!repository.topology().container(QStringLiteral("pair")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-e")));
    QVERIFY(!repository.topology().isIndependent(QStringLiteral("window-d")));
    QVERIFY(!repository.topology().ownerOf(QStringLiteral("window-d")));
    QVERIFY(repository.topology().validate().valid);

    const auto duplicateOwned = coordinator.execute(
        AddIndependentWindow{QStringLiteral("window-a")});
    QCOMPARE(duplicateOwned.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{2});
}

void TopologyCommandsTest::docksReordersAndNormalizesDetach()
{
    TopologyRepository repository(topology({QStringLiteral("window-b"),
                                            QStringLiteral("window-a")}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto dock = coordinator.execute(
        dockCommand(QStringLiteral("container"),
                    QStringLiteral("ab"),
                    QStringLiteral("window-a"),
                    QStringLiteral("window-b")));
    QVERIFY2(dock.committed(), qPrintable(dock.message));
    QCOMPARE(dock.previousRevision, quint64{0});
    QCOMPARE(dock.revision, quint64{1});
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("window-a")),
             std::optional<QString>(QStringLiteral("container")));
    QVERIFY(repository.topology().independentWindowIds().isEmpty());

    const auto reorder = coordinator.execute(ReorderMembers{
        .containerId = QStringLiteral("container"),
        .firstWindowId = QStringLiteral("window-a"),
        .secondWindowId = QStringLiteral("window-b"),
    });
    QVERIFY(reorder.committed());
    const auto *container = repository.topology().container(QStringLiteral("container"));
    QVERIFY(container);
    QCOMPARE(container->findNode(QStringLiteral("ab-leaf-a"))->windowId(),
             QStringLiteral("window-b"));
    QCOMPARE(container->findNode(QStringLiteral("ab-leaf-b"))->windowId(),
             QStringLiteral("window-a"));

    const auto detach = coordinator.execute(DetachMember{
        .containerId = QStringLiteral("container"),
        .windowId = QStringLiteral("window-a"),
    });
    QVERIFY2(detach.committed(), qPrintable(detach.message));
    QCOMPARE(detach.revision, quint64{3});
    QVERIFY(!repository.topology().container(QStringLiteral("container")));
    QCOMPARE(repository.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-a"), QStringLiteral("window-b")}));
    QVERIFY(repository.topology().validate().valid);
}

void TopologyCommandsTest::movesAcrossContainersAndMergesPages()
{
    QVector<Core::WindowContainer> containers;
    containers.append(splitContainer(QStringLiteral("one"),
                                     QStringLiteral("one"),
                                     QStringLiteral("window-a"),
                                     QStringLiteral("window-b")));
    containers.append(splitContainer(QStringLiteral("two"),
                                     QStringLiteral("two"),
                                     QStringLiteral("window-c"),
                                     QStringLiteral("window-d")));
    containers.append(splitContainer(QStringLiteral("three"),
                                     QStringLiteral("three"),
                                     QStringLiteral("window-e"),
                                     QStringLiteral("window-f")));
    TopologyRepository repository(topology({}, std::move(containers), 10));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto moved = coordinator.execute(MoveMember{
        .sourceContainerId = QStringLiteral("two"),
        .targetContainerId = QStringLiteral("one"),
        .windowId = QStringLiteral("window-c"),
        .destination = MoveAsPage{QStringLiteral("moved-page"), 0},
    });
    QVERIFY2(moved.committed(), qPrintable(moved.message));
    QCOMPARE(repository.topology().container(QStringLiteral("one"))->pages()[0].id(),
             QStringLiteral("moved-page"));
    QVERIFY(!repository.topology().container(QStringLiteral("two")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-d")));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("window-c")),
             std::optional<QString>(QStringLiteral("one")));

    const auto reordered = coordinator.execute(ReorderPage{
        .containerId = QStringLiteral("one"),
        .pageId = QStringLiteral("moved-page"),
        .destinationPageIndex = 1,
    });
    QVERIFY(reordered.committed());
    QCOMPARE(repository.topology().container(QStringLiteral("one"))->pages()[1].id(),
             QStringLiteral("moved-page"));

    const auto merged = coordinator.execute(MergeContainers{
        .targetContainerId = QStringLiteral("one"),
        .sourceContainerId = QStringLiteral("three"),
        .destinationPageIndex = 1,
    });
    QVERIFY2(merged.committed(), qPrintable(merged.message));
    const auto *mergedContainer = repository.topology().container(QStringLiteral("one"));
    QVERIFY(mergedContainer);
    QCOMPARE(mergedContainer->pages().size(), qsizetype{3});
    QCOMPARE(mergedContainer->pages()[0].id(), QStringLiteral("one-page"));
    QCOMPARE(mergedContainer->pages()[1].id(), QStringLiteral("three-page"));
    QCOMPARE(mergedContainer->pages()[2].id(), QStringLiteral("moved-page"));
    QVERIFY(!repository.topology().container(QStringLiteral("three")));

    const auto released = coordinator.execute(
        ReleaseContainer{QStringLiteral("one")});
    QVERIFY(released.committed());
    QCOMPARE(released.revision, quint64{14});
    QVERIFY(repository.topology().containerIds().isEmpty());
    QCOMPARE(repository.topology().independentWindowIds(),
             QStringList({QStringLiteral("window-a"),
                          QStringLiteral("window-b"),
                          QStringLiteral("window-c"),
                          QStringLiteral("window-d"),
                          QStringLiteral("window-e"),
                          QStringLiteral("window-f")}));
}

void TopologyCommandsTest::movesWholePageAcrossContainers()
{
    auto source = splitContainer(QStringLiteral("source"),
                                 QStringLiteral("source"),
                                 QStringLiteral("window-a"),
                                 QStringLiteral("window-b"));
    QString error;
    QVERIFY(source.addPage(QStringLiteral("source-spare-page"),
                           QStringLiteral("source-spare-leaf"),
                           QStringLiteral("window-c"), &error));
    auto target = splitContainer(QStringLiteral("target"),
                                 QStringLiteral("target"),
                                 QStringLiteral("window-d"),
                                 QStringLiteral("window-e"));
    TopologyRepository repository(topology({}, {source, target}, 4));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto moved = coordinator.execute(MovePage{
        .sourceContainerId = QStringLiteral("source"),
        .targetContainerId = QStringLiteral("target"),
        .pageId = QStringLiteral("source-page"),
        .destinationPageIndex = 0,
    });
    QVERIFY2(moved.committed(), qPrintable(moved.message));
    QCOMPARE(moved.kind, TopologyCommandKind::MovePage);
    QCOMPARE(moved.revision, quint64{5});
    QVERIFY(!repository.topology().container(QStringLiteral("source")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-c")));
    const auto *targetAfter = repository.topology().container(QStringLiteral("target"));
    QVERIFY(targetAfter);
    QCOMPARE(targetAfter->pages().size(), qsizetype{2});
    QCOMPARE(targetAfter->pages().constFirst().id(), QStringLiteral("source-page"));
    QCOMPARE(targetAfter->pages().constFirst().root().id(),
             QStringLiteral("source-split"));
    QCOMPARE(targetAfter->pages().constFirst().root().firstChild()->id(),
             QStringLiteral("source-leaf-a"));
    QCOMPARE(targetAfter->pages().constFirst().root().secondChild()->id(),
             QStringLiteral("source-leaf-b"));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("window-a")),
             std::optional<QString>(QStringLiteral("target")));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("window-b")),
             std::optional<QString>(QStringLiteral("target")));
    QVERIFY(repository.topology().validate().valid);

    const auto invalid = coordinator.execute(MovePage{
        .sourceContainerId = QStringLiteral("target"),
        .targetContainerId = QStringLiteral("target"),
        .pageId = QStringLiteral("source-page"),
        .destinationPageIndex = 0,
    });
    QCOMPARE(invalid.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{5});
}

void TopologyCommandsTest::detachesWholePagesWithoutDiscardingLayout()
{
    {
        auto container = splitContainer(QStringLiteral("source"),
                                        QStringLiteral("tiled"),
                                        QStringLiteral("window-a"),
                                        QStringLiteral("window-b"));
        QString error;
        QVERIFY(container.addPage(QStringLiteral("spare-page"),
                                  QStringLiteral("spare-leaf"),
                                  QStringLiteral("window-c"), &error));
        TopologyRepository repository(topology({}, {container}));
        AlwaysReadyFactory scene;
        TopologyCoordinator coordinator(repository, scene);

        const auto detached = coordinator.execute(DetachPage{
            .sourceContainerId = QStringLiteral("source"),
            .pageId = QStringLiteral("tiled-page"),
            .newContainerId = QStringLiteral("separated"),
        });
        QVERIFY2(detached.committed(), qPrintable(detached.message));
        QCOMPARE(detached.kind, TopologyCommandKind::DetachPage);
        QVERIFY(!repository.topology().container(QStringLiteral("source")));
        QVERIFY(repository.topology().isIndependent(QStringLiteral("window-c")));
        const auto *separated = repository.topology().container(
            QStringLiteral("separated"));
        QVERIFY(separated);
        QCOMPARE(separated->pages().constFirst().id(), QStringLiteral("tiled-page"));
        QCOMPARE(separated->pages().constFirst().root().id(),
                 QStringLiteral("tiled-split"));
    }

    auto container = splitContainer(QStringLiteral("source"),
                                    QStringLiteral("tiled"),
                                    QStringLiteral("window-a"),
                                    QStringLiteral("window-b"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("single-page"),
                              QStringLiteral("single-leaf"),
                              QStringLiteral("window-c"), &error));
    TopologyRepository repository(topology({}, {container}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);
    const auto detachedSingle = coordinator.execute(DetachPage{
        .sourceContainerId = QStringLiteral("source"),
        .pageId = QStringLiteral("single-page"),
        .newContainerId = QStringLiteral("unused"),
    });
    QVERIFY2(detachedSingle.committed(), qPrintable(detachedSingle.message));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-c")));
    QVERIFY(repository.topology().container(QStringLiteral("source")));
    QVERIFY(!repository.topology().container(QStringLiteral("unused")));

    const auto onlyPage = coordinator.execute(DetachPage{
        .sourceContainerId = QStringLiteral("source"),
        .pageId = QStringLiteral("tiled-page"),
        .newContainerId = QStringLiteral("separated"),
    });
    QCOMPARE(onlyPage.error, TopologyCommandError::InvalidCommand);
}

void TopologyCommandsTest::movesMemberIntoSplitPreservingLeafId()
{
    auto source = splitContainer(QStringLiteral("source"),
                                 QStringLiteral("source"),
                                 QStringLiteral("window-a"),
                                 QStringLiteral("window-b"));
    QString error;
    QVERIFY(source.splitWindow(
        {.targetWindowId = QStringLiteral("window-b"),
         .newWindowId = QStringLiteral("window-c"),
         .newLeafNodeId = QStringLiteral("source-leaf-c"),
         .splitNodeId = QStringLiteral("source-inner"),
         .orientation = Core::SplitOrientation::Vertical,
         .ratio = 0.6,
         .position = Core::InsertPosition::Second},
        &error));
    auto target = splitContainer(QStringLiteral("target"),
                                 QStringLiteral("target"),
                                 QStringLiteral("window-d"),
                                 QStringLiteral("window-e"));
    TopologyRepository repository(topology({}, {source, target}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto moved = coordinator.execute(MoveMember{
        .sourceContainerId = QStringLiteral("source"),
        .targetContainerId = QStringLiteral("target"),
        .windowId = QStringLiteral("window-c"),
        .destination = MoveAsSplit{
            .targetWindowId = QStringLiteral("window-d"),
            .splitNodeId = QStringLiteral("target-new-split"),
            .orientation = Core::SplitOrientation::Vertical,
            .ratio = 0.4,
            .position = Core::InsertPosition::First,
        },
    });
    QVERIFY2(moved.committed(), qPrintable(moved.message));
    const auto *targetAfter = repository.topology().container(QStringLiteral("target"));
    QVERIFY(targetAfter);
    QCOMPARE(targetAfter->findWindow(QStringLiteral("window-c"))->id(),
             QStringLiteral("source-leaf-c"));
    QVERIFY(repository.topology().container(QStringLiteral("source")));
    QCOMPARE(repository.topology().windowIds(QStringLiteral("source")).size(), qsizetype{2});
    QVERIFY(repository.topology().validate().valid);
}

void TopologyCommandsTest::activatesPagesAndResizesSplitsAtomically()
{
    auto container = splitContainer(QStringLiteral("container"),
                                    QStringLiteral("first"),
                                    QStringLiteral("window-a"),
                                    QStringLiteral("window-b"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("second-page"),
                              QStringLiteral("second-leaf"),
                              QStringLiteral("window-c"),
                              &error));
    TopologyRepository repository(topology({}, {container}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto activated = coordinator.execute(ActivatePage{
        QStringLiteral("container"), QStringLiteral("second-page")});
    QVERIFY2(activated.committed(), qPrintable(activated.message));
    QCOMPARE(repository.topology().container(QStringLiteral("container"))->activePageId(),
             QStringLiteral("second-page"));
    QCOMPARE(activated.kind, TopologyCommandKind::ActivatePage);

    const auto resized = coordinator.execute(ResizeSplit{
        QStringLiteral("container"), QStringLiteral("first-split"), 0.7});
    QVERIFY2(resized.committed(), qPrintable(resized.message));
    QCOMPARE(repository.topology()
                 .container(QStringLiteral("container"))
                 ->findNode(QStringLiteral("first-split"))
                 ->ratio(),
             std::optional(0.7));
    QCOMPARE(resized.kind, TopologyCommandKind::ResizeSplit);
    QCOMPARE(repository.topology().revision(), quint64{2});

    const auto repeatedActivation = coordinator.execute(ActivatePage{
        QStringLiteral("container"), QStringLiteral("second-page")});
    const auto repeatedResize = coordinator.execute(ResizeSplit{
        QStringLiteral("container"), QStringLiteral("first-split"), 0.7});
    const auto invalidResize = coordinator.execute(ResizeSplit{
        QStringLiteral("container"), QStringLiteral("first-split"), 1.0});
    QCOMPARE(repeatedActivation.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repeatedResize.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(invalidResize.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{2});
}

void TopologyCommandsTest::rejectsDuplicateOwnershipAndMergeCollisions()
{
    auto container = splitContainer(QStringLiteral("container"),
                                    QStringLiteral("same"),
                                    QStringLiteral("window-a"),
                                    QStringLiteral("window-b"));
    QString error;
    QVERIFY(!WindowTopology::create({QStringLiteral("window-a")}, {container}, 0, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate topology ownership")));
    QVERIFY(!WindowTopology::create({QStringLiteral("window-z"),
                                     QStringLiteral("window-z")},
                                    {},
                                    0,
                                    &error));
    QVERIFY(error.contains(QStringLiteral("duplicate independent window")));

    Core::WindowContainer singleton(QStringLiteral("singleton"));
    QVERIFY(singleton.addPage(QStringLiteral("page"),
                              QStringLiteral("leaf"),
                              QStringLiteral("window-z"),
                              &error));
    QVERIFY(!WindowTopology::create({}, {singleton}, 0, &error));
    QVERIFY(error.contains(QStringLiteral("at least two members")));

    auto target = splitContainer(QStringLiteral("target"),
                                 QStringLiteral("same"),
                                 QStringLiteral("window-a"),
                                 QStringLiteral("window-b"));
    auto source = splitContainer(QStringLiteral("source"),
                                 QStringLiteral("same"),
                                 QStringLiteral("window-c"),
                                 QStringLiteral("window-d"));
    TopologyRepository repository(topology({}, {target, source}, 7));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);
    const auto beforeIds = repository.topology().containerIds();

    const auto rejected = coordinator.execute(MergeContainers{
        .targetContainerId = QStringLiteral("target"),
        .sourceContainerId = QStringLiteral("source"),
        .destinationPageIndex = 0,
    });
    QVERIFY(!rejected.committed());
    QCOMPARE(rejected.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(rejected.revision, quint64{7});
    QCOMPARE(repository.topology().containerIds(), beforeIds);
    QCOMPARE(repository.topology().windowIds(QStringLiteral("target")),
             QStringList({QStringLiteral("window-a"), QStringLiteral("window-b")}));

    const auto rejectedMove = coordinator.execute(MoveMember{
        .sourceContainerId = QStringLiteral("source"),
        .targetContainerId = QStringLiteral("target"),
        .windowId = QStringLiteral("window-c"),
        .destination = MoveAsSplit{
            .targetWindowId = QStringLiteral("window-a"),
            .splitNodeId = QStringLiteral("unique-new-split"),
        },
    });
    QVERIFY(!rejectedMove.committed());
    QCOMPARE(rejectedMove.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{7});
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("window-c")),
             std::optional<QString>(QStringLiteral("source")));
    QCOMPARE(repository.topology().windowIds(QStringLiteral("source")),
             QStringList({QStringLiteral("window-c"), QStringLiteral("window-d")}));
}

QTEST_APPLESS_MAIN(TopologyCommandsTest)

#include "tst_topology_commands.moc"
