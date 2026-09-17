// SPDX-License-Identifier: LGPL-3.0-or-later
#include "hybridchromepointerrouter.h"
#include "hybridshadestripgeometry.h"
#include "qindaqt/hybrid_chrome/chromehittest.h"
#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_input/interactiontypes.h"

#include <QPointF>
#include <QtMath>
#include <QtTest>

#include <optional>

using namespace QindaQt::Compositor::KWinIntegration;
using namespace QindaQt::HybridChrome;

namespace {

constexpr qreal TestDoubleClickIntervalMs = 30.0;

qreal measuredLabelWidth(const QString &label)
{
    return ChromeShadedBadge::labelWidthFor(
        label, QFontMetricsF(ChromeShadedBadge::labelFont()));
}

ChromeRenderPlan shadedBadgePlan(qsizetype tabCount, qreal widthOverride = 0.0)
{
    ChromeLayoutRequest request;
    request.containerId = QStringLiteral("container-shaded");
    request.shaded = true;
    request.containerFocused = true;
    request.identityColor = QColor(QStringLiteral("#b65447"));
    request.containerTitle = QStringLiteral("Project 102b");
    for (qsizetype index = 0; index < tabCount; ++index) {
        request.tabs.append({QStringLiteral("page-%1").arg(index),
                             QStringLiteral("Tab %1").arg(index),
                             index == 0});
    }
    // ADR-0189: the caller resolves and measures the label, then sizes the
    // strip for that measurement.
    request.badgeLabelText = ChromeShadedBadge::resolveLabel(
        request.containerTitle, false,
        request.tabs.isEmpty() ? QString() : request.tabs.constFirst().title);
    request.badgeLabelWidth = measuredLabelWidth(request.badgeLabelText);
    // Size the badge to its content-driven strip width unless the caller
    // overrides (for the degradation case).
    const qreal width = widthOverride > 0.0
        ? widthOverride
        : ChromeShadedBadge::badgeWidth(ChromeMetrics{}, tabCount,
                                        request.badgeLabelWidth)
            + 2.0 * ChromeMetrics{}.outerBorder;
    request.outerRect = QRectF(120.0, 90.0, width, 31.0);
    auto plan = ChromeLayoutEngine::build(request);
    if (!plan) {
        qFatal("shaded badge fixture plan failed to build");
    }
    return *plan;
}

// A resolver over one static hit map, so router tests need no compositor.
class FixedResolver
{
public:
    explicit FixedResolver(ChromePointerHit hit)
        : m_hit(std::move(hit))
    {
    }

    [[nodiscard]] std::optional<ChromePointerHit> at(const QPointF &) const
    {
        return m_hit;
    }

private:
    ChromePointerHit m_hit;
};

} // namespace

class ContainerChromeBadgeTests final : public QObject
{
    Q_OBJECT

private slots:
    void stripWidthGrowsWithTabsAndStaysInsideItsFrame()
    {
        const QRect frame(100, 100, 800, 600);
        const qreal label = ChromeShadedBadge::LabelMinimumWidth;
        int previous = 0;
        for (const auto tabs : {qsizetype{1}, qsizetype{2}, qsizetype{8}, qsizetype{12}}) {
            const auto width = HybridShadeStripGeometry::stripWidth(tabs, frame, label);
            QVERIFY(width > 0);
            QVERIFY(width <= frame.width());
            QVERIFY(width > previous);
            previous = width;
        }
        // Anchored inside a narrow frame, the strip never exceeds it.
        QCOMPARE(HybridShadeStripGeometry::stripWidth(12, QRect(0, 0, 40, 30), label),
                 40);
    }

