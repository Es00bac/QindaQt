// SPDX-License-Identifier: GPL-3.0-or-later
#include "testfixtures.h"

#include <QtTest>

using namespace QindaQt::Hybrid;
using namespace QindaQt::Hybrid::Test;
namespace Core = QindaQt::Core;

class TopologyPlacementTest final : public QObject
{
    Q_OBJECT

private slots:
    void insertsIndependentWindowAsSplitAndPage();
    void groupsIndependentWindowsAsPages();
    void regroupsMemberWithIndependentAndNormalizesSource();
    void movesIndividualMembersAcrossPages();
    void regroupsWholePageWithIndependentTarget();
    void reparentsMemberPreservingLeafAndRejectsNoOps();
};

void TopologyPlacementTest::insertsIndependentWindowAsSplitAndPage()
{
    auto target = splitContainer(QStringLiteral("target"),
                                 QStringLiteral("target"),
                                 QStringLiteral("window-a"),
                                 QStringLiteral("window-b"));
    TopologyRepository repository(topology({QStringLiteral("window-c"),
                                            QStringLiteral("window-d")},
                                           {target}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto split = coordinator.execute(InsertIndependentWindow{
        .targetContainerId = QStringLiteral("target"),
        .windowId = QStringLiteral("window-c"),
        .leafNodeId = QStringLiteral("inserted-leaf-c"),
        .destination = MoveAsSplit{
            .targetWindowId = QStringLiteral("window-a"),
            .splitNodeId = QStringLiteral("inserted-split"),
            .orientation = Core::SplitOrientation::Vertical,
            .ratio = 0.4,
            .position = Core::InsertPosition::First,
        },
    });
    QVERIFY2(split.committed(), qPrintable(split.message));
    QCOMPARE(split.kind, TopologyCommandKind::InsertIndependentWindow);
    const auto *container = repository.topology().container(QStringLiteral("target"));
    QVERIFY(container);
    QCOMPARE(container->findWindow(QStringLiteral("window-c"))->id(),
             QStringLiteral("inserted-leaf-c"));
    const auto *newSplit = container->findNode(QStringLiteral("inserted-split"));
    QVERIFY(newSplit);
    QCOMPARE(newSplit->firstChild()->windowId(), QStringLiteral("window-c"));

    const auto page = coordinator.execute(InsertIndependentWindow{
        .targetContainerId = QStringLiteral("target"),
        .windowId = QStringLiteral("window-d"),
        .leafNodeId = QStringLiteral("inserted-leaf-d"),
        .destination = MoveAsPage{QStringLiteral("inserted-page"), 0},
    });
    QVERIFY2(page.committed(), qPrintable(page.message));
    container = repository.topology().container(QStringLiteral("target"));
    QCOMPARE(container->pages().constFirst().id(), QStringLiteral("inserted-page"));
    QCOMPARE(container->pages().constFirst().root().id(),
             QStringLiteral("inserted-leaf-d"));
    QVERIFY(repository.topology().independentWindowIds().isEmpty());
}

void TopologyPlacementTest::groupsIndependentWindowsAsPages()
{
    TopologyRepository repository(topology({QStringLiteral("window-a"),
                                            QStringLiteral("window-b")}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);
    const GroupIndependentWindowsAsPages command{
        .containerId = QStringLiteral("group"),
        .firstWindowId = QStringLiteral("window-a"),
        .firstPageId = QStringLiteral("page-a"),
        .firstLeafNodeId = QStringLiteral("leaf-a"),
        .secondWindowId = QStringLiteral("window-b"),
        .secondPageId = QStringLiteral("page-b"),
        .secondLeafNodeId = QStringLiteral("leaf-b"),
    };

    const auto grouped = coordinator.execute(command);
    QVERIFY2(grouped.committed(), qPrintable(grouped.message));
    QCOMPARE(grouped.kind, TopologyCommandKind::GroupIndependentWindowsAsPages);
    const auto *container = repository.topology().container(QStringLiteral("group"));
    QVERIFY(container);
    QCOMPARE(container->pages().size(), qsizetype{2});
    QCOMPARE(container->pages()[0].root().windowId(), QStringLiteral("window-a"));
    QCOMPARE(container->pages()[1].root().windowId(), QStringLiteral("window-b"));
    QCOMPARE(container->activePageId(), QStringLiteral("page-a"));

    const auto duplicate = coordinator.execute(command);
    QCOMPARE(duplicate.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{1});
}

void TopologyPlacementTest::regroupsMemberWithIndependentAndNormalizesSource()
{
    auto source = splitContainer(QStringLiteral("source"),
                                 QStringLiteral("source"),
                                 QStringLiteral("window-a"),
                                 QStringLiteral("window-b"));
    TopologyRepository repository(topology({QStringLiteral("window-c")}, {source}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto rejected = coordinator.execute(RegroupMemberWithIndependent{
        .sourceContainerId = QStringLiteral("source"),
        .memberWindowId = QStringLiteral("window-a"),
        .independentWindowId = QStringLiteral("window-c"),
        .newContainerId = QStringLiteral("invalid-group"),
        .layout = RegroupAsPages{
            QStringLiteral("duplicate-page"),
            QStringLiteral("duplicate-page"),
            QStringLiteral("leaf-c"),
        },
    });
    QCOMPARE(rejected.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{0});
    QVERIFY(repository.topology().container(QStringLiteral("source")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-c")));

    const auto regrouped = coordinator.execute(RegroupMemberWithIndependent{
        .sourceContainerId = QStringLiteral("source"),
        .memberWindowId = QStringLiteral("window-a"),
        .independentWindowId = QStringLiteral("window-c"),
        .newContainerId = QStringLiteral("regrouped"),
        .layout = RegroupAsSplit{
            .pageId = QStringLiteral("regrouped-page"),
            .independentLeafNodeId = QStringLiteral("leaf-c"),
            .splitNodeId = QStringLiteral("regrouped-split"),
            .orientation = Core::SplitOrientation::Horizontal,
            .ratio = 0.5,
            .memberPosition = Core::InsertPosition::Second,
        },
    });
    QVERIFY2(regrouped.committed(), qPrintable(regrouped.message));
    QCOMPARE(regrouped.kind, TopologyCommandKind::RegroupMemberWithIndependent);
    QVERIFY(!repository.topology().container(QStringLiteral("source")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-b")));
    const auto *group = repository.topology().container(QStringLiteral("regrouped"));
    QVERIFY(group);
    QCOMPARE(group->findWindow(QStringLiteral("window-a"))->id(),
             QStringLiteral("source-leaf-a"));
    QCOMPARE(group->findNode(QStringLiteral("regrouped-split"))
                 ->secondChild()
                 ->windowId(),
             QStringLiteral("window-a"));
    QVERIFY(repository.topology().validate().valid);
}

void TopologyPlacementTest::movesIndividualMembersAcrossPages()
{
    auto container = splitContainer(QStringLiteral("container"),
                                    QStringLiteral("source"),
                                    QStringLiteral("window-a"),
                                    QStringLiteral("window-b"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("page-c"), QStringLiteral("leaf-c"),
                              QStringLiteral("window-c"), &error));
    TopologyRepository repository(topology({}, {container}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto extracted = coordinator.execute(MoveMemberToPage{
        .containerId = QStringLiteral("container"),
        .windowId = QStringLiteral("window-a"),
        .newPageId = QStringLiteral("page-a"),
        .targetPageId = QStringLiteral("page-c"),
    });
    QVERIFY2(extracted.committed(), qPrintable(extracted.message));
    QCOMPARE(extracted.kind, TopologyCommandKind::MoveMemberToPage);
    const auto *after = repository.topology().container(QStringLiteral("container"));
    QVERIFY(after);
    QCOMPARE(after->pages().size(), qsizetype{3});
    QCOMPARE(after->pages()[0].root().windowId(), QStringLiteral("window-b"));
    QCOMPARE(after->pages()[1].id(), QStringLiteral("page-c"));
    QCOMPARE(after->pages()[2].id(), QStringLiteral("page-a"));
    QCOMPARE(after->pages()[2].root().id(), QStringLiteral("source-leaf-a"));

    // Removing a single-member source page shifts the target index. The
    // command addresses the stable target page ID so the final placement is
    // still deterministic after that normalization.
    const auto movedSingle = coordinator.execute(MoveMemberToPage{
        .containerId = QStringLiteral("container"),
        .windowId = QStringLiteral("window-c"),
        .newPageId = QStringLiteral("page-c-moved"),
        .targetPageId = QStringLiteral("page-a"),
    });
    QVERIFY2(movedSingle.committed(), qPrintable(movedSingle.message));
    after = repository.topology().container(QStringLiteral("container"));
    QCOMPARE(after->pages().size(), qsizetype{3});
    QCOMPARE(after->pages()[1].id(), QStringLiteral("page-a"));
    QCOMPARE(after->pages()[2].id(), QStringLiteral("page-c-moved"));
    QCOMPARE(after->pages()[2].root().id(), QStringLiteral("leaf-c"));
    QVERIFY(repository.topology().validate().valid);
}

void TopologyPlacementTest::regroupsWholePageWithIndependentTarget()
{
    auto source = splitContainer(QStringLiteral("source"),
                                 QStringLiteral("source"),
                                 QStringLiteral("window-a"),
                                 QStringLiteral("window-b"));
    QString error;
    QVERIFY(source.addPage(QStringLiteral("spare-page"),
                           QStringLiteral("spare-leaf"),
                           QStringLiteral("window-c"), &error));
    TopologyRepository repository(topology({QStringLiteral("window-d")}, {source}));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto regrouped = coordinator.execute(RegroupPageWithIndependent{
        .sourceContainerId = QStringLiteral("source"),
        .pageId = QStringLiteral("source-page"),
        .independentWindowId = QStringLiteral("window-d"),
        .newContainerId = QStringLiteral("regrouped"),
        .independentPageId = QStringLiteral("target-page"),
        .independentLeafNodeId = QStringLiteral("target-leaf"),
    });
    QVERIFY2(regrouped.committed(), qPrintable(regrouped.message));
    QCOMPARE(regrouped.kind, TopologyCommandKind::RegroupPageWithIndependent);
    QVERIFY(!repository.topology().container(QStringLiteral("source")));
    QVERIFY(repository.topology().isIndependent(QStringLiteral("window-c")));
    const auto *group = repository.topology().container(QStringLiteral("regrouped"));
    QVERIFY(group);
    QCOMPARE(group->pages().size(), qsizetype{2});
    QCOMPARE(group->pages()[0].root().windowId(), QStringLiteral("window-d"));
    QCOMPARE(group->pages()[1].id(), QStringLiteral("source-page"));
    QCOMPARE(group->pages()[1].root().id(), QStringLiteral("source-split"));
    QCOMPARE(group->pages()[1].root().firstChild()->id(),
             QStringLiteral("source-leaf-a"));
    QCOMPARE(group->pages()[1].root().secondChild()->id(),
             QStringLiteral("source-leaf-b"));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("window-a")),
             std::optional<QString>(QStringLiteral("regrouped")));
    QCOMPARE(repository.topology().ownerOf(QStringLiteral("window-b")),
             std::optional<QString>(QStringLiteral("regrouped")));
    QVERIFY(repository.topology().validate().valid);
}

void TopologyPlacementTest::reparentsMemberPreservingLeafAndRejectsNoOps()
{
    auto container = splitContainer(QStringLiteral("container"),
                                    QStringLiteral("tree"),
                                    QStringLiteral("window-a"),
                                    QStringLiteral("window-b"));
    QString error;
    QVERIFY(container.splitWindow(
        {.targetWindowId = QStringLiteral("window-b"),
         .newWindowId = QStringLiteral("window-c"),
         .newLeafNodeId = QStringLiteral("leaf-c"),
         .splitNodeId = QStringLiteral("inner-split"),
         .orientation = Core::SplitOrientation::Horizontal,
         .ratio = 0.6,
         .position = Core::InsertPosition::Second},
        &error));
    TopologyRepository repository(topology({}, {container}, 5));
    AlwaysReadyFactory scene;
    TopologyCoordinator coordinator(repository, scene);

    const auto reparented = coordinator.execute(ReparentMember{
        .containerId = QStringLiteral("container"),
        .windowId = QStringLiteral("window-c"),
        .targetWindowId = QStringLiteral("window-a"),
        .splitNodeId = QStringLiteral("reparented-split"),
        .orientation = Core::SplitOrientation::Vertical,
        .ratio = 0.4,
        .position = Core::InsertPosition::First,
    });
    QVERIFY2(reparented.committed(), qPrintable(reparented.message));
    QCOMPARE(reparented.kind, TopologyCommandKind::ReparentMember);
    const auto *after = repository.topology().container(QStringLiteral("container"));
    QVERIFY(after);
    QCOMPARE(after->findWindow(QStringLiteral("window-c"))->id(),
             QStringLiteral("leaf-c"));
    const auto *split = after->findNode(QStringLiteral("reparented-split"));
    QVERIFY(split);
    QCOMPARE(split->orientation(), std::optional(Core::SplitOrientation::Vertical));
    QCOMPARE(split->ratio(), std::optional(0.4));
    QCOMPARE(split->firstChild()->windowId(), QStringLiteral("window-c"));
    QCOMPARE(split->secondChild()->windowId(), QStringLiteral("window-a"));

    const auto noOp = coordinator.execute(ReparentMember{
        .containerId = QStringLiteral("container"),
        .windowId = QStringLiteral("window-c"),
        .targetWindowId = QStringLiteral("window-a"),
        .splitNodeId = QStringLiteral("unused-split"),
        .orientation = Core::SplitOrientation::Vertical,
        .ratio = 0.4,
        .position = Core::InsertPosition::First,
    });
    QCOMPARE(noOp.error, TopologyCommandError::InvalidCommand);
    const auto collision = coordinator.execute(ReparentMember{
        .containerId = QStringLiteral("container"),
        .windowId = QStringLiteral("window-b"),
        .targetWindowId = QStringLiteral("window-a"),
        .splitNodeId = QStringLiteral("reparented-split"),
    });
    QCOMPARE(collision.error, TopologyCommandError::InvalidCommand);
    QCOMPARE(repository.topology().revision(), quint64{6});
    QCOMPARE(repository.topology().windowIds(QStringLiteral("container")),
             QStringList({QStringLiteral("window-c"),
                          QStringLiteral("window-a"),
                          QStringLiteral("window-b")}));
}

QTEST_APPLESS_MAIN(TopologyPlacementTest)

#include "tst_topology_placement.moc"
