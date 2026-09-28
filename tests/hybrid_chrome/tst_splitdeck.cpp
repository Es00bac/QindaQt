// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0281: the split-deck container title row (Corner Bar). Layout caps and
// gap, button placement, the card-deck carousel, hit testing through the gap,
// wheel stepping, rendering, and the paint-only glide.
#include "qindaqt/hybrid_chrome/chromehittest.h"
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_chrome/chromerenderer.h"
#include "qindaqt/hybrid_chrome/chromesplitdeck.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QtTest>

#include <algorithm>
#include <tuple>

using namespace QindaQt::HybridChrome;

namespace {

constexpr qreal Epsilon = 0.01;

ChromeLayoutRequest splitRequest(qreal width, int tabCount, int activeIndex,
                                 ButtonSide side = ButtonSide::Left,
                                 TabVisualDirection direction = TabVisualDirection::LeftToRight)
{
    ChromeLayoutRequest request;
    request.containerId = QStringLiteral("container-deck");
    request.outerRect = QRectF(100.0, 50.0, width, 500.0);
    request.containerFocused = true;
    request.style = ChromeStyle::standard(side);
    request.style.tabDirection = direction;
    request.style.titleLayout = ContainerTitleLayout::SplitDeck;
    request.style.palette.titleBar = QColor(QStringLiteral("#F6C02B"));
    request.style.palette.titleBarInactive = QColor(QStringLiteral("#DDD8CB"));
    request.containerTitle = QStringLiteral("Container");
    request.containerTitleIsGenerated = true;
    request.containerTitleWidth = 64.0;
    for (int index = 0; index < tabCount; ++index) {
        request.tabs.append({QStringLiteral("page-%1").arg(index),
                             QStringLiteral("Page %1").arg(index), index == activeIndex});
    }
    const QRectF inner = request.outerRect.adjusted(1.0, 1.0, -1.0, -1.0);
    request.members = {{QStringLiteral("member-a"), QStringLiteral("Editor"),
                        QRectF(inner.left(), inner.top() + 28.0, inner.width(),
                               inner.height() - 28.0)}};
    return request;
}

ChromeRenderPlan build(const ChromeLayoutRequest &request)
{
    QString error;
    const auto plan = ChromeLayoutEngine::build(request, &error);
    if (!plan) {
        qFatal("split-deck plan rejected: %s", qPrintable(error));
    }
    return *plan;
}

qreal rowWidth(const ChromeRenderPlan &plan) { return plan.outerTitleBar.width(); }

QImage render(const ChromeRenderPlan &plan, const ChromePaintState &state = {})
{
    QImage image(qCeil(plan.outerFrame.width()), qCeil(plan.outerFrame.height()),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.translate(-plan.outerFrame.topLeft());
    ChromeRenderer::paint(painter, plan, state);
    return image;
}

QColor pixel(const QImage &image, const ChromeRenderPlan &plan, const QPointF &global)
{
    const QPointF local = global - plan.outerFrame.topLeft();
    return image.pixelColor(qFloor(local.x()), qFloor(local.y()));
}

} // namespace

class SplitDeckTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void titleTabHoldsNameAndButtonsWithinTwentyPercent();
    void longOrUnmeasuredNamesTakeExactlyTheShare();
    void rightButtonSidePutsTheClusterAfterTheName();
    void deckAnchorsRightWithinSeventyFivePercentAndKeepsTheGap();
    void tabsThatFitLineUpInLogicalOrder_data();
    void tabsThatFitLineUpInLogicalOrder();
    void overflowBuildsACardDeckAroundTheActiveTab_data();
    void overflowBuildsACardDeckAroundTheActiveTab();
    void everyTabStaysInThePlanForAccessibility();
    void narrowContainerKeepsButtonsDragAndGap();
    void tooNarrowRowIsRejected();
    void hitTestPrefersTheNearestCardAndMarksTheWheel();
    void gapIsNotChromeAndHasNoTopResize();
    void titleTabDragsAndDeckSurfaceStepsTheWheel();
    void keyboardChipSitsBesideTheName();
    void wheelStepsStopAtEitherEnd();
    void rendererLeavesTheGapTransparentAndPaintsTheTab();
    void frontCardPaintsOverItsNeighbours();
    void unfocusedTabUsesTheInactiveColor();
    void glideInterpolatesOnlyTabRects();
    void classicLayoutIsUnchanged();
    void cornerBarThemesRenderEveryDeckState();
};

