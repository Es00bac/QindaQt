// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/hybrid_gather/gather_layout.h"

#include <QtTest>

using namespace QindaQt::HybridGather;

namespace {

// A 1080p output with the default 90-pixel buffer: the field is
// (90, 90, 1740, 900).
[[nodiscard]] GatherRequest request1080p()
{
    GatherRequest request;
    request.workArea = QRectF(0, 0, 1920, 1080);
    return request;
}

[[nodiscard]] QVector<QString> ids(const QString &prefix, int count)
{
    QVector<QString> out;
    out.reserve(count);
    for (int index = 0; index < count; ++index) {
        out.append(prefix + QString::number(index));
    }
    return out;
}

[[nodiscard]] QVector<GatherTile> tiles(int count, const QSizeF &size)
{
    QVector<GatherTile> out;
    out.reserve(count);
    for (int index = 0; index < count; ++index) {
        out.append(GatherTile{QStringLiteral("w%1").arg(index), size});
    }
    return out;
}

} // namespace

class GatherLayoutTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void insetsTheFieldByTheMargin();
    void placesThreeLanesLeftToRight();
    void stacksEachLaneAndWrapsIntoANewColumn();
    void fillsTheGridRowMajorWithoutMagnifying();
    void clampsScrollAndHidesRowsOutsideTheViewport();
    void reportsNoGridWhenTheLanesLeaveLessThanOneCell();
    void refusesAnUnusableRequest();
    void isDeterministic();
};

void GatherLayoutTests::insetsTheFieldByTheMargin()
{
    const GatherLayout layout = planGather(request1080p());
    QVERIFY2(layout.ok, qPrintable(layout.diagnostic));
    QCOMPARE(layout.field, QRectF(90, 90, 1740, 900));

    // The buffer is a parameter, not a constant.
    GatherRequest tight = request1080p();
    tight.margin = 24;
    QCOMPARE(planGather(tight).field, QRectF(24, 24, 1872, 1032));
}

void GatherLayoutTests::placesThreeLanesLeftToRight()
{
    GatherRequest request = request1080p();
    request.iconifiedWindowIds = ids(QStringLiteral("i"), 2);
    request.containerIds = ids(QStringLiteral("c"), 2);
    request.windows = tiles(3, QSizeF(1280, 720));
    const GatherLayout layout = planGather(request);
    QVERIFY2(layout.ok, qPrintable(layout.diagnostic));

    // Icons start at the field's top-left corner and are square.
    QCOMPARE(layout.icons.size(), 2);
    QCOMPARE(layout.icons.constFirst().frame, QRectF(90, 90, 48, 48));
    QVERIFY(layout.icons.constFirst().visible);

    // Cards begin one gap right of the icon lane: 90 + 48 + 16.
    QCOMPARE(layout.cards.size(), 2);
    QCOMPARE(layout.cards.constFirst().frame, QRectF(154, 90, 240, 64));

    // The grid begins one gap right of the card lane: 154 + 240 + 16, and
    // takes the rest of the field's width.
    QCOMPARE(layout.gridViewport, QRectF(410, 90, 1420, 900));
    QVERIFY(layout.gridViewport.right() <= layout.field.right() + 0.5);

    // Every lane is disjoint from the next, which is the whole point of the
    // arrangement: three regions, not an overlapping pile.
    for (const GatherPlacement &icon : layout.icons) {
        for (const GatherPlacement &card : layout.cards) {
            QVERIFY(!icon.frame.intersects(card.frame));
        }
        QVERIFY(!icon.frame.intersects(layout.gridViewport));
    }
    for (const GatherPlacement &card : layout.cards) {
        QVERIFY(!card.frame.intersects(layout.gridViewport));
    }

    // With no icons the card lane takes the field's left edge, and with
    // neither lane the grid does.
    GatherRequest cardsOnly = request;
    cardsOnly.iconifiedWindowIds.clear();
    QCOMPARE(planGather(cardsOnly).cards.constFirst().frame, QRectF(90, 90, 240, 64));
    GatherRequest windowsOnly = cardsOnly;
    windowsOnly.containerIds.clear();
    QCOMPARE(planGather(windowsOnly).gridViewport, QRectF(90, 90, 1740, 900));
}

