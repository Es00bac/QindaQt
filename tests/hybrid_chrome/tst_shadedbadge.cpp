// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_chrome/chromeidentity.h"
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_chrome/chromerenderer.h"
#include "qindaqt/hybrid_chrome/chromeshadedbadge.h"

#include <QImage>
#include <QPainter>
#include <QtTest>

#include <cmath>

using namespace QindaQt::HybridChrome;

namespace {

ChromeRenderPlan badgePlan(qsizetype tabCount)
{
    ChromeLayoutRequest request;
    request.containerId = QStringLiteral("container-shaded");
    request.outerRect = QRectF(0.0, 0.0,
                               ChromeShadedBadge::badgeWidth(ChromeMetrics{}, tabCount)
                                   + 2.0 * ChromeMetrics{}.outerBorder,
                               31.0);
    request.devicePixelRatio = 2.0;
    request.shaded = true;
    request.containerFocused = true;
    request.identityColor = QColor(QStringLiteral("#b65447"));
    request.containerTitle = QStringLiteral("Project 102b");
    for (qsizetype index = 0; index < tabCount; ++index) {
        request.tabs.append({QStringLiteral("page-%1").arg(index),
                             QStringLiteral("Tab %1").arg(index),
                             index == 0});
    }
    auto plan = ChromeLayoutEngine::build(request);
    if (!plan) {
        qFatal("badge fixture failed");
    }
    return *plan;
}

QImage render(const ChromeRenderPlan &plan)
{
    const auto physicalSize = QSize(qCeil(plan.outerFrame.width() * plan.devicePixelRatio),
                                    qCeil(plan.outerFrame.height() * plan.devicePixelRatio));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(plan.devicePixelRatio);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    ChromeRenderer::paint(painter, plan);
    return image;
}

QPoint physicalPoint(const QPointF &logical, qreal dpr)
{
    return {qRound(logical.x() * dpr), qRound(logical.y() * dpr)};
}

} // namespace

class ShadedBadgePaintTests final : public QObject
{
    Q_OBJECT

private slots:
    void badgePaintsIdentityFrameLabelAndPills()
    {
        const auto plan = badgePlan(3);
        const auto image = render(plan);
        const qreal dpr = plan.devicePixelRatio;

        // The 2 px full-strength identity frame on the left edge.
        const auto frameSample = physicalPoint(QPointF(0.5, plan.outerFrame.center().y()), dpr);
        QVERIFY(qAbs(image.pixelColor(frameSample).red() - plan.identity.border.red()) <= 24
                && qAbs(image.pixelColor(frameSample).green() - plan.identity.border.green()) <= 24
                && qAbs(image.pixelColor(frameSample).blue() - plan.identity.border.blue()) <= 24);

        // The label paints identity ink inside its rect (text pixels differ
        // from the plain surface fill behind them).
        bool labelInkFound = false;
        const auto labelTopLeft = physicalPoint(plan.badgeLabelRect.topLeft()
                                                + QPointF(6.0, 6.0), dpr);
        const auto surface = plan.style.palette.surface;
        for (int dy = 0; dy < 16 && !labelInkFound; ++dy) {
            for (int dx = 0; dx < 160 && !labelInkFound; ++dx) {
                const auto pixel = image.pixelColor(labelTopLeft + QPoint(dx, dy));
                labelInkFound = pixel.alpha() > 0
                    && (qAbs(pixel.red() - surface.red()) > 24
                        || qAbs(pixel.green() - surface.green()) > 24
                        || qAbs(pixel.blue() - surface.blue()) > 24);
            }
        }
        QVERIFY(labelInkFound);

        // The active pill paints its (strongest) tint; inactive pills paint
        // a lighter tint of the same identity color.
        const auto active = std::find_if(plan.tabs.cbegin(), plan.tabs.cend(),
                                         [](const TabGeometry &pill) { return pill.active; });
        QVERIFY(active != plan.tabs.cend());
        const auto activeTint = identityPillTint(plan.identity.base,
                                                 plan.style.palette.surface, 0, true);
        const auto activeSample = physicalPoint(active->rect.center(), dpr);
        QCOMPARE(image.pixelColor(activeSample).rgba(), activeTint.rgba());
        const auto &inactive = plan.tabs.at(1);
        const auto inactiveTint = identityPillTint(plan.identity.base,
                                                   plan.style.palette.surface, 1, false);
        const auto inactiveSample = physicalPoint(inactive.rect.center(), dpr);
        QCOMPARE(image.pixelColor(inactiveSample).rgba(), inactiveTint.rgba());
        QVERIFY(inactiveTint != activeTint);
    }