void SplitDeckTests::titleTabHoldsNameAndButtonsWithinTwentyPercent()
{
    const auto plan = build(splitRequest(1000.0, 3, 0));
    const QRectF row = plan.outerTitleBar;
    QVERIFY(plan.titleTab.isValid());
    QVERIFY(plan.titleTab.right() - row.left() <= rowWidth(plan) * ChromeSplitDeck::TitleShare
                                                     + Epsilon);
    // Sized to its content: buttons, a gap, the measured name and padding.
    const qreal cluster = plan.buttons.constLast().rect.right()
        - plan.buttons.constFirst().rect.left();
    const qreal expected = plan.metrics.buttonClusterInset + cluster
        + plan.metrics.titleHorizontalInset + 64.0 + 2.0 * ChromeSplitDeck::TitleTextInset
        + plan.metrics.buttonClusterInset / 2.0;
    QVERIFY(qAbs((plan.titleTab.right() - row.left()) - expected) < Epsilon);
    // Theme placement "left": close, minimize, maximize from the left edge.
    QCOMPARE(plan.buttons[0].action, WindowAction::Close);
    QCOMPARE(plan.buttons[1].action, WindowAction::Minimize);
    QCOMPARE(plan.buttons[2].action, WindowAction::Maximize);
    QVERIFY(qAbs(plan.buttons[0].rect.left() - (row.left() + plan.metrics.buttonClusterInset))
            < Epsilon);
    for (const auto &button : plan.buttons) {
        QVERIFY(plan.titleTab.contains(button.rect));
    }
    QVERIFY(plan.titleLabelRect.left() > plan.buttons.constLast().rect.right());
    QVERIFY(plan.titleTab.contains(plan.titleLabelRect));
    QVERIFY(plan.titleLabelRect.width() >= 64.0);
    QCOMPARE(plan.outerTitleDragRect.left(), row.left());
    QCOMPARE(plan.outerTitleDragRect.right(), plan.titleTab.right());
}

void SplitDeckTests::longOrUnmeasuredNamesTakeExactlyTheShare()
{
    auto request = splitRequest(1000.0, 3, 0);
    request.containerTitle = QStringLiteral("A very long container name that cannot fit");
    request.containerTitleWidth = 900.0;
    const auto longPlan = build(request);
    const qreal share = rowWidth(longPlan) * ChromeSplitDeck::TitleShare;
    QVERIFY(qAbs(longPlan.titleTab.right() - longPlan.outerTitleBar.left() - share) < Epsilon);
    QVERIFY(longPlan.titleLabelRect.width() < 900.0);

    // Unknown width is not zero: the tab takes the whole share, not a guess.
    request.containerTitleWidth = 0.0;
    const auto unmeasured = build(request);
    QVERIFY(qAbs(unmeasured.titleTab.right() - unmeasured.outerTitleBar.left() - share)
            < Epsilon);
}

void SplitDeckTests::rightButtonSidePutsTheClusterAfterTheName()
{
    const auto plan = build(splitRequest(1000.0, 3, 0, ButtonSide::Right));
    // Symbols on the right read minimize, maximize, close.
    QCOMPARE(plan.buttons.constLast().action, WindowAction::Close);
    QVERIFY(qAbs(plan.buttons.constLast().rect.right()
                 - (plan.titleTab.right() - plan.metrics.buttonClusterInset))
            < Epsilon);
    QVERIFY(plan.titleLabelRect.right() < plan.buttons.constFirst().rect.left());
    QVERIFY(plan.titleLabelRect.left() >= plan.outerTitleBar.left());
    // The deck and its controls stay on the right whatever the button side.
    QVERIFY(plan.controls.constFirst().rect.left() > plan.titleTab.right());
}

