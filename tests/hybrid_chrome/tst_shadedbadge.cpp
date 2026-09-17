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

// The label the badge paints, taken from the plan the caller resolved
// (ADR-0189). Asserting the text keeps the intent legible where a pixel probe
// would only say "some ink appeared".
QString badgeLabelText(const ChromeRenderPlan &plan)
{
    return plan.badgeLabelText;
}

// Mirrors ChromeShadedBadge's own label text inset; the badge keeps it
// private, so the probes restate it rather than guessing.
constexpr qreal BadgeLabelTextInsetForTests = 4.0;

QFontMetricsF labelMetrics()
{
    return QFontMetricsF(ChromeShadedBadge::labelFont());
}

// Exactly what HybridChromePlanBuilder::shadedLabel does, so a fixture cannot
// drift from production: resolve the label from the request's own titles, then
// measure it (ADR-0189).
void applyResolvedLabel(ChromeLayoutRequest &request)
{
    QString foremost;
    for (const auto &tab : request.tabs) {
        if (tab.active) {
            foremost = tab.title;
            break;
        }
    }
    if (foremost.isEmpty() && !request.tabs.isEmpty()) {
        foremost = request.tabs.constFirst().title;
    }
    request.badgeLabelText = ChromeShadedBadge::resolveLabel(
        request.containerTitle, request.containerTitleIsGenerated, foremost);
    request.badgeLabelWidth =
        ChromeShadedBadge::labelWidthFor(request.badgeLabelText, labelMetrics());
}

// Mirrors production: resolve the label, measure it, size the strip for that
// measurement, and carry both onto the request (ADR-0189).
ChromeRenderPlan badgePlanWithTitles(qsizetype tabCount,
                                     const QString &containerTitle,
                                     bool containerTitleIsGenerated,
                                     const QString &firstTabTitle,
                                     qreal widthOverride = 0.0)
{
    ChromeLayoutRequest request;
    request.containerId = QStringLiteral("container-shaded");
    request.devicePixelRatio = 2.0;
    request.shaded = true;
    request.containerFocused = true;
    request.identityColor = QColor(QStringLiteral("#b65447"));
    request.containerTitle = containerTitle;
    request.containerTitleIsGenerated = containerTitleIsGenerated;
    for (qsizetype index = 0; index < tabCount; ++index) {
        request.tabs.append(
            {QStringLiteral("page-%1").arg(index),
             index == 0 && !firstTabTitle.isEmpty()
                 ? firstTabTitle
                 : QStringLiteral("Tab %1").arg(index),
             index == 0});
    }
    applyResolvedLabel(request);
    const qreal outerWidth = widthOverride > 0.0
        ? widthOverride
        : ChromeShadedBadge::badgeWidth(ChromeMetrics{}, tabCount,
                                        request.badgeLabelWidth)
            + 2.0 * ChromeMetrics{}.outerBorder;
    request.outerRect = QRectF(0.0, 0.0, outerWidth, 31.0);
    auto plan = ChromeLayoutEngine::build(request);
    if (!plan) {
        qFatal("badge fixture failed");
    }
    return *plan;
}