    // ADR-0189: the user-visible outcome. A rolled-up container with a long
    // page title gets a wider strip, so the title is readable instead of
    // elided into a fixed 140 px label.
    void stripWidthGrowsWithTheBadgeLabel()
    {
        const QRect frame(100, 100, 1600, 900);
        const qreal shortLabel = measuredLabelWidth(QStringLiteral("Inbox"));
        const qreal longLabel = measuredLabelWidth(
            QStringLiteral("Quarterly revenue model, revision 12"));
        QVERIFY(longLabel > shortLabel);

        const int narrow = HybridShadeStripGeometry::stripWidth(3, frame, shortLabel);
        const int wide = HybridShadeStripGeometry::stripWidth(3, frame, longLabel);
        QVERIFY2(wide > narrow,
                 qPrintable(QStringLiteral("strip did not grow: %1 vs %2")
                                .arg(narrow)
                                .arg(wide)));
        // The growth is exactly the label growth, not a guess.
        QCOMPARE(wide - narrow, qCeil(longLabel) - qCeil(shortLabel));

        // Clamped both ways, and still never wider than its own frame.
        QCOMPARE(HybridShadeStripGeometry::stripWidth(3, frame, 0.0),
                 HybridShadeStripGeometry::stripWidth(
                     3, frame, ChromeShadedBadge::LabelMinimumWidth));
        QCOMPARE(HybridShadeStripGeometry::stripWidth(3, frame, 100000.0),
                 HybridShadeStripGeometry::stripWidth(
                     3, frame, ChromeShadedBadge::LabelMaximumWidth));
        QCOMPARE(HybridShadeStripGeometry::stripWidth(3, QRect(0, 0, 60, 30),
                                                      longLabel),
                 60);
    }

    void badgeLayoutHoldsForOneTwoEightAndTwelveTabs()
    {
        for (const auto tabs : {qsizetype{1}, qsizetype{2}, qsizetype{8}, qsizetype{12}}) {
            const auto plan = shadedBadgePlan(tabs);
            QVERIFY(plan.shaded);
            QVERIFY(plan.buttons.isEmpty());
            QCOMPARE(plan.controls.size(), 3);
            const qsizetype expectedPills = qMin(tabs, ChromeShadedBadge::MaxPills);
            QCOMPARE(plan.tabs.size(), expectedPills);
            QCOMPARE(plan.badgeOverflowCount, int(tabs - expectedPills));
            QCOMPARE(plan.tabsOverflowed, tabs > ChromeShadedBadge::MaxPills);

            // Pills are disjoint, inside the badge, and keep request order.
            for (qsizetype index = 0; index < plan.tabs.size(); ++index) {
                const auto &pill = plan.tabs.at(index);
                QCOMPARE(pill.tabId, QStringLiteral("page-%1").arg(index));
                QVERIFY(plan.outerFrame.contains(pill.rect));
                QVERIFY(pill.rect.isValid());
                if (index > 0) {
                    QVERIFY(plan.tabs.at(index - 1).rect.right()
                            < pill.rect.left());
                }
            }
            // Controls and the label live inside the badge and do not
            // overlap the pills.
            for (const auto &control : plan.controls) {
                QVERIFY(plan.outerFrame.contains(control.rect));
                for (const auto &pill : plan.tabs) {
                    QVERIFY(!control.rect.intersects(pill.rect));
                }
            }
            QVERIFY(plan.badgeLabelRect.isValid());
            QVERIFY(plan.badgeLabelRect.width() >= 48.0);
            QVERIFY(plan.outerFrame.contains(plan.badgeLabelRect));
            // The whole badge body is the drag region; controls and pills
            // win by hit priority.
            QCOMPARE(plan.outerTitleDragRect, plan.outerTitleBar);
        }

        // A strip narrower than promised degrades gracefully: pills fold
        // into the overflow counter, the label never vanishes, and nothing
        // leaves the badge.
        const auto degraded = shadedBadgePlan(12, 300.0);
        QCOMPARE(degraded.tabs.size() + qsizetype(degraded.badgeOverflowCount), qsizetype{12});
        QVERIFY(degraded.badgeLabelRect.width() >= 48.0);
        for (const auto &pill : degraded.tabs) {
            QVERIFY(degraded.outerFrame.contains(pill.rect));
        }
    }

