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
    void outputWorkAreasKeepIndependentPageBounds();
    void reservesResponsivePagerAndCompactsFullscreenStrips();
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
    QVERIFY(first.pageCount > 1);
    QCOMPARE(first.appliedPage, 0);
    QVERIFY(!first.icons.isEmpty() || !first.containers.isEmpty());
    QVERIFY(first.field.contains(first.pagerFrame));
    QVector<QString> iconIds;
    QVector<QString> containerIds;
    for (int page = 0; page < first.pageCount; ++page) {
        const auto layout = planMinimizedGather(request, page);
        QVERIFY(layout.ok);
        QCOMPARE(layout.pageCount, first.pageCount);
        QVERIFY(layout.field.contains(layout.pagerFrame));
        QVERIFY(layout.itemArea.top() > layout.pagerFrame.bottom());
        for (const auto &placement : layout.icons) {
            QVERIFY(layout.itemArea.contains(placement.frame));
            QVERIFY(!layout.pagerFrame.intersects(placement.frame));
            iconIds.append(placement.id);
        }
        for (const auto &placement : layout.containers) {
            QVERIFY(layout.itemArea.contains(placement.frame));
            QVERIFY(!layout.pagerFrame.intersects(placement.frame));
            containerIds.append(placement.id);
            for (const auto &icon : layout.icons) {
                QVERIFY(!placement.frame.intersects(icon.frame));
            }
        }
    }
    QCOMPARE(iconIds, request.iconifiedWindowIds);
    QCOMPARE(containerIds, QStringList({
        QStringLiteral("container-0"), QStringLiteral("container-1"),
        QStringLiteral("container-2"), QStringLiteral("container-3"),
        QStringLiteral("container-4"), QStringLiteral("container-5"),
        QStringLiteral("container-6"), QStringLiteral("container-7"),
        QStringLiteral("container-8"), QStringLiteral("container-9"),
        QStringLiteral("container-10"), QStringLiteral("container-11"),
        QStringLiteral("container-12")}));
    const auto clamped = planMinimizedGather(request, 99);
    QVERIFY(clamped.ok);
    QCOMPARE(clamped.appliedPage, clamped.pageCount - 1);
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

void MinimizedGatherLayoutTests::outputWorkAreasKeepIndependentPageBounds()
{
    MinimizedGatherRequest primary;
    primary.workArea = QRectF(0, 28, 1280, 720);
    primary.margin = 18;
    primary.iconExtent = 48;
    primary.gap = 8;
    primary.iconifiedWindowIds = {QStringLiteral("primary-a"),
                                 QStringLiteral("primary-b")};
    const auto primaryLayout = planMinimizedGather(primary, 7);
    QVERIFY(primaryLayout.ok);
    QCOMPARE(primaryLayout.appliedPage, 0);
    for (const auto &item : primaryLayout.icons) {
        QVERIFY(primaryLayout.field.contains(item.frame));
        QVERIFY(item.frame.top() >= 46);
    }

    MinimizedGatherRequest secondary = primary;
    secondary.workArea = QRectF(1280, 52, 640, 480);
    secondary.iconifiedWindowIds = {QStringLiteral("secondary-a")};
    const auto secondaryLayout = planMinimizedGather(secondary, 7);
    QVERIFY(secondaryLayout.ok);
    QCOMPARE(secondaryLayout.appliedPage, 0);
    QCOMPARE(secondaryLayout.icons.constFirst().id, QStringLiteral("secondary-a"));
    QVERIFY(secondaryLayout.icons.constFirst().frame.left() >= 1298);
    QVERIFY(secondaryLayout.icons.constFirst().frame.top() >= 70);
}

