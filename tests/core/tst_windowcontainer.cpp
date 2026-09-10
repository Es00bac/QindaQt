// SPDX-License-Identifier: GPL-3.0-or-later
#include "windowcontainer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace QindaQt::Core;

class WindowContainerTest final : public QObject
{
    Q_OBJECT

private slots:
    void addsPagesAndBuildsSplitTrees();
    void splitsPageRootPreservingSubtree();
    void detachNormalizesTreeAndPreservesIds();
    void removingActivePageSelectsItsNeighbor();
    void detachesWholePageAndPreservesItsTree();
    void rejectsInvalidMutationsAtomically();
    void serializesRoundTrip();
    void rejectsMalformedJson();
    void identifiesSingletonForCompositorUnwrap();
};

void WindowContainerTest::addsPagesAndBuildsSplitTrees()
{
    WindowContainer container(QStringLiteral("container-1"));
    QString error;

    QVERIFY2(container.addPage(QStringLiteral("page-1"),
                               QStringLiteral("leaf-a"),
                               QStringLiteral("window-a"),
                               &error),
             qPrintable(error));
    QCOMPARE(container.activePageId(), QStringLiteral("page-1"));
    QVERIFY(container.addPage(QStringLiteral("page-2"),
                              QStringLiteral("leaf-z"),
                              QStringLiteral("window-z"),
                              &error));
    QCOMPARE(container.activePageId(), QStringLiteral("page-1"));
    QVERIFY(container.activatePage(QStringLiteral("page-2"), &error));

    const SplitRequest request{
        .targetWindowId = QStringLiteral("window-a"),
        .newWindowId = QStringLiteral("window-b"),
        .newLeafNodeId = QStringLiteral("leaf-b"),
        .splitNodeId = QStringLiteral("split-ab"),
        .orientation = SplitOrientation::Horizontal,
        .ratio = 0.4,
        .position = InsertPosition::First,
    };
    QVERIFY2(container.splitWindow(request, &error), qPrintable(error));

    const auto *page = container.page(QStringLiteral("page-1"));
    QVERIFY(page);
    const auto &root = page->root();
    QVERIFY(root.isSplit());
    QCOMPARE(root.id(), QStringLiteral("split-ab"));
    QVERIFY(root.orientation().has_value());
    QCOMPARE(root.orientation().value(), SplitOrientation::Horizontal);
    QCOMPARE(root.ratio().value(), 0.4);
    QCOMPARE(root.firstChild()->id(), QStringLiteral("leaf-b"));
    QCOMPARE(root.secondChild()->id(), QStringLiteral("leaf-a"));

    QVERIFY(container.setSplitRatio(QStringLiteral("split-ab"), 0.65, &error));
    QCOMPARE(container.findNode(QStringLiteral("split-ab"))->ratio().value(), 0.65);
    QVERIFY(container.validate().valid);
}