    void badgeHitsCarryTheShadedBadgeFlagAndSkipResize()
    {
        const auto plan = shadedBadgePlan(3);
        const auto pill = ChromeHitTester::hitTest(plan, plan.tabs.at(1).rect.center());
        QCOMPARE(pill.kind, HitKind::Tab);
        QCOMPARE(pill.stableId, QStringLiteral("page-1"));
        QVERIFY(pill.fromShadedBadge);

        const auto control = ChromeHitTester::hitTest(plan, plan.controls.at(2).rect.center());
        QCOMPARE(control.kind, HitKind::ContainerControl);
        QVERIFY(control.containerControl);
        QCOMPARE(*control.containerControl, ContainerControl::ManagementMenu);

        const auto dragPoint = QPointF(plan.outerFrame.center().x(),
                                       plan.badgeLabelRect.center().y());
        const auto drag = ChromeHitTester::hitTest(plan, dragPoint);
        QCOMPARE(drag.kind, HitKind::OuterTitleDrag);
        QVERIFY(drag.fromShadedBadge);

        // No resize affordance exists around a rolled-up badge.
        const auto resizeProbe = ChromeHitTester::hitTest(
            plan, QPointF(plan.outerFrame.left() - 3.0, plan.outerFrame.top() - 3.0));
        QCOMPARE(resizeProbe.kind, HitKind::None);
    }

    void pillClickActivatesAndRequestsUnroll()
    {
        const auto plan = shadedBadgePlan(3);
        FixedResolver fixed({plan.containerId,
                             {HitKind::Tab, QStringLiteral("page-2"), 2,
                              std::nullopt, {}, std::nullopt, true}});
        HybridChromePointerRouter router(
            [&fixed](const QPointF &position) { return fixed.at(position); },
            8.0, TestDoubleClickIntervalMs);

        const QPointF position(0.0, 0.0);
        auto decision = router.pointerPress({position, Qt::LeftButton, Qt::LeftButton,
                                             Qt::NoModifier});
        QVERIFY(decision.consumed);
        decision = router.pointerRelease({position, Qt::LeftButton, Qt::NoButton,
                                          Qt::NoModifier});
        QVERIFY(decision.consumed);
        QCOMPARE(decision.activations.size(), 1);
        QCOMPARE(decision.activations.constFirst().target.stableId,
                 QStringLiteral("page-2"));
        // ADR-0139: the pill click also unrolls to that tab.
        QCOMPARE(decision.shadeRequests.size(), 1);
        QVERIFY(!decision.shadeRequests.constFirst().shade);
        QCOMPARE(decision.shadeRequests.constFirst().containerId,
                 plan.containerId);
    }

    void doubleClickOnBadgeBodyRequestsUnroll()
    {
        const auto plan = shadedBadgePlan(3);
        FixedResolver fixed({plan.containerId,
                             {HitKind::OuterTitleDrag, plan.containerId, -1,
                              std::nullopt, {}, std::nullopt, true}});
        HybridChromePointerRouter router(
            [&fixed](const QPointF &position) { return fixed.at(position); },
            8.0, TestDoubleClickIntervalMs);
        const QPointF position(0.0, 0.0);
        const auto press = [&router, &position](Qt::MouseButtons buttons) {
            return router.pointerPress({position, Qt::LeftButton, buttons,
                                        Qt::NoModifier});
        };
        const auto release = [&router, &position]() {
            return router.pointerRelease({position, Qt::LeftButton, Qt::NoButton,
                                          Qt::NoModifier});
        };

        // First click: consumed raise only, no unroll request.
        QVERIFY(press(Qt::LeftButton).consumed);
        auto first = release();
        QVERIFY(first.consumed);
        QCOMPARE(first.shadeRequests.size(), 0);

        // Second click inside the interval: unroll request, no activation.
        QVERIFY(press(Qt::LeftButton).consumed);
        auto second = release();
        QVERIFY(second.consumed);
        QCOMPARE(second.activations.size(), 0);
        QCOMPARE(second.shadeRequests.size(), 1);
        QVERIFY(!second.shadeRequests.constFirst().shade);

        // A third click after the interval is a fresh single click again.
        QTest::qWait(int(TestDoubleClickIntervalMs) + 20);
        QVERIFY(press(Qt::LeftButton).consumed);
        auto third = release();
        QCOMPARE(third.shadeRequests.size(), 0);
    }
};

QTEST_MAIN(ContainerChromeBadgeTests)
#include "tst_container_chrome_badge.moc"