void GatherLayoutTests::stacksEachLaneAndWrapsIntoANewColumn()
{
    GatherRequest request = request1080p();
    // A 900 px field fits floor((900 + 16) / 64) = 14 icons per column.
    request.iconifiedWindowIds = ids(QStringLiteral("i"), 15);
    const GatherLayout layout = planGather(request);
    QVERIFY2(layout.ok, qPrintable(layout.diagnostic));
    QCOMPARE(layout.icons.size(), 15);
    QCOMPARE(layout.icons.at(13).frame, QRectF(90, 90 + 13 * 64, 48, 48));
    // The fifteenth starts a second column one gap to the right, back at the
    // top of the field.
    QCOMPARE(layout.icons.at(14).frame, QRectF(90 + 48 + 16, 90, 48, 48));
    // Which pushes the card lane, and therefore the grid, right by one column.
    GatherRequest withCards = request;
    withCards.containerIds = ids(QStringLiteral("c"), 1);
    QCOMPARE(planGather(withCards).cards.constFirst().frame,
             QRectF(90 + 2 * 48 + 2 * 16, 90, 240, 64));
}

void GatherLayoutTests::fillsTheGridRowMajorWithoutMagnifying()
{
    GatherRequest request = request1080p();
    request.windows = tiles(7, QSizeF(1280, 720));
    const GatherLayout layout = planGather(request);
    QVERIFY2(layout.ok, qPrintable(layout.diagnostic));

    // A 1740 px field fits floor((1740 + 16) / 276) = 6 columns.
    QCOMPARE(layout.gridColumns, 6);
    QCOMPARE(layout.gridRows, 2);
    QCOMPARE(layout.windows.size(), 7);

    // 16:9 inside a 260x176 cell fits on width: 260x146.25, centred.
    const QRectF first = layout.windows.constFirst().frame;
    QCOMPARE(first.width(), 260.0);
    QVERIFY(qFuzzyCompare(first.height(), 260.0 * 720.0 / 1280.0));
    QVERIFY(qFuzzyCompare(first.center().x(), 90.0 + 130.0));

    // Row-major: the seventh tile is the second row's first column.
    QVERIFY(qFuzzyCompare(layout.windows.at(6).frame.center().x(),
                          layout.windows.constFirst().frame.center().x()));
    QVERIFY(layout.windows.at(6).frame.top() > layout.windows.constFirst().frame.top());

    // A window smaller than its cell keeps its own size rather than being
    // magnified into a blurry preview.
    GatherRequest small = request1080p();
    small.windows = tiles(1, QSizeF(200, 100));
    const QRectF fitted = planGather(small).windows.constFirst().frame;
    QCOMPARE(fitted.size(), QSizeF(200, 100));
    // An unusable source size falls back to the whole cell.
    GatherRequest unknown = request1080p();
    unknown.windows = tiles(1, QSizeF(0, 0));
    QCOMPARE(planGather(unknown).windows.constFirst().frame.size(), QSizeF(260, 176));
}

void GatherLayoutTests::clampsScrollAndHidesRowsOutsideTheViewport()
{
    GatherRequest request = request1080p();
    // 6 columns, 31 tiles: 6 rows, content 6 * 192 - 16 = 1136 over a 900 px
    // viewport, so 236 px of travel.
    request.windows = tiles(31, QSizeF(1280, 720));
    GatherLayout layout = planGather(request);
    QVERIFY2(layout.ok, qPrintable(layout.diagnostic));
    QCOMPARE(layout.gridRows, 6);
    QCOMPARE(layout.gridContentHeight, 1136.0);
    QCOMPARE(layout.maximumScrollOffset, 236.0);
    QCOMPARE(layout.appliedScrollOffset, 0.0);
    // The last row starts below the viewport before any scrolling.
    QVERIFY(!layout.windows.constLast().visible);

    request.scrollOffset = layout.maximumScrollOffset;
    layout = planGather(request);
    QCOMPARE(layout.appliedScrollOffset, 236.0);
    QVERIFY(layout.windows.constLast().visible);
    // Scrolled to the end, the first row has left the top of the viewport.
    QVERIFY(layout.windows.constFirst().frame.top() < layout.gridViewport.top());

    // Out-of-range wheel positions clamp rather than scrolling into nothing.
    request.scrollOffset = -500;
    QCOMPARE(planGather(request).appliedScrollOffset, 0.0);
    request.scrollOffset = 99999;
    QCOMPARE(planGather(request).appliedScrollOffset, 236.0);

    // A grid shorter than its viewport cannot scroll at all.
    GatherRequest shortGrid = request1080p();
    shortGrid.windows = tiles(3, QSizeF(1280, 720));
    shortGrid.scrollOffset = 400;
    const GatherLayout shortLayout = planGather(shortGrid);
    QCOMPARE(shortLayout.maximumScrollOffset, 0.0);
    QCOMPARE(shortLayout.appliedScrollOffset, 0.0);
}