void SplitDeckTests::deckAnchorsRightWithinSeventyFivePercentAndKeepsTheGap()
{
    for (const int tabs : {1, 3, 12}) {
        const auto plan = build(splitRequest(1000.0, tabs, 0));
        QCOMPARE(plan.deckPiece.right(), plan.outerFrame.right());
        const qreal deckInRow = plan.outerTitleBar.right() - plan.deckPiece.left();
        QVERIFY2(deckInRow <= rowWidth(plan) * ChromeSplitDeck::DeckShare + Epsilon,
                 qPrintable(QString::number(deckInRow)));
        const qreal gap = plan.deckPiece.left() - plan.titleTab.right();
        QVERIFY2(gap >= rowWidth(plan) * ChromeSplitDeck::MinimumGapShare - Epsilon,
                 qPrintable(QString::number(gap)));
        // Controls at the far right, cards left of them.
        QVERIFY(plan.controls.constLast().rect.right() < plan.outerTitleBar.right());
        for (const auto &tab : plan.tabs) {
            QVERIFY(tab.rect.right() < plan.controls.constFirst().rect.left());
            QVERIFY(tab.rect.left() >= plan.deckPiece.left());
        }
        // Few tabs: the deck grows only as far as its content.
        if (tabs == 1) {
            QVERIFY(deckInRow < rowWidth(plan) * 0.4);
        }
    }
}

void SplitDeckTests::tabsThatFitLineUpInLogicalOrder_data()
{
    QTest::addColumn<TabVisualDirection>("direction");
    QTest::newRow("left-to-right") << TabVisualDirection::LeftToRight;
    QTest::newRow("right-to-left") << TabVisualDirection::RightToLeft;
}

void SplitDeckTests::tabsThatFitLineUpInLogicalOrder()
{
    QFETCH(TabVisualDirection, direction);
    const auto plan = build(splitRequest(1200.0, 3, 1, ButtonSide::Left, direction));
    QVERIFY(!plan.tabsOverflowed);
    QCOMPARE(plan.tabs.size(), 3);
    for (qsizetype index = 0; index < plan.tabs.size(); ++index) {
        QCOMPARE(plan.tabs[index].logicalIndex, index);
        QCOMPARE(plan.tabs[index].tabId, QStringLiteral("page-%1").arg(index));
        QCOMPARE(plan.tabs[index].deckDepth, 0);
        QCOMPARE(plan.tabs[index].rect.width(), plan.tabs[0].rect.width());
    }
    const bool ltr = direction == TabVisualDirection::LeftToRight;
    QVERIFY((plan.tabs[0].rect.center().x() < plan.tabs[1].rect.center().x()) == ltr);
    QVERIFY((plan.tabs[1].rect.center().x() < plan.tabs[2].rect.center().x()) == ltr);
    // Side by side, never overlapping.
    for (qsizetype a = 0; a < plan.tabs.size(); ++a) {
        for (qsizetype b = a + 1; b < plan.tabs.size(); ++b) {
            QVERIFY(!plan.tabs[a].rect.intersects(plan.tabs[b].rect));
        }
    }
}

void SplitDeckTests::overflowBuildsACardDeckAroundTheActiveTab_data()
{
    QTest::addColumn<TabVisualDirection>("direction");
    QTest::newRow("left-to-right") << TabVisualDirection::LeftToRight;
    QTest::newRow("right-to-left") << TabVisualDirection::RightToLeft;
}

void SplitDeckTests::overflowBuildsACardDeckAroundTheActiveTab()
{
    QFETCH(TabVisualDirection, direction);
    constexpr int active = 6;
    const auto plan = build(splitRequest(900.0, 14, active, ButtonSide::Left, direction));
    QVERIFY(plan.tabsOverflowed);
    const auto &front = plan.tabs[active];
    QCOMPARE(front.deckDepth, 0);
    QVERIFY(front.active);
    // The front card sits in the middle of the deck.
    QVERIFY(qAbs(front.rect.center().x() - plan.tabStrip.center().x()) < Epsilon);
    const bool ltr = direction == TabVisualDirection::LeftToRight;
    for (qsizetype index = 0; index < plan.tabs.size(); ++index) {
        const auto &card = plan.tabs[index];
        const int depth = static_cast<int>(qAbs(index - active));
        QCOMPARE(card.deckDepth, depth);
        QVERIFY(plan.tabStrip.adjusted(-Epsilon, -Epsilon, Epsilon, Epsilon).contains(card.rect));
        QVERIFY(card.rect.width() > 0.0 && card.rect.height() > 0.0);
        QCOMPARE(card.rect.bottom(), plan.outerTitleBar.bottom());
        if (depth == 0) {
            continue;
        }
        // Earlier logical tabs recede to the leading side.
        const bool leading = index < active;
        QVERIFY((card.rect.center().x() < front.rect.center().x()) == (leading == ltr));
        // Each step behind is no larger than the card in front of it, and the
        // nearer card overlaps it.
        const auto &nearer = plan.tabs[index + (index < active ? 1 : -1)];
        QVERIFY(card.rect.width() <= nearer.rect.width() + Epsilon);
        QVERIFY(card.rect.height() <= nearer.rect.height() + Epsilon);
        if (depth <= ChromeSplitDeck::VisibleDepth) {
            QVERIFY(card.rect.intersects(nearer.rect));
            QVERIFY(card.rect.width() < front.rect.width());
        } else {
            // Beyond the visible depth: hidden exactly behind the deepest card.
            const auto &deepest = plan.tabs[active + (index < active ? -1 : 1)
                                            * ChromeSplitDeck::VisibleDepth];
            QCOMPARE(card.rect, deepest.rect);
        }
    }
}