void MinimizedGatherLayoutTests::reservesResponsivePagerAndCompactsFullscreenStrips()
{
    MinimizedGatherRequest wide;
    wide.workArea = QRectF(0, 28, 1920, 1052);
    wide.iconifiedWindowIds.reserve(500);
    for (int index = 0; index < 500; ++index) {
        wide.iconifiedWindowIds.append(QStringLiteral("wide-%1").arg(index));
    }
    wide.shadedContainers = {{QStringLiteral("fullscreen-strip"), QSizeF(1920, 31)}};
    const auto widePage = planMinimizedGather(wide, 0);
    QVERIFY(widePage.ok);
    QVERIFY(widePage.pageCount > 1);
    QVERIFY(!widePage.pagerFrame.isEmpty());
    QVERIFY(widePage.field.contains(widePage.pagerFrame));
    QVERIFY(widePage.itemArea.top() > widePage.pagerFrame.bottom());
    for (const auto &icon : widePage.icons) {
        QVERIFY(widePage.itemArea.contains(icon.frame));
        QVERIFY(!widePage.pagerFrame.intersects(icon.frame));
    }
    for (const auto &strip : widePage.containers) {
        QVERIFY(widePage.itemArea.contains(strip.frame));
        QVERIFY(strip.frame.width() <= wide.containerWidthLimit);
        QVERIFY(!widePage.pagerFrame.intersects(strip.frame));
    }
    QVector<QRectF> visibleFrames;
    for (const auto &icon : widePage.icons) {
        visibleFrames.append(icon.frame);
    }
    for (const auto &strip : widePage.containers) {
        visibleFrames.append(strip.frame);
    }
    for (qsizetype left = 0; left < visibleFrames.size(); ++left) {
        for (qsizetype right = left + 1; right < visibleFrames.size(); ++right) {
            QVERIFY(!visibleFrames.at(left).intersects(visibleFrames.at(right)));
        }
        QVERIFY(widePage.field.contains(visibleFrames.at(left)));
    }

    MinimizedGatherRequest narrow;
    narrow.workArea = QRectF(0, 0, 96, 540);
    narrow.iconifiedWindowIds = {QStringLiteral("narrow-a"),
                                 QStringLiteral("narrow-b"),
                                 QStringLiteral("narrow-c")};
    narrow.shadedContainers = {{QStringLiteral("narrow-fullscreen-strip"),
                               QSizeF(1920, 31)}};
    const auto narrowPage = planMinimizedGather(narrow, 99);
    QVERIFY(narrowPage.ok);
    QVERIFY(narrowPage.pageCount > 1);
    QCOMPARE(narrowPage.appliedPage, narrowPage.pageCount - 1);
    QVERIFY(narrowPage.field.contains(narrowPage.pagerFrame));
    QVERIFY(narrowPage.field.width() <= narrow.workArea.width());
    QVERIFY(narrowPage.field.height() <= narrow.workArea.height());
    QVERIFY(narrowPage.itemArea.top() > narrowPage.pagerFrame.bottom());
    QVERIFY(!narrowPage.icons.isEmpty() || !narrowPage.containers.isEmpty());
    for (const auto &item : narrowPage.icons) {
        QVERIFY(narrowPage.itemArea.contains(item.frame));
        QVERIFY(!narrowPage.pagerFrame.intersects(item.frame));
    }
    for (const auto &item : narrowPage.containers) {
        QVERIFY(narrowPage.itemArea.contains(item.frame));
        QVERIFY(item.frame.width() <= narrow.workArea.width());
        QVERIFY(!narrowPage.pagerFrame.intersects(item.frame));
    }
    QVERIFY(narrowPage.pagerFrame.contains(narrowPage.pagerPreviousButton));
    QVERIFY(narrowPage.pagerFrame.contains(narrowPage.pagerNextButton));
    QVERIFY(!narrowPage.pagerPreviousButton.intersects(narrowPage.pagerNextButton));
}

void MinimizedGatherLayoutTests::rejectsGeometryThatCannotFitOneItem()
{
    MinimizedGatherRequest request;
    request.workArea = QRectF(0, 0, 100, 80);
    request.margin = 40;
    const auto emptyField = planMinimizedGather(request);
    QVERIFY(!emptyField.ok);

    request.workArea = QRectF(0, 0, 99, 99);
    request.margin = 10;
    request.iconExtent = 100;
    request.iconifiedWindowIds = {QStringLiteral("too-large")};
    const auto oversized = planMinimizedGather(request);
    QVERIFY(!oversized.ok);
    QVERIFY(oversized.diagnostic.contains(QStringLiteral("fit")));
}

QTEST_APPLESS_MAIN(MinimizedGatherLayoutTests)
#include "tst_minimized_gather_layout.moc"
