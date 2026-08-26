// SPDX-License-Identifier: GPL-3.0-or-later
#include "windowcontainer.h"

#include <QtTest>

using namespace QindaQt::Core;

class ReorganizationTest final : public QObject
{
    Q_OBJECT

private slots:
    void swapsWindowPayloadsWithoutChangingStructure();
    void movesPagesByFinalIndex();
};

void ReorganizationTest::swapsWindowPayloadsWithoutChangingStructure()
{
    WindowContainer container(QStringLiteral("container"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("page-1"),
                              QStringLiteral("leaf-a"),
                              QStringLiteral("window-a"),
                              &error));
    QVERIFY(container.splitWindow({QStringLiteral("window-a"),
                                   QStringLiteral("window-b"),
                                   QStringLiteral("leaf-b"),
                                   QStringLiteral("split-ab"),
                                   SplitOrientation::Horizontal,
                                   0.4,
                                   InsertPosition::Second},
                                  &error));
    QVERIFY(container.addPage(QStringLiteral("page-2"),
                              QStringLiteral("leaf-c"),
                              QStringLiteral("window-c"),
                              &error));

    QVERIFY2(container.swapWindows(QStringLiteral("window-a"),
                                   QStringLiteral("window-c"),
                                   &error),
             qPrintable(error));
    QCOMPARE(container.findWindow(QStringLiteral("window-a"))->id(),
             QStringLiteral("leaf-c"));
    QCOMPARE(container.findWindow(QStringLiteral("window-c"))->id(),
             QStringLiteral("leaf-a"));
    QCOMPARE(container.page(QStringLiteral("page-1"))->root().id(),
             QStringLiteral("split-ab"));
    QCOMPARE(container.findNode(QStringLiteral("leaf-a"))->windowId(),
             QStringLiteral("window-c"));
    QVERIFY(container.validate().valid);

    const auto serialized = container.toJson();
    const auto restored = WindowContainer::fromJson(serialized, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->toJson(), serialized);

    const auto beforeRejectedOperations = container.toJson();
    QVERIFY(!container.swapWindows(QStringLiteral("window-a"),
                                   QStringLiteral("window-a"),
                                   &error));
    QVERIFY(!container.swapWindows(QStringLiteral("window-a"),
                                   QStringLiteral("missing"),
                                   &error));
    QCOMPARE(container.toJson(), beforeRejectedOperations);
}

void ReorganizationTest::movesPagesByFinalIndex()
{
    WindowContainer container(QStringLiteral("container"));
    QString error;
    for (int index = 1; index <= 4; ++index) {
        QVERIFY(container.addPage(QStringLiteral("page-%1").arg(index),
                                  QStringLiteral("leaf-%1").arg(index),
                                  QStringLiteral("window-%1").arg(index),
                                  &error));
    }
    QVERIFY(container.activatePage(QStringLiteral("page-2"), &error));

    QVERIFY2(container.movePage(QStringLiteral("page-1"), 3, &error), qPrintable(error));
    QCOMPARE(container.pages()[0].id(), QStringLiteral("page-2"));
    QCOMPARE(container.pages()[1].id(), QStringLiteral("page-3"));
    QCOMPARE(container.pages()[2].id(), QStringLiteral("page-4"));
    QCOMPARE(container.pages()[3].id(), QStringLiteral("page-1"));
    QCOMPARE(container.activePageId(), QStringLiteral("page-2"));

    QVERIFY(container.movePage(QStringLiteral("page-4"), 0, &error));
    QCOMPARE(container.pages()[0].id(), QStringLiteral("page-4"));
    QCOMPARE(container.pages()[1].id(), QStringLiteral("page-2"));
    QVERIFY(container.validate().valid);

    const auto serialized = container.toJson();
    const auto restored = WindowContainer::fromJson(serialized, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->toJson(), serialized);

    const auto beforeRejectedOperations = container.toJson();
    QVERIFY(!container.movePage(QStringLiteral("missing"), 0, &error));
    QVERIFY(!container.movePage(QStringLiteral("page-4"), -1, &error));
    QVERIFY(!container.movePage(QStringLiteral("page-4"), container.pages().size(), &error));
    QVERIFY(!container.movePage(QStringLiteral("page-4"), 0, &error));
    QCOMPARE(container.toJson(), beforeRejectedOperations);
}

QTEST_APPLESS_MAIN(ReorganizationTest)

#include "tst_reorganization.moc"