void SplitDeckTests::everyTabStaysInThePlanForAccessibility()
{
    const auto plan = build(splitRequest(700.0, 30, 29));
    QCOMPARE(plan.tabs.size(), 30);
    for (qsizetype index = 0; index < plan.tabs.size(); ++index) {
        QCOMPARE(plan.tabs[index].logicalIndex, index);
        QVERIFY(!plan.tabs[index].rect.isEmpty());
    }
    QVERIFY(plan.tabStrip.isValid());
}

void SplitDeckTests::narrowContainerKeepsButtonsDragAndGap()
{
    // The compositor's 240 px minimum outer frame.
    const auto plan = build(splitRequest(240.0, 6, 2));
    QVERIFY(plan.titleTab.right() - plan.outerTitleBar.left()
            > rowWidth(plan) * ChromeSplitDeck::TitleShare);
    QVERIFY(plan.titleLabelRect.width() >= ChromeSplitDeck::MinimumTitleDragWidth - Epsilon);
    for (const auto &button : plan.buttons) {
        QVERIFY(plan.titleTab.contains(button.rect));
    }
    QVERIFY(plan.deckPiece.left() - plan.titleTab.right()
            >= rowWidth(plan) * ChromeSplitDeck::MinimumGapShare - Epsilon);
    QVERIFY(plan.tabsOverflowed);
}

void SplitDeckTests::tooNarrowRowIsRejected()
{
    QString error;
    QVERIFY(!ChromeLayoutEngine::build(splitRequest(150.0, 3, 0), &error));
    QVERIFY(error.contains(QStringLiteral("split deck")));
}

void SplitDeckTests::hitTestPrefersTheNearestCardAndMarksTheWheel()
{
    constexpr int active = 5;
    const auto plan = build(splitRequest(900.0, 12, active));
    const auto &front = plan.tabs[active];
    const auto &right = plan.tabs[active + 1];
    // In the overlap the front card wins.
    const QRectF overlap = front.rect.intersected(right.rect);
    QVERIFY(overlap.isValid());
    const auto frontHit = ChromeHitTester::hitTest(plan, overlap.center());
    QCOMPARE(frontHit.kind, HitKind::Tab);
    QCOMPARE(frontHit.stableId, front.tabId);
    QVERIFY(frontHit.wheelStepsTabs);
    // The neighbour's exposed edge is the neighbour.
    const QPointF exposed((front.rect.right() + right.rect.right()) / 2.0,
                          right.rect.center().y());
    const auto sideHit = ChromeHitTester::hitTest(plan, exposed);
    QCOMPARE(sideHit.kind, HitKind::Tab);
    QCOMPARE(sideHit.stableId, right.tabId);
    QCOMPARE(sideHit.logicalIndex, active + 1);
    QVERIFY(sideHit.wheelStepsTabs);
    // A classic row never marks tabs for wheel stepping.
    auto classic = splitRequest(1200.0, 3, 0);
    classic.style.titleLayout = ContainerTitleLayout::Classic;
    const auto classicPlan = build(classic);
    const auto classicHit = ChromeHitTester::hitTest(classicPlan,
                                                     classicPlan.tabs[0].rect.center());
    QCOMPARE(classicHit.kind, HitKind::Tab);
    QVERIFY(!classicHit.wheelStepsTabs);
}