    void badgeLabelPaintsWithGeneratedNameAndWithoutAnyTitle()
    {
        // ADR-0163 regression guard: a rolled-up badge must never paint an
        // empty label area. The session passes the generated "Container N"
        // name for never-renamed containers; a badge without any title still
        // paints the active tab's title as the fallback.
        const auto surface = ChromePalette{}.surface;
        for (const auto &containerTitle : {QStringLiteral("Container 1"), QString()}) {
            ChromeLayoutRequest request;
            request.containerId = QStringLiteral("container-shaded");
            request.outerRect = QRectF(0.0, 0.0,
                                       ChromeShadedBadge::badgeWidth(ChromeMetrics{}, 1)
                                           + 2.0 * ChromeMetrics{}.outerBorder,
                                       31.0);
            request.shaded = true;
            request.containerTitle = containerTitle;
            request.tabs = {{QStringLiteral("page-a"),
                             QStringLiteral("SuperTuxKart"), true}};
            const auto plan = ChromeLayoutEngine::build(request);
            QVERIFY(plan);
            QVERIFY(plan->badgeLabelRect.isValid());
            QVERIFY(plan->badgeLabelRect.width() >= 48.0);

            const auto image = render(*plan);
            const auto labelTopLeft = physicalPoint(
                plan->badgeLabelRect.topLeft() + QPointF(5.0, 8.0),
                plan->devicePixelRatio);
            bool labelInkFound = false;
            for (int dy = 0; dy < 16 && !labelInkFound; ++dy) {
                for (int dx = 0; dx < 120 && !labelInkFound; ++dx) {
                    const auto pixel = image.pixelColor(labelTopLeft + QPoint(dx, dy));
                    labelInkFound = pixel.alpha() > 0
                        && (qAbs(pixel.red() - surface.red()) > 24
                            || qAbs(pixel.green() - surface.green()) > 24
                            || qAbs(pixel.blue() - surface.blue()) > 24);
                }
            }
            QVERIFY2(labelInkFound, qPrintable(containerTitle.isEmpty()
                ? QStringLiteral("empty-title fallback label did not paint")
                : QStringLiteral("generated-name label did not paint")));
        }
    }

    void badgeUnsetIdentityUsesAccentDerivation()
    {
        // Negative control: an unset identity color drives the whole badge
        // from the theme accent, exactly like the unshaded chrome.
        ChromeLayoutRequest request;
        request.containerId = QStringLiteral("container-shaded");
        request.outerRect = QRectF(0.0, 0.0, 300.0, 31.0);
        request.shaded = true;
        request.tabs = {{QStringLiteral("page-a"), QStringLiteral("Alpha"), true}};
        const auto plan = ChromeLayoutEngine::build(request);
        QVERIFY(plan);
        QCOMPARE(plan->identity.base, plan->style.palette.accent);
        QCOMPARE(plan->identityColor, QColor());
        QVERIFY(plan->tabs.size() == 1);
        QVERIFY(plan->tabs.constFirst().rect.width() > 0.0);
    }
};

QTEST_MAIN(ShadedBadgePaintTests)
#include "tst_shadedbadge.moc"