void GatherLayoutTests::reportsNoGridWhenTheLanesLeaveLessThanOneCell()
{
    // AGENT-GUARD: hundreds of iconified windows must not produce a mangled
    // grid overlapping the card lane. The layout stays ok, the viewport is
    // empty, and every window is explicitly invisible.
    GatherRequest request = request1080p();
    request.iconifiedWindowIds = ids(QStringLiteral("i"), 14 * 30);
    request.windows = tiles(4, QSizeF(1280, 720));
    const GatherLayout layout = planGather(request);
    QVERIFY(layout.ok);
    QCOMPARE(layout.gridColumns, 0);
    QVERIFY(layout.gridViewport.isEmpty());
    QVERIFY(!layout.diagnostic.isEmpty());
    QCOMPARE(layout.windows.size(), 4);
    for (const GatherPlacement &window : layout.windows) {
        QVERIFY(!window.visible);
    }
    // Icons that ran past the field's right edge are invisible too, so a
    // caller cannot draw one outside the arrangement.
    QVERIFY(!layout.icons.constLast().visible);
    QVERIFY(layout.icons.constFirst().visible);
}

void GatherLayoutTests::refusesAnUnusableRequest()
{
    const auto refused = [](auto mutate) {
        GatherRequest request = request1080p();
        mutate(request);
        const GatherLayout layout = planGather(request);
        return !layout.ok && !layout.diagnostic.isEmpty();
    };
    QVERIFY(refused([](GatherRequest &r) { r.workArea = QRectF(); }));
    QVERIFY(refused([](GatherRequest &r) { r.workArea = QRectF(0, 0, 1920, 0); }));
    QVERIFY(refused([](GatherRequest &r) {
        r.workArea = QRectF(0, 0, std::numeric_limits<qreal>::infinity(), 1080);
    }));
    QVERIFY(refused([](GatherRequest &r) { r.margin = -1; }));
    QVERIFY(refused([](GatherRequest &r) { r.gap = -1; }));
    QVERIFY(refused([](GatherRequest &r) { r.iconExtent = 0; }));
    QVERIFY(refused([](GatherRequest &r) { r.cardSize = QSizeF(0, 64); }));
    QVERIFY(refused([](GatherRequest &r) { r.cellSize = QSizeF(260, -1); }));
    QVERIFY(refused([](GatherRequest &r) {
        r.scrollOffset = std::numeric_limits<qreal>::quiet_NaN();
    }));
    // A margin wider than the output leaves no field.
    QVERIFY(refused([](GatherRequest &r) { r.margin = 1000; }));
}

void GatherLayoutTests::isDeterministic()
{
    GatherRequest request = request1080p();
    request.iconifiedWindowIds = ids(QStringLiteral("i"), 5);
    request.containerIds = ids(QStringLiteral("c"), 3);
    request.windows = tiles(11, QSizeF(1600, 900));
    request.scrollOffset = 40;
    const GatherLayout first = planGather(request);
    const GatherLayout second = planGather(request);
    QVERIFY(first.ok);
    QCOMPARE(first, second);

    // Ordering is the input's ordering, so a caller can zip results back onto
    // its own inventories by index.
    for (qsizetype index = 0; index < request.windows.size(); ++index) {
        QCOMPARE(first.windows.at(index).id, request.windows.at(index).id);
    }
    for (qsizetype index = 0; index < request.iconifiedWindowIds.size(); ++index) {
        QCOMPARE(first.icons.at(index).id, request.iconifiedWindowIds.at(index));
    }
    for (qsizetype index = 0; index < request.containerIds.size(); ++index) {
        QCOMPARE(first.cards.at(index).id, request.containerIds.at(index));
    }
}

QTEST_GUILESS_MAIN(GatherLayoutTests)
#include "tst_gather_layout.moc"