void SplitDeckTests::gapIsNotChromeAndHasNoTopResize()
{
    const auto plan = build(splitRequest(1000.0, 3, 0));
    const qreal gapX = (plan.titleTab.right() + plan.deckPiece.left()) / 2.0;
    for (const qreal y : {plan.outerFrame.top() - 3.0, plan.outerFrame.top() + 1.0,
                          plan.outerTitleBar.center().y(), plan.outerTitleBar.bottom() - 1.0}) {
        const auto hit = ChromeHitTester::hitTest(plan, QPointF(gapX, y));
        QVERIFY2(hit.kind == HitKind::None,
                 qPrintable(QStringLiteral("y=%1 hit kind %2").arg(y).arg(int(hit.kind))));
    }
    // The painted pieces keep their top resize edge.
    const auto overTitle = ChromeHitTester::hitTest(
        plan, QPointF(plan.titleLabelRect.center().x(), plan.outerFrame.top() + 1.0));
    QCOMPARE(overTitle.kind, HitKind::OuterResize);
    QVERIFY(overTitle.resizeEdges.testFlag(Qt::TopEdge));
    const auto overDeck = ChromeHitTester::hitTest(
        plan, QPointF(plan.tabs[1].rect.center().x(), plan.outerFrame.top() + 1.0));
    QCOMPARE(overDeck.kind, HitKind::OuterResize);
    QVERIFY(overDeck.resizeEdges.testFlag(Qt::TopEdge));
    // Below the row the member content is unchanged.
    QCOMPARE(ChromeHitTester::hitTest(plan, QPointF(gapX, plan.outerTitleBar.bottom() + 40.0))
                 .kind,
             HitKind::Client);
}

void SplitDeckTests::titleTabDragsAndDeckSurfaceStepsTheWheel()
{
    const auto plan = build(splitRequest(1000.0, 2, 0));
    const auto name = ChromeHitTester::hitTest(plan, plan.titleLabelRect.center());
    QCOMPARE(name.kind, HitKind::OuterTitleDrag);
    QVERIFY(!name.wheelStepsTabs);
    const auto button = ChromeHitTester::hitTest(plan, plan.buttons[0].rect.center());
    QCOMPARE(button.kind, HitKind::WindowButton);
    // Deck padding left of the first tab: still the container, wheel steps.
    const QPointF padding(plan.deckPiece.left() + 2.0, plan.outerTitleBar.center().y());
    const auto surface = ChromeHitTester::hitTest(plan, padding);
    QCOMPARE(surface.kind, HitKind::OuterTitleDrag);
    QVERIFY(surface.wheelStepsTabs);
    const auto control = ChromeHitTester::hitTest(plan, plan.controls[0].rect.center());
    QCOMPARE(control.kind, HitKind::ContainerControl);
}

void SplitDeckTests::keyboardChipSitsBesideTheName()
{
    auto request = splitRequest(1000.0, 3, 0);
    request.containerTitleWidth = 120.0;
    request.indexBadge = 4;
    const auto plan = build(request);
    QVERIFY(plan.indexBadgeRect.isValid());
    QVERIFY(plan.titleTab.contains(plan.indexBadgeRect));
    QVERIFY(plan.indexBadgeRect.left() > plan.buttons.constLast().rect.right());
    QVERIFY(plan.titleLabelRect.left() > plan.indexBadgeRect.right());
}

void SplitDeckTests::wheelStepsStopAtEitherEnd()
{
    const auto middle = build(splitRequest(1000.0, 4, 1));
    QCOMPARE(ChromeSplitDeck::steppedTab(middle, -1), std::optional<qsizetype>(0));
    QCOMPARE(ChromeSplitDeck::steppedTab(middle, 1), std::optional<qsizetype>(2));
    QCOMPARE(ChromeSplitDeck::steppedTab(middle, 0), std::nullopt);
    const auto first = build(splitRequest(1000.0, 4, 0));
    QCOMPARE(ChromeSplitDeck::steppedTab(first, -1), std::nullopt);
    const auto last = build(splitRequest(1000.0, 4, 3));
    QCOMPARE(ChromeSplitDeck::steppedTab(last, 1), std::nullopt);
    QCOMPARE(ChromeSplitDeck::steppedTab(last, -5), std::optional<qsizetype>(2));
}

