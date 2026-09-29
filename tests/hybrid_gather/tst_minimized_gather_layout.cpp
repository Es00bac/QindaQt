// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_gather/gather_layout.h"

#include <QtTest>

using namespace QindaQt::HybridGather;

class MinimizedGatherLayoutTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void paginatesBothLanesInsideReservedWorkArea();
    void narrowOutputUsesNonOverlappingKindPages();
    void rejectsGeometryThatCannotFitOneItem();
};

void MinimizedGatherLayoutTests::paginatesBothLanesInsideReservedWorkArea()
{
    MinimizedGatherRequest request;
    request.workArea = QRectF(100, 70, 500, 350);
    request.margin = 20;
    request.gap = 10;
    request.iconExtent = 40;
    for (int index = 0; index < 45; ++index) {
        request.iconifiedWindowIds.append(QStringLiteral("window-%1").arg(index));
    }
    for (int index = 0; index < 13; ++index) {
        request.shadedContainers.append({QStringLiteral("container-%1").arg(index),
                                         QSizeF(70, 30)});
    }

    const auto first = planMinimizedGather(request);
    QVERIFY(first.ok);
    QCOMPARE(first.pageCount, 2);
    QCOMPARE(first.appliedPage, 0);
    QVERIFY(!first.icons.isEmpty());
    QVERIFY(!first.containers.isEmpty());
    for (const auto &placement : first.icons) {
        QVERIFY(first.field.contains(placement.frame));
    }
    for (const auto &placement : first.containers) {
        QVERIFY(first.field.contains(placement.frame));
        for (const auto &icon : first.icons) {
            QVERIFY(!placement.frame.intersects(icon.frame));
        }
    }

    const auto last = planMinimizedGather(request, 99);
    QVERIFY(last.ok);
    QCOMPARE(last.appliedPage, 1);
    QVERIFY(last.icons.size() + first.icons.size() == 45);
    QVERIFY(last.containers.size() + first.containers.size() == 13);
    QCOMPARE(last.icons.constFirst().id, QStringLiteral("window-42"));
    QCOMPARE(last.containers.constFirst().id, QStringLiteral("container-8"));
    for (const auto &placement : last.icons) {
        QVERIFY(last.field.contains(placement.frame));
    }
    for (const auto &placement : last.containers) {
        QVERIFY(last.field.contains(placement.frame));
    }
}

void MinimizedGatherLayoutTests::narrowOutputUsesNonOverlappingKindPages()
{
    MinimizedGatherRequest request;
    request.workArea = QRectF(0, 0, 300, 260);
    request.margin = 10;
    request.gap = 12;
    request.iconExtent = 48;
    request.iconifiedWindowIds = {QStringLiteral("a"), QStringLiteral("b")};
    request.shadedContainers = {{QStringLiteral("group-a"), QSizeF(240, 64)},
                                {QStringLiteral("group-b"), QSizeF(200, 40)}};

    const auto icons = planMinimizedGather(request, 0);
    QVERIFY(icons.ok);
    QCOMPARE(icons.pageCount, 2);
    QCOMPARE(icons.icons.size(), 2);
    QVERIFY(icons.containers.isEmpty());
    const auto cards = planMinimizedGather(request, 1);
    QVERIFY(cards.ok);
    QCOMPARE(cards.icons.size(), 0);
    QCOMPARE(cards.containers.size(), 2);
    for (const auto &placement : cards.containers) {
        QVERIFY(cards.field.contains(placement.frame));
    }
}

void MinimizedGatherLayoutTests::rejectsGeometryThatCannotFitOneItem()
{
    MinimizedGatherRequest request;
    request.workArea = QRectF(0, 0, 100, 80);
    request.margin = 40;
    const auto emptyField = planMinimizedGather(request);
    QVERIFY(!emptyField.ok);

    request.workArea = QRectF(0, 0, 100, 100);
    request.margin = 10;
    request.iconExtent = 100;
    request.iconifiedWindowIds = {QStringLiteral("too-large")};
    const auto oversized = planMinimizedGather(request);
    QVERIFY(!oversized.ok);
    QVERIFY(oversized.diagnostic.contains(QStringLiteral("fit")));
}

QTEST_APPLESS_MAIN(MinimizedGatherLayoutTests)
#include "tst_minimized_gather_layout.moc"
