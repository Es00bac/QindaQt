// SPDX-License-Identifier: LGPL-3.0-or-later
#include "testfixtures.h"

#include "qindaqt/hybrid_chrome/chromeidentity.h"
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_chrome/chromerenderer.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QtTest>

#include <cmath>
#include <optional>

using namespace QindaQt::HybridChrome;
using namespace QindaQt::HybridChrome::TestFixtures;

namespace {

const char *const AllThemes[] = {"qinda-light", "qinda-dark",
                                 "qinda-high-contrast", "qinda-macos",
                                 "qinda-dusk", "qinda-bliss"};

// Harsh deliberate identity colors: near-white, near-black, and saturated,
// so the contrast-raising ladder is exercised, not just curated palette
// entries.
const char *const IdentityProbeColors[] = {
    "#f2f0eb", "#141414", "#ff2941", "#b65447", "#2f6f5f"};

std::optional<ChromePalette> paletteFromTheme(const QString &themeId)
{
    QFile file(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/%1.json").arg(themeId));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto colors = document.object().value(QStringLiteral("colors")).toObject();
    ChromePalette palette;
    const auto color = [&colors](const QString &name) {
        return QColor(colors.value(name).toString());
    };
    palette.surface = color(QStringLiteral("surface"));
    palette.surfaceRaised = color(QStringLiteral("surfaceRaised"));
    palette.border = color(QStringLiteral("border"));
    palette.text = color(QStringLiteral("text"));
    palette.textMuted = color(QStringLiteral("textMuted"));
    palette.accent = color(QStringLiteral("accent"));
    palette.close = color(QStringLiteral("danger"));
    palette.minimize = palette.accent;
    palette.maximize = palette.accent;
    QString error;
    if (!palette.isValid(&error)) {
        return std::nullopt;
    }
    return palette;
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

QPoint physicalPoint(const QPointF &logical, qreal devicePixelRatio)
{
    return {qRound(logical.x() * devicePixelRatio),
            qRound(logical.y() * devicePixelRatio)};
}

bool closeColor(const QColor &first, const QColor &second, int tolerance)
{
    return qAbs(first.red() - second.red()) <= tolerance
        && qAbs(first.green() - second.green()) <= tolerance
        && qAbs(first.blue() - second.blue()) <= tolerance
        && qAbs(first.alpha() - second.alpha()) <= tolerance;
}

} // namespace

class ChromeIdentityTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void shadesHoldContrastThresholdsForEveryTheme()
    {
        for (const auto *themeId : AllThemes) {
            const auto palette = paletteFromTheme(QString::fromLatin1(themeId));
            QVERIFY2(palette.has_value(), themeId);
            for (const auto *probe : IdentityProbeColors) {
                const auto identity = QColor(QString::fromLatin1(probe));
                const auto shades = resolveIdentityShades(identity, *palette);
                QVERIFY(shades.isValid());
                const auto label = QStringLiteral("%1/%2").arg(themeId, probe);
                QVERIFY2(identityContrastRatio(shades.border, palette->surface)
                             >= IdentityBorderContrast,
                         qPrintable(label + QStringLiteral(" border")));
                QVERIFY2(identityContrastRatio(shades.borderDimmed, palette->surface)
                             >= IdentityBorderContrast,
                         qPrintable(label + QStringLiteral(" dimmed")));
                QCOMPARE(shades.borderDimmed.alpha(), 255);
                QVERIFY2(identityContrastRatio(shades.textOnFill, shades.tabTint)
                             >= IdentityTextContrast,
                         qPrintable(label + QStringLiteral(" text")));
                QVERIFY2(identityContrastRatio(shades.handlebarInk, shades.handlebarFill)
                             >= IdentityTextContrast,
                         qPrintable(label + QStringLiteral(" ink")));
                QVERIFY(shades.tabTint != palette->surfaceRaised);
                QVERIFY(shades.glow.isValid());
                QCOMPARE(shades.glow.alpha(), qRound(0.28 * 255));
            }
        }
    }

    void invalidIdentityFallsBackToThemeAccentShades()
    {
        for (const auto *themeId : AllThemes) {
            const auto palette = paletteFromTheme(QString::fromLatin1(themeId));
            QVERIFY2(palette.has_value(), themeId);
            const auto fallback = resolveIdentityShades(QColor(), *palette);
            const auto fromAccent = resolveIdentityShades(palette->accent, *palette);
            QCOMPARE(fallback, fromAccent);
            QCOMPARE(fallback.base, palette->accent);
        }
    }

    void planInputsDefaultToTodaysOutput()
    {
        const auto plan = ChromeLayoutEngine::build(baseRequest());
        QVERIFY(plan);
        QCOMPARE(plan->identityColor, QColor());
        QCOMPARE(plan->indexBadge, 0);
        QCOMPARE(plan->identity.base, plan->style.palette.accent);
        // Unset overrides keep the derived titles.
        QCOMPARE(plan->tabs[0].title, QStringLiteral("Alpha"));
        QCOMPARE(plan->tabs[1].title, QStringLiteral("Beta"));
    }

    void planInputsOverrideTabTitlesAndBadge()
    {
        auto request = baseRequest();
        request.identityColor = QColor(QStringLiteral("#b65447"));
        request.indexBadge = 3;
        request.tabTitleOverrides = {{QStringLiteral("page-b"), QStringLiteral("Research")},
                                     {QStringLiteral("page-c"), QStringLiteral("")}};
        const auto plan = ChromeLayoutEngine::build(request);
        QVERIFY(plan);
        QCOMPARE(plan->identityColor, request.identityColor);
        QCOMPARE(plan->indexBadge, 3);
        QCOMPARE(plan->identity.base, request.identityColor);
        // A nonempty override replaces the derived title; an empty entry
        // keeps it (CONTRACTS §2.4).
        QCOMPARE(plan->tabs[1].title, QStringLiteral("Research"));
        QCOMPARE(plan->tabs[2].title, QStringLiteral("Gamma"));
        // Unknown override keys are tolerated, never rejected.
        request.tabTitleOverrides.insert(QStringLiteral("page-absent"),
                                         QStringLiteral("Ghost"));
        QVERIFY(ChromeLayoutEngine::build(request).has_value());
    }

    void frameStripeTabAndRingPaintIdentityShades()
    {
        const auto palette = paletteFromTheme(QStringLiteral("qinda-light"));
        QVERIFY(palette.has_value());
        const QColor identity(QStringLiteral("#b65447"));

        auto unfocusedRequest = baseRequest();
        unfocusedRequest.style.palette = *palette;
        unfocusedRequest.identityColor = identity;
        unfocusedRequest.devicePixelRatio = 2.0;
        const auto unfocusedPlan = ChromeLayoutEngine::build(unfocusedRequest);
        QVERIFY(unfocusedPlan);
        const auto unfocusedImage = render(*unfocusedPlan);

        auto focusedRequest = unfocusedRequest;
        focusedRequest.containerFocused = true;
        focusedRequest.members[0].focused = true;
        const auto focusedPlan = ChromeLayoutEngine::build(focusedRequest);
        QVERIFY(focusedPlan);
        const auto focusedImage = render(*focusedPlan);
        const qreal dpr = focusedPlan->devicePixelRatio;

        // Frame edge, mid-height, well away from corners and member holes:
        // 3 px full-strength border when focused, 2 px dimmed when not.
        const auto frameSample = physicalPoint(QPointF(0.5, 350.0), dpr);
        QVERIFY(closeColor(focusedImage.pixelColor(frameSample),
                           focusedPlan->identity.border, 24));
        QVERIFY(closeColor(unfocusedImage.pixelColor(frameSample),
                           unfocusedPlan->identity.borderDimmed, 24));
        QVERIFY(unfocusedImage.pixelColor(frameSample)
                != focusedPlan->identity.border);

        // The stripe along the title-row top edge is the identity color.
        const auto stripeSample = physicalPoint(QPointF(500.0, 2.5), dpr);
        QVERIFY(closeColor(focusedImage.pixelColor(stripeSample),
                           focusedPlan->identity.border, 24));

        // The active tab underline is 3 px of the identity border.
        const auto activeTab = std::find_if(focusedPlan->tabs.cbegin(),
                                            focusedPlan->tabs.cend(),
                                            [](const auto &tab) { return tab.active; });
        QVERIFY(activeTab != focusedPlan->tabs.cend());
        const auto underlineSample = physicalPoint(
            QPointF(activeTab->rect.center().x(), activeTab->rect.bottom() - 1.5), dpr);
        QVERIFY(closeColor(focusedImage.pixelColor(underlineSample),
                           focusedPlan->identity.border, 24));

        // The focused member ring carries the identity color; the member
        // interior stays a transparent hole.
        const auto ringSample = physicalPoint(QPointF(0.5, 350.0), dpr);
        QVERIFY(closeColor(focusedImage.pixelColor(ringSample),
                           focusedPlan->identity.border, 24));
        QCOMPARE(focusedImage.pixelColor(
                     physicalPoint(focusedPlan->members[0].windowRect.center(), dpr)),
                 QColor(Qt::transparent));
    }
};

QTEST_MAIN(ChromeIdentityTests)
#include "tst_identity.moc"