void SplitDeckTests::rendererLeavesTheGapTransparentAndPaintsTheTab()
{
    const auto plan = build(splitRequest(1000.0, 3, 0));
    const auto image = render(plan);
    const qreal gapX = (plan.titleTab.right() + plan.deckPiece.left()) / 2.0;
    for (const qreal y : {plan.outerFrame.top() + 1.0, plan.outerTitleBar.center().y(),
                          plan.outerTitleBar.bottom() - 2.0}) {
        QCOMPARE(pixel(image, plan, QPointF(gapX, y)).alpha(), 0);
    }
    // The title tab wears the theme's signature color (between the stripe
    // and the name, left of the label).
    const QPointF tabPoint(plan.titleLabelRect.right() - 1.0, plan.outerTitleBar.bottom() - 3.0);
    QCOMPARE(pixel(image, plan, tabPoint).name(), QStringLiteral("#f6c02b"));
    // The body below the row is still painted.
    QVERIFY(pixel(image, plan, QPointF(gapX, plan.outerFrame.bottom() - 0.5)).alpha() > 0);
    // Name glyphs contrast with the tab: some dark ink inside the label.
    int ink = 0;
    for (qreal x = plan.titleLabelRect.left(); x < plan.titleLabelRect.right(); x += 1.0) {
        for (qreal y = plan.outerTitleBar.top() + 4.0; y < plan.outerTitleBar.bottom() - 4.0;
             y += 1.0) {
            ink += pixel(image, plan, QPointF(x, y)).lightness() < 110 ? 1 : 0;
        }
    }
    QVERIFY2(ink > 20, qPrintable(QString::number(ink)));
}

void SplitDeckTests::frontCardPaintsOverItsNeighbours()
{
    constexpr int active = 5;
    const auto plan = build(splitRequest(900.0, 12, active));
    const auto image = render(plan);
    const auto &front = plan.tabs[active];
    const QRectF overlap = front.rect.intersected(plan.tabs[active - 1].rect);
    // Sample near the top of the overlap, clear of the title text.
    const QPointF sample(overlap.center().x(), front.rect.top() + 3.0);
    QCOMPARE(pixel(image, plan, sample).name(), plan.identity.tabTint.name());
}

void SplitDeckTests::unfocusedTabUsesTheInactiveColor()
{
    auto request = splitRequest(1000.0, 3, 0);
    request.containerFocused = false;
    const auto plan = build(request);
    const auto image = render(plan);
    const QPointF tabPoint(plan.titleLabelRect.right() - 1.0, plan.outerTitleBar.bottom() - 3.0);
    QCOMPARE(pixel(image, plan, tabPoint).name(), QStringLiteral("#ddd8cb"));
}

void SplitDeckTests::glideInterpolatesOnlyTabRects()
{
    const auto from = build(splitRequest(900.0, 12, 5));
    const auto to = build(splitRequest(900.0, 12, 6));
    QVERIFY(ChromeSplitDeck::canAnimate(from, to));
    QVERIFY(!ChromeSplitDeck::canAnimate(to, to));
    const auto half = ChromeSplitDeck::interpolate(from, to, 0.5);
    for (qsizetype index = 0; index < half.tabs.size(); ++index) {
        const QRectF &a = from.tabs[index].rect;
        const QRectF &b = to.tabs[index].rect;
        QVERIFY(qAbs(half.tabs[index].rect.left() - (a.left() + b.left()) / 2.0) < Epsilon);
        QVERIFY(qAbs(half.tabs[index].rect.width() - (a.width() + b.width()) / 2.0) < Epsilon);
        QCOMPARE(half.tabs[index].deckDepth, to.tabs[index].deckDepth);
        QCOMPARE(half.tabs[index].active, to.tabs[index].active);
    }
    QCOMPARE(half.buttons.constFirst().rect, to.buttons.constFirst().rect);
    const auto end = ChromeSplitDeck::interpolate(from, to, 7.0);
    QCOMPARE(end.tabs[3].rect, to.tabs[3].rect);
    const auto start = ChromeSplitDeck::interpolate(from, to, -1.0);
    QCOMPARE(start.tabs[3].rect, from.tabs[3].rect);
    // A different tab set, or a resized frame, never glides.
    QVERIFY(!ChromeSplitDeck::canAnimate(build(splitRequest(900.0, 11, 5)), to));
    QVERIFY(!ChromeSplitDeck::canAnimate(build(splitRequest(950.0, 12, 5)), to));
}