void WindowContainerTest::splitsPageRootPreservingSubtree()
{
    WindowContainer container(QStringLiteral("container-1"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("page-1"),
                              QStringLiteral("leaf-a"),
                              QStringLiteral("window-a"),
                              &error));
    QVERIFY(container.splitWindow(
        {.targetWindowId = QStringLiteral("window-a"),
         .newWindowId = QStringLiteral("window-b"),
         .newLeafNodeId = QStringLiteral("leaf-b"),
         .splitNodeId = QStringLiteral("split-ab"),
         .orientation = SplitOrientation::Horizontal,
         .ratio = 0.5,
         .position = InsertPosition::Second},
        &error));
    QVERIFY(container.addPage(QStringLiteral("page-2"),
                              QStringLiteral("leaf-z"),
                              QStringLiteral("window-z"),
                              &error));

    // Docking at the edge of the whole page wraps the complete existing tree:
    // the new window spans the full page beside [a | b] instead of splitting
    // one member tile.
    const PageSplitRequest request{
        .pageId = QStringLiteral("page-1"),
        .newWindowId = QStringLiteral("window-c"),
        .newLeafNodeId = QStringLiteral("leaf-c"),
        .splitNodeId = QStringLiteral("split-root"),
        .orientation = SplitOrientation::Vertical,
        .ratio = 0.3,
        .position = InsertPosition::First,
    };
    QVERIFY2(container.splitPage(request, &error), qPrintable(error));
    const auto &root = container.page(QStringLiteral("page-1"))->root();
    QVERIFY(root.isSplit());
    QCOMPARE(root.id(), QStringLiteral("split-root"));
    QCOMPARE(root.orientation().value(), SplitOrientation::Vertical);
    QCOMPARE(root.ratio().value(), 0.3);
    QCOMPARE(root.firstChild()->id(), QStringLiteral("leaf-c"));
    QCOMPARE(root.firstChild()->windowId(), QStringLiteral("window-c"));
    QCOMPARE(root.secondChild()->id(), QStringLiteral("split-ab"));
    QCOMPARE(root.secondChild()->firstChild()->id(), QStringLiteral("leaf-a"));
    QCOMPARE(root.secondChild()->secondChild()->id(), QStringLiteral("leaf-b"));
    QCOMPARE(container.page(QStringLiteral("page-2"))->root().id(),
             QStringLiteral("leaf-z"));
    QVERIFY(container.validate().valid);

    // A leaf root is wrapped the same way, with the new leaf on the requested
    // side, so a single-member page needs no special casing by callers.
    QVERIFY2(container.splitPage(
                 {.pageId = QStringLiteral("page-2"),
                  .newWindowId = QStringLiteral("window-y"),
                  .newLeafNodeId = QStringLiteral("leaf-y"),
                  .splitNodeId = QStringLiteral("split-zy"),
                  .orientation = SplitOrientation::Horizontal,
                  .ratio = 0.5,
                  .position = InsertPosition::Second},
                 &error),
             qPrintable(error));
    const auto &second = container.page(QStringLiteral("page-2"))->root();
    QCOMPARE(second.id(), QStringLiteral("split-zy"));
    QCOMPARE(second.firstChild()->id(), QStringLiteral("leaf-z"));
    QCOMPARE(second.secondChild()->id(), QStringLiteral("leaf-y"));

    // Rejections leave the tree untouched.
    const auto before = container.toJson();
    auto unknownPage = request;
    unknownPage.pageId = QStringLiteral("missing");
    unknownPage.newWindowId = QStringLiteral("window-d");
    unknownPage.newLeafNodeId = QStringLiteral("leaf-d");
    unknownPage.splitNodeId = QStringLiteral("split-d");
    QVERIFY(!container.splitPage(unknownPage, &error));
    QVERIFY(error.contains(QStringLiteral("unknown page")));
    auto duplicateWindow = request;
    duplicateWindow.newWindowId = QStringLiteral("window-a");
    duplicateWindow.newLeafNodeId = QStringLiteral("leaf-d");
    duplicateWindow.splitNodeId = QStringLiteral("split-d");
    QVERIFY(!container.splitPage(duplicateWindow, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate window")));
    auto duplicateNode = request;
    duplicateNode.newWindowId = QStringLiteral("window-d");
    duplicateNode.newLeafNodeId = QStringLiteral("leaf-d");
    duplicateNode.splitNodeId = QStringLiteral("leaf-a");
    QVERIFY(!container.splitPage(duplicateNode, &error));
    QVERIFY(error.contains(QStringLiteral("unique")));
    auto badRatio = request;
    badRatio.newWindowId = QStringLiteral("window-d");
    badRatio.newLeafNodeId = QStringLiteral("leaf-d");
    badRatio.splitNodeId = QStringLiteral("split-d");
    badRatio.ratio = 1.0;
    QVERIFY(!container.splitPage(badRatio, &error));
    QVERIFY(error.contains(QStringLiteral("between 0 and 1")));
    QCOMPARE(container.toJson(), before);
}

void WindowContainerTest::detachNormalizesTreeAndPreservesIds()
{
    WindowContainer container(QStringLiteral("container"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("page"),
                              QStringLiteral("leaf-a"),
                              QStringLiteral("window-a"),
                              &error));
    QVERIFY(container.splitWindow({QStringLiteral("window-a"),
                                   QStringLiteral("window-b"),
                                   QStringLiteral("leaf-b"),
                                   QStringLiteral("split-outer"),
                                   SplitOrientation::Horizontal,
                                   0.5,
                                   InsertPosition::Second},
                                  &error));
    QVERIFY(container.splitWindow({QStringLiteral("window-b"),
                                   QStringLiteral("window-c"),
                                   QStringLiteral("leaf-c"),
                                   QStringLiteral("split-inner"),
                                   SplitOrientation::Vertical,
                                   0.6,
                                   InsertPosition::Second},
                                  &error));

    const auto detachedB = container.detachWindow(QStringLiteral("window-b"), &error);
    QVERIFY2(detachedB.has_value(), qPrintable(error));
    QCOMPARE(detachedB->leafNodeId, QStringLiteral("leaf-b"));
    QCOMPARE(detachedB->sourcePageId, QStringLiteral("page"));
    const auto &afterB = container.page(QStringLiteral("page"))->root();
    QCOMPARE(afterB.id(), QStringLiteral("split-outer"));
    QCOMPARE(afterB.secondChild()->id(), QStringLiteral("leaf-c"));
    QVERIFY(!container.findNode(QStringLiteral("split-inner")));

    const auto detachedA = container.detachWindow(QStringLiteral("window-a"), &error);
    QVERIFY(detachedA.has_value());
    const auto &afterA = container.page(QStringLiteral("page"))->root();
    QCOMPARE(afterA.id(), QStringLiteral("leaf-c"));
    QCOMPARE(afterA.windowId(), QStringLiteral("window-c"));
    QVERIFY(!container.findNode(QStringLiteral("split-outer")));

    QVERIFY(container.removeWindow(QStringLiteral("window-c"), &error));
    QVERIFY(container.pages().isEmpty());
    QVERIFY(container.activePageId().isEmpty());
    QVERIFY(container.validate().valid);
}

void WindowContainerTest::removingActivePageSelectsItsNeighbor()
{
    WindowContainer container(QStringLiteral("container"));
    QString error;
    for (int index = 1; index <= 3; ++index) {
        QVERIFY(container.addPage(QStringLiteral("page-%1").arg(index),
                                  QStringLiteral("leaf-%1").arg(index),
                                  QStringLiteral("window-%1").arg(index),
                                  &error));
    }
    QVERIFY(container.activatePage(QStringLiteral("page-2"), &error));
    QVERIFY(container.removeWindow(QStringLiteral("window-2"), &error));
    QCOMPARE(container.activePageId(), QStringLiteral("page-3"));

    QVERIFY(container.removeWindow(QStringLiteral("window-3"), &error));
    QCOMPARE(container.activePageId(), QStringLiteral("page-1"));
    QVERIFY(container.validate().valid);
}

void WindowContainerTest::detachesWholePageAndPreservesItsTree()
{
    WindowContainer container(QStringLiteral("container"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("page-a"), QStringLiteral("leaf-a"),
                              QStringLiteral("window-a"), &error));
    QVERIFY(container.splitWindow({QStringLiteral("window-a"),
                                   QStringLiteral("window-b"),
                                   QStringLiteral("leaf-b"),
                                   QStringLiteral("split-ab"),
                                   SplitOrientation::Horizontal,
                                   0.4,
                                   InsertPosition::Second},
                                  &error));
    QVERIFY(container.addPage(QStringLiteral("page-c"), QStringLiteral("leaf-c"),
                              QStringLiteral("window-c"), &error));
    QVERIFY(container.activatePage(QStringLiteral("page-a"), &error));

    const auto detached = container.detachPage(QStringLiteral("page-a"), &error);
    QVERIFY2(detached.has_value(), qPrintable(error));
    QCOMPARE(detached->id(), QStringLiteral("page-a"));
    QCOMPARE(detached->root().id(), QStringLiteral("split-ab"));
    QCOMPARE(detached->root().firstChild()->id(), QStringLiteral("leaf-a"));
    QCOMPARE(detached->root().secondChild()->id(), QStringLiteral("leaf-b"));
    QCOMPARE(container.activePageId(), QStringLiteral("page-c"));
    QVERIFY(!container.page(QStringLiteral("page-a")));

    const auto last = container.detachPage(QStringLiteral("page-c"), &error);
    QVERIFY(last.has_value());
    QVERIFY(container.pages().isEmpty());
    QVERIFY(container.activePageId().isEmpty());
    QVERIFY(container.validate().valid);
    QVERIFY(!container.detachPage(QStringLiteral("missing"), &error));
}

void WindowContainerTest::rejectsInvalidMutationsAtomically()
{
    WindowContainer container(QStringLiteral("container"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("page"),
                              QStringLiteral("leaf-a"),
                              QStringLiteral("window-a"),
                              &error));
    const auto before = container.toJson();

    QVERIFY(!container.addPage(QStringLiteral("container"),
                               QStringLiteral("leaf-b"),
                               QStringLiteral("window-b"),
                               &error));
    QCOMPARE(container.toJson(), before);
    QVERIFY(!container.addPage(QStringLiteral("page-2"),
                               QStringLiteral("leaf-b"),
                               QStringLiteral("window-a"),
                               &error));
    QCOMPARE(container.toJson(), before);

    const auto invalidTree = LayoutNode::makeSplit(
        QStringLiteral("split"),
        SplitOrientation::Vertical,
        1.0,
        LayoutNode::makeLeaf(QStringLiteral("left"), QStringLiteral("window-left")),
        LayoutNode::makeLeaf(QStringLiteral("right"), QStringLiteral("window-right")));
    QVERIFY(!container.addPage(ContainerPage(QStringLiteral("invalid-page"), invalidTree),
                               &error));
    QCOMPARE(container.toJson(), before);

    QVERIFY(!container.splitWindow({QStringLiteral("window-a"),
                                    QStringLiteral("window-b"),
                                    QStringLiteral("same-id"),
                                    QStringLiteral("same-id"),
                                    SplitOrientation::Horizontal,
                                    0.5,
                                    InsertPosition::Second},
                                   &error));
    QVERIFY(!container.setSplitRatio(QStringLiteral("leaf-a"), 0.5, &error));
    QVERIFY(!container.activatePage(QStringLiteral("missing"), &error));
    QVERIFY(!container.detachWindow(QStringLiteral("missing"), &error));
    QCOMPARE(container.toJson(), before);
    QVERIFY(container.validate().valid);
}

void WindowContainerTest::serializesRoundTrip()
{
    WindowContainer original(QStringLiteral("container"));
    QString error;
    QVERIFY(original.addPage(QStringLiteral("page-1"),
                             QStringLiteral("leaf-a"),
                             QStringLiteral("window-a"),
                             &error));
    QVERIFY(original.splitWindow({QStringLiteral("window-a"),
                                  QStringLiteral("window-b"),
                                  QStringLiteral("leaf-b"),
                                  QStringLiteral("split-ab"),
                                  SplitOrientation::Vertical,
                                  0.375,
                                  InsertPosition::Second},
                                 &error));
    QVERIFY(original.addPage(QStringLiteral("page-2"),
                             QStringLiteral("leaf-c"),
                             QStringLiteral("window-c"),
                             &error));
    QVERIFY(original.activatePage(QStringLiteral("page-2"), &error));

    const auto serialized = original.toJson();
    const auto restored = WindowContainer::fromJson(serialized, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QVERIFY(restored->validate().valid);
    QCOMPARE(restored->toJson(), serialized);
    QCOMPARE(restored->activePageId(), QStringLiteral("page-2"));
    QCOMPARE(restored->findWindow(QStringLiteral("window-b"))->id(),
             QStringLiteral("leaf-b"));

    WindowContainer empty(QStringLiteral("empty"));
    const auto emptyRestored = WindowContainer::fromJson(empty.toJson(), &error);
    QVERIFY2(emptyRestored.has_value(), qPrintable(error));
    QCOMPARE(emptyRestored->toJson(), empty.toJson());
}

void WindowContainerTest::rejectsMalformedJson()
{
    WindowContainer valid(QStringLiteral("container"));
    QString error;
    QVERIFY(valid.addPage(QStringLiteral("page"),
                          QStringLiteral("leaf"),
                          QStringLiteral("window"),
                          &error));

    auto invalidRatio = valid.toJson();
    auto pages = invalidRatio.value(QStringLiteral("pages")).toArray();
    auto page = pages[0].toObject();
    page.insert(QStringLiteral("root"),
                QJsonObject{{QStringLiteral("id"), QStringLiteral("split")},
                            {QStringLiteral("type"), QStringLiteral("split")},
                            {QStringLiteral("orientation"), QStringLiteral("horizontal")},
                            {QStringLiteral("ratio"), 0.0},
                            {QStringLiteral("first"), valid.page(QStringLiteral("page"))->root().toJson()},
                            {QStringLiteral("second"), valid.page(QStringLiteral("page"))->root().toJson()}});
    pages[0] = page;
    invalidRatio.insert(QStringLiteral("pages"), pages);
    QVERIFY(!WindowContainer::fromJson(invalidRatio, &error));
    QVERIFY(!error.isEmpty());

    auto missingActive = valid.toJson();
    missingActive.remove(QStringLiteral("activePageId"));
    QVERIFY(!WindowContainer::fromJson(missingActive, &error));

    auto wrongVersion = valid.toJson();
    wrongVersion.insert(QStringLiteral("schemaVersion"), 99);
    QVERIFY(!WindowContainer::fromJson(wrongVersion, &error));
}

void WindowContainerTest::identifiesSingletonForCompositorUnwrap()
{
    WindowContainer container(QStringLiteral("container"));
    QVERIFY(!container.singleWindowId().has_value());

    QString error;
    QVERIFY(container.addPage(QStringLiteral("page-a"),
                              QStringLiteral("leaf-a"),
                              QStringLiteral("window-a"),
                              &error));
    QCOMPARE(container.singleWindowId(), std::optional<QString>(QStringLiteral("window-a")));

    QVERIFY(container.splitWindow({QStringLiteral("window-a"),
                                   QStringLiteral("window-b"),
                                   QStringLiteral("leaf-b"),
                                   QStringLiteral("split-ab"),
                                   SplitOrientation::Horizontal,
                                   0.5,
                                   InsertPosition::Second},
                                  &error));
    QVERIFY(!container.singleWindowId().has_value());

    QVERIFY(container.detachWindow(QStringLiteral("window-b"), &error).has_value());
    QCOMPARE(container.singleWindowId(), std::optional<QString>(QStringLiteral("window-a")));
    QVERIFY(container.addPage(QStringLiteral("page-c"),
                              QStringLiteral("leaf-c"),
                              QStringLiteral("window-c"),
                              &error));
    QVERIFY(!container.singleWindowId().has_value());
}

QTEST_APPLESS_MAIN(WindowContainerTest)

#include "tst_windowcontainer.moc"