ChromeRenderPlan badgePlan(qsizetype tabCount)
{
    return badgePlanWithTitles(tabCount, QStringLiteral("Project 102b"), false, {});
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

// Glyph ink inside the label rect only.
//
// AGENT-GUARD: bound *both* axes to the label rect, inset past the identity
// frame. ChromeRenderer paints a 2 px full-perimeter frame in
// identity.border on shaded plans too, and that colour differs from the badge
// surface by far more than any threshold — so a y range of the whole image
// makes a blank label pass on border pixels alone. Bounding x matters for the
// same reason: past the label rect sits the pill cluster, whose active tint
// also clears the threshold. Both mistakes were made here and caught in
// review.
int labelInkPixels(const QImage &image, const ChromeRenderPlan &plan,
                   qreal fromOffset = 0.0)
{
    const auto surface = plan.style.palette.surface;
    const QRectF band = plan.badgeLabelRect.adjusted(
        qMax(BadgeLabelTextInsetForTests, fromOffset), 3.0,
        -BadgeLabelTextInsetForTests, -3.0);
    if (!band.isValid() || band.isEmpty()) {
        return 0;
    }
    int found = 0;
    const int left = qRound(band.left() * plan.devicePixelRatio);
    const int right = qRound(band.right() * plan.devicePixelRatio);
    const int top = qRound(band.top() * plan.devicePixelRatio);
    const int bottom = qRound(band.bottom() * plan.devicePixelRatio);
    for (int x = qMax(0, left); x <= qMin(image.width() - 1, right); ++x) {
        for (int y = qMax(0, top); y <= qMin(image.height() - 1, bottom); ++y) {
            const auto pixel = image.pixelColor(x, y);
            if (pixel.alpha() > 0
                && (qAbs(pixel.red() - surface.red()) > 24
                    || qAbs(pixel.green() - surface.green()) > 24
                    || qAbs(pixel.blue() - surface.blue()) > 24)) {
                ++found;
            }
        }
    }
    return found;
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
        // from the plain surface fill behind them). Bounded to the rect: the
        // old window ran 160 px from the rect's top-left, past its right edge
        // and into the pill cluster, whose tint also clears the threshold.
        QVERIFY2(labelInkPixels(image, plan) > 20,
                 qPrintable(QStringLiteral("label rect %1 wide painted %2 glyph pixels")
                                .arg(plan.badgeLabelRect.width())
                                .arg(labelInkPixels(image, plan))));

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
        for (const auto &containerTitle : {QStringLiteral("Container 1"), QString()}) {
            ChromeLayoutRequest request;
            request.containerId = QStringLiteral("container-shaded");
            request.shaded = true;
            request.containerTitle = containerTitle;
            request.tabs = {{QStringLiteral("page-a"),
                             QStringLiteral("SuperTuxKart"), true}};
            applyResolvedLabel(request);
            request.outerRect = QRectF(
                0.0, 0.0,
                ChromeShadedBadge::badgeWidth(ChromeMetrics{}, 1,
                                              request.badgeLabelWidth)
                    + 2.0 * ChromeMetrics{}.outerBorder,
                31.0);
            const auto plan = ChromeLayoutEngine::build(request);
            QVERIFY(plan);
            QVERIFY(plan->badgeLabelRect.isValid());
            QVERIFY(plan->badgeLabelRect.width() >= 48.0);

            const auto image = render(*plan);
            // Bounded to the label rect: a 120 px window from its top-left ran
            // past the rect into the pill cluster, so this row passed on pill
            // tint even with no text.
            const int ink = labelInkPixels(image, *plan);
            QVERIFY2(ink > 20, qPrintable((containerTitle.isEmpty()
                ? QStringLiteral("empty-title fallback label painted %1 glyph pixels")
                : QStringLiteral("generated-name label painted %1 glyph pixels"))
                    .arg(ink)));
        }
    }

    // ADR-0168 regression for the reported "name shows on the container window,
    // disappears when rolled up, unrolling brings it back". ADR-0163 made the
    // session pass a GENERATED "Container N" whenever no rename existed, and
    // the badge prefixed it unconditionally. The label rect was 48-140 px, so
    // "Container 7 \u00B7 " consumed most of it and elided the page title - the
    // only text the user recognised - out of the badge. A generated placeholder
    // must never displace real text.
    void generatedNameNeverDisplacesTheRealTitle()
    {
        const QString pageTitle = QStringLiteral("Quarterly Planning Notes");

        ChromeLayoutRequest generated;
        generated.containerId = QStringLiteral("container-shaded");
        generated.shaded = true;
        generated.containerTitle = QStringLiteral("Container 7");
        generated.containerTitleIsGenerated = true;
        generated.tabs = {{QStringLiteral("page-a"), pageTitle, true}};
        applyResolvedLabel(generated);
        generated.outerRect = QRectF(
            0.0, 0.0,
            ChromeShadedBadge::badgeWidth(ChromeMetrics{}, 1,
                                          generated.badgeLabelWidth)
                + 2.0 * ChromeMetrics{}.outerBorder,
            31.0);
        const auto generatedPlan = ChromeLayoutEngine::build(generated);
        QVERIFY(generatedPlan);
        QCOMPARE(badgeLabelText(*generatedPlan), pageTitle);

        // A container the user actually named keeps the name, ahead of the page.
        ChromeLayoutRequest renamed = generated;
        renamed.containerTitle = QStringLiteral("Planning");
        renamed.containerTitleIsGenerated = false;
        applyResolvedLabel(renamed);
        renamed.outerRect = QRectF(
            0.0, 0.0,
            ChromeShadedBadge::badgeWidth(ChromeMetrics{}, 1,
                                          renamed.badgeLabelWidth)
                + 2.0 * ChromeMetrics{}.outerBorder,
            31.0);
        const auto renamedPlan = ChromeLayoutEngine::build(renamed);
        QVERIFY(renamedPlan);
        QCOMPARE(badgeLabelText(*renamedPlan),
                 QStringLiteral("Planning \u00B7 ") + pageTitle);

        // ADR-0163's promise still holds: with no page title to show, the
        // generated placeholder is what keeps the badge from being anonymous.
        ChromeLayoutRequest anonymous = generated;
        anonymous.tabs = {{QStringLiteral("page-a"), QString(), true}};
        applyResolvedLabel(anonymous);
        const auto anonymousPlan = ChromeLayoutEngine::build(anonymous);
        QVERIFY(anonymousPlan);
        QCOMPARE(badgeLabelText(*anonymousPlan), QStringLiteral("Container 7"));
    }

    // ADR-0189. The defect: the label rect was a fixed 48-140 px and the strip
    // was sized for 140 px of label whatever the title said, so an ordinary
    // page title was elided away the moment the container rolled up. These
    // rows are the oracle for the measured label.
    void measuredLabelKeepsAFortyCharacterTitleUnelided()
    {
        const QString title =
            QStringLiteral("Quarterly revenue model, revision 12");
        QCOMPARE(title.size(), 36);
        const auto plan = badgePlanWithTitles(3, {}, false, title);
        QCOMPARE(plan.badgeLabelText, title);

        // The reserved rect holds the whole title: eliding it at the width
        // paint() uses returns the same string, with no ellipsis.
        const auto metrics = labelMetrics();
        const qreal paintWidth = plan.badgeLabelRect.width() - 8.0;
        QVERIFY(paintWidth > 0.0);
        QCOMPARE(metrics.elidedText(title, Qt::ElideRight, qRound(paintWidth)), title);
        // And it is genuinely wider than the old fixed maximum.
        QVERIFY2(plan.badgeLabelRect.width() > 140.0,
                 qPrintable(QStringLiteral("label rect was %1")
                                .arg(plan.badgeLabelRect.width())));
        QVERIFY(plan.badgeLabelRect.width()
                <= ChromeShadedBadge::LabelMaximumWidth + 0.01);

        // The ink proves the strip really grew rather than the rect merely
        // claiming width: glyphs are painted past the old 140 px label bound.
        //
        // AGENT-GUARD: probe for a colour that differs from the badge surface,
        // never for alpha alone. The strip's own surface is opaque, so an
        // alpha-only probe passes on an empty label and proves nothing — that
        // mistake hid a genuinely unpainted label here until a nested capture
        // was inspected by eye.
        const auto image = render(plan);
        // Glyphs, inside the label rect, past where the old fixed 140 px rect
        // would have ended.
        const int inkPastOldBound = labelInkPixels(image, plan, 141.0);
        QVERIFY2(inkPastOldBound > 10,
                 qPrintable(QStringLiteral("only %1 glyph pixels past the former "
                                           "140 px label bound")
                                .arg(inkPastOldBound)));
        // And the label band as a whole carries text, so the row cannot pass
        // on a stray pixel.
        QVERIFY(labelInkPixels(image, plan) > 40);
    }

    void labelWidthIsClampedToItsDocumentedBounds()
    {
        const auto metrics = labelMetrics();
        QCOMPARE(ChromeShadedBadge::labelWidthFor(QString(), metrics),
                 ChromeShadedBadge::LabelMinimumWidth);
        QCOMPARE(ChromeShadedBadge::labelWidthFor(QStringLiteral("."), metrics),
                 ChromeShadedBadge::LabelMinimumWidth);
        QCOMPARE(ChromeShadedBadge::labelWidthFor(QString(600, QLatin1Char('W')),
                                                  metrics),
                 ChromeShadedBadge::LabelMaximumWidth);
        // Strictly between the bounds for a realistic title, and monotone.
        const QString shortTitle = QStringLiteral("Notes");
        const QString longTitle = QStringLiteral("Notes on the third quarter");
        const qreal narrow = ChromeShadedBadge::labelWidthFor(shortTitle, metrics);
        const qreal wide = ChromeShadedBadge::labelWidthFor(longTitle, metrics);
        QVERIFY(narrow <= wide);
        QVERIFY(wide > ChromeShadedBadge::LabelMinimumWidth);
        QVERIFY(wide < ChromeShadedBadge::LabelMaximumWidth);
    }

    void badgeWidthFollowsTheLabelAndClampsWithIt()
    {
        const ChromeMetrics metrics;
        const qreal atMinimum =
            ChromeShadedBadge::badgeWidth(metrics, 3,
                                          ChromeShadedBadge::LabelMinimumWidth);
        const qreal atMaximum =
            ChromeShadedBadge::badgeWidth(metrics, 3,
                                          ChromeShadedBadge::LabelMaximumWidth);
        QVERIFY(atMaximum > atMinimum);
        QCOMPARE(atMaximum - atMinimum,
                 ChromeShadedBadge::LabelMaximumWidth
                     - ChromeShadedBadge::LabelMinimumWidth);
        // Out-of-range input is clamped, never trusted.
        QCOMPARE(ChromeShadedBadge::badgeWidth(metrics, 3, 0.0), atMinimum);
        QCOMPARE(ChromeShadedBadge::badgeWidth(metrics, 3, -500.0), atMinimum);
        QCOMPARE(ChromeShadedBadge::badgeWidth(metrics, 3, 5000.0), atMaximum);
    }

    // AGENT-GUARD: the label is the only thing on a rolled-up badge that says
    // which page this is, so pills must yield first. A strip that dropped the
    // label instead would be the original defect in a new form.
    void pillsYieldBeforeTheLabelWhenTheStripIsTooNarrow()
    {
        const QString title = QStringLiteral("Quarterly revenue model");
        const qreal measured = ChromeShadedBadge::labelWidthFor(
            ChromeShadedBadge::resolveLabel({}, false, title), labelMetrics());
        const auto roomy = badgePlanWithTitles(8, {}, false, title);
        QCOMPARE(roomy.tabs.size(), 8);
        QCOMPARE(roomy.badgeOverflowCount, 0);
        QVERIFY(qAbs(roomy.badgeLabelRect.width() - measured) < 0.51);

        // Two thirds of the room: pills collapse into "+N" while the label
        // keeps as much of its measured width as the minimum allows.
        const qreal narrowOuter =
            (ChromeShadedBadge::badgeWidth(ChromeMetrics{}, 8, measured)
             + 2.0 * ChromeMetrics{}.outerBorder) * 2.0 / 3.0;
        const auto cramped =
            badgePlanWithTitles(8, {}, false, title, narrowOuter);
        QVERIFY2(cramped.tabs.size() < 8,
                 qPrintable(QStringLiteral("pills did not yield: %1")
                                .arg(cramped.tabs.size())));
        QVERIFY(cramped.badgeOverflowCount > 0);
        QVERIFY(cramped.tabsOverflowed);
        QVERIFY2(cramped.badgeLabelRect.width()
                     >= ChromeShadedBadge::LabelMinimumWidth - 0.01,
                 qPrintable(QStringLiteral("label fell below its minimum: %1")
                                .arg(cramped.badgeLabelRect.width())));
        // Nothing painted outside the strip.
        QVERIFY(cramped.badgeLabelRect.left() >= cramped.outerFrame.left() - 0.01);
        if (!cramped.tabs.isEmpty()) {
            QVERIFY(cramped.tabs.constLast().rect.right()
                    <= cramped.outerFrame.right() + 0.01);
        }
    }

    void resolvedLabelFollowsTheGeneratedNameRule()
    {
        // ADR-0168, now a pure function shared by the strip sizing and paint.
        QCOMPARE(ChromeShadedBadge::resolveLabel({}, false,
                                                 QStringLiteral("Inbox")),
                 QStringLiteral("Inbox"));
        QCOMPARE(ChromeShadedBadge::resolveLabel(QStringLiteral("Work"), false,
                                                 QStringLiteral("Inbox")),
                 QStringLiteral("Work \u00B7 Inbox"));
        QCOMPARE(ChromeShadedBadge::resolveLabel(QStringLiteral("Work"), false, {}),
                 QStringLiteral("Work"));
        // A generated placeholder never displaces a real title.
        QCOMPARE(ChromeShadedBadge::resolveLabel(QStringLiteral("Container 7"), true,
                                                 QStringLiteral("Inbox")),
                 QStringLiteral("Inbox"));
        QCOMPARE(ChromeShadedBadge::resolveLabel(QStringLiteral("Container 7"), true,
                                                 {}),
                 QStringLiteral("Container 7"));
        QCOMPARE(ChromeShadedBadge::resolveLabel({}, true, {}), QString());
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