void SplitDeckTests::classicLayoutIsUnchanged()
{
    auto request = splitRequest(1000.0, 3, 0);
    request.style.titleLayout = ContainerTitleLayout::Classic;
    const auto plan = build(request);
    QVERIFY(plan.titleTab.isNull());
    QVERIFY(plan.deckPiece.isNull());
    QCOMPARE(plan.tabStrip, plan.outerTitleBar);
    // The classic row is painted edge to edge.
    const auto image = render(plan);
    QVERIFY(pixel(image, plan, QPointF(plan.outerFrame.center().x(),
                                       plan.outerTitleBar.center().y()))
                .alpha()
            > 0);
}

// Every Corner Bar treatment, light and dark, renders a roomy row and an
// overflowing card deck from its own theme colors. With
// QINDAQT_SPLITDECK_EVIDENCE_DIR set, the frames are saved over a checkered
// backdrop (so the transparent gap shows) for visual review; they are
// offscreen renderer output, not a live-session capture.
void SplitDeckTests::cornerBarThemesRenderEveryDeckState()
{
    const QString evidence = qEnvironmentVariable("QINDAQT_SPLITDECK_EVIDENCE_DIR");
    for (const auto *themeId : {"qinda-marigold", "qinda-marigold-dark", "qinda-corner-teal",
                                "qinda-corner-teal-dark", "qinda-corner-violet",
                                "qinda-corner-violet-dark"}) {
        QFile file(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/%1.json")
                       .arg(QString::fromLatin1(themeId)));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        const auto colors = root.value(QStringLiteral("colors")).toObject();
        const auto decoration = root.value(QStringLiteral("decoration")).toObject();
        QCOMPARE(decoration.value(QStringLiteral("containerTitleLayout")).toString(),
                 QStringLiteral("split-deck"));
        const auto color = [](const QJsonObject &object, const char *name) {
            return QColor(object.value(QString::fromLatin1(name)).toString());
        };
        for (const auto &[tabs, active, focused] :
             {std::tuple{3, 1, true}, std::tuple{16, 7, true}, std::tuple{16, 7, false}}) {
            auto request = splitRequest(1100.0, tabs, active);
            request.containerFocused = focused;
            auto &palette = request.style.palette;
            palette.surface = color(colors, "surface");
            palette.surfaceRaised = color(colors, "surfaceRaised");
            palette.border = color(colors, "border");
            palette.text = color(colors, "text");
            palette.textMuted = color(colors, "textMuted");
            palette.accent = color(colors, "accent");
            palette.close = color(decoration, "closeColor");
            palette.minimize = color(decoration, "minimizeColor");
            palette.maximize = color(decoration, "maximizeColor");
            palette.titleBar = color(decoration, "titleBarColor");
            palette.titleBarInactive = color(decoration, "titleBarInactiveColor");
            request.style.buttonStyle = ButtonStyle::TrafficLights;
            request.metrics.cornerRadius =
                root.value(QStringLiteral("radii")).toObject().value(QStringLiteral("decoration"))
                    .toInt(3);
            request.outerRect.setHeight(140.0);
            request.members[0].windowRect.setHeight(request.outerRect.bottom() - 1.0
                                                    - request.members[0].windowRect.top());
            const auto plan = build(request);
            QCOMPARE(plan.tabsOverflowed, tabs > 3);
            const auto image = render(plan);
            if (evidence.isEmpty()) {
                continue;
            }
            QImage backdrop(image.size(), QImage::Format_ARGB32_Premultiplied);
            QPainter painter(&backdrop);
            for (int y = 0; y < backdrop.height(); y += 8) {
                for (int x = 0; x < backdrop.width(); x += 8) {
                    painter.fillRect(x, y, 8, 8,
                                     ((x + y) / 8) % 2 ? QColor(0x9a, 0xa4, 0xb0)
                                                       : QColor(0xc8, 0xd0, 0xd8));
                }
            }
            painter.drawImage(0, 0, image);
            painter.end();
            QVERIFY(QDir().mkpath(evidence));
            QVERIFY(backdrop.save(QDir(evidence).filePath(
                QStringLiteral("%1-%2tabs-%3.png")
                    .arg(QString::fromLatin1(themeId))
                    .arg(tabs)
                    .arg(focused ? QStringLiteral("focused") : QStringLiteral("unfocused")))));
        }
    }
}

QTEST_MAIN(SplitDeckTests)
#include "tst_splitdeck.moc"
