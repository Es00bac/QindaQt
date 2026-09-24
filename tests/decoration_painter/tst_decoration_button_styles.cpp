// SPDX-License-Identifier: GPL-3.0-or-later
#include "legacy_button_painters.h"

#include "qindaqt/decoration_painter/decoration_button_style.h"
#include "qindaqt/hybrid_chrome/chromelayoutengine.h"
#include "qindaqt/hybrid_chrome/chromerenderer.h"
#include "qindaqt/themes/decoration_theme_spec.h"
#include "qindaqt/themes/theme_spec.h"

#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTest>

#include <cmath>
#include <utility>

// Painter pixel tests for the named window-button styles (ADR-0264): the
// shipped styles against a frozen copy of their old painters, the solid
// console glyphs, every style at 1x and 2x on light and dark chrome, the
// tab title, the caption options, and the container path.
using namespace QindaQt::Decoration;

namespace {

DecorationChrome darkChrome()
{
    DecorationChrome chrome;
    chrome.surface = QColor(QStringLiteral("#20242a"));
    chrome.surfaceRaised = QColor(QStringLiteral("#2c313a"));
    chrome.border = QColor(QStringLiteral("#4a5261"));
    chrome.text = QColor(QStringLiteral("#f2f4f7"));
    chrome.textMuted = QColor(QStringLiteral("#a4adba"));
    chrome.close = QColor(QStringLiteral("#ff5f57"));
    chrome.minimize = QColor(QStringLiteral("#febc2e"));
    chrome.maximize = QColor(QStringLiteral("#28c840"));
    return chrome;
}

DecorationChrome lightChrome()
{
    auto chrome = darkChrome();
    chrome.surface = QColor(QStringLiteral("#e7efec"));
    chrome.surfaceRaised = QColor(QStringLiteral("#f7faf9"));
    chrome.border = QColor(QStringLiteral("#9dafa9"));
    chrome.text = QColor(QStringLiteral("#17231f"));
    chrome.textMuted = QColor(QStringLiteral("#60716c"));
    return chrome;
}

DecorationChrome lunaChrome()
{
    auto chrome = darkChrome();
    chrome.titleBar = QColor(QStringLiteral("#2B6FD4"));
    chrome.titleBarInactive = QColor(QStringLiteral("#7E96B8"));
    chrome.restore = QColor(QStringLiteral("#E87BD0"));
    chrome.close = QColor(QStringLiteral("#2D6BE4"));
    chrome.minimize = QColor(QStringLiteral("#E23B3B"));
    chrome.maximize = QColor(QStringLiteral("#3DA53D"));
    return chrome;
}

DecorationChrome styled(DecorationChrome chrome, const QString &style)
{
    chrome.buttonStyle = style;
    return chrome;
}

QImage canvas(const QSizeF &size, qreal scale)
{
    QImage image((size * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(scale);
    image.fill(Qt::transparent);
    return image;
}

QImage paintedChrome(const DecorationChrome &chrome, const QSizeF &size, qreal scale,
                     bool controlsHovered = false, bool hoverFirst = false,
                     const QIcon &icon = {})
{
    QImage image = canvas(size, scale);
    QPainter painter(&image);
    DecorationFrameVisual frame;
    frame.size = size;
    frame.caption = QStringLiteral("Preview");
    frame.controlsHovered = controlsHovered;
    frame.icon = icon;
    auto buttons = layoutDecorationButtons(chrome, size);
    if (hoverFirst && !buttons.isEmpty()) {
        buttons.first().hovered = true;
    }
    paintDecoration(painter, chrome, frame, buttons);
    return image;
}

// Any pixel in the logical rect that clearly differs from the ground.
bool hasInk(const QImage &image, const QRectF &logical, const QColor &ground, qreal scale)
{
    const QRect physical = QRectF(logical.topLeft() * scale, logical.size() * scale)
                               .toAlignedRect()
                               .intersected(image.rect());
    for (int y = physical.top(); y <= physical.bottom(); ++y) {
        for (int x = physical.left(); x <= physical.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (qAbs(pixel.red() - ground.red()) + qAbs(pixel.green() - ground.green())
                    + qAbs(pixel.blue() - ground.blue()) > 60) {
                return true;
            }
        }
    }
    return false;
}

bool inked(const QImage &image, const QPointF &logical, const QColor &stroke, qreal scale)
{
    const QColor pixel = image.pixelColor(QPointF(logical * scale).toPoint());
    return pixel.alpha() > 200 && qAbs(pixel.red() - stroke.red()) < 48
        && qAbs(pixel.green() - stroke.green()) < 48 && qAbs(pixel.blue() - stroke.blue()) < 48;
}

} // namespace

class DecorationButtonStyleTests final : public QObject
{
    Q_OBJECT

private slots:
    void shippedStylesRepaintTheirOldPixels();
    void consoleGlyphsAreSolidAndKeepTheirMeaning();
    void everyStylePaintsAtBothScalesOnLightAndDark();
    void tabStyleClearsTheStripBesideItsTab();
    void appIconAndTitleWeightChangeTheCaption();
    void containersPaintThroughTheSameStyles();
};

void DecorationButtonStyleTests::shippedStylesRepaintTheirOldPixels()
{
    // ADR-0264: the four shipped styles are rows of the style table now; in
    // every state they must paint exactly what the old painters painted.
    const QList<std::pair<QString, DecorationChrome>> cases{
        {QStringLiteral("symbols"), darkChrome()},  {QStringLiteral("traffic-lights"), lightChrome()},
        {QStringLiteral("flat"), darkChrome()},     {QStringLiteral("flat"), lightChrome()},
        {QStringLiteral("flat"), lunaChrome()},     {QStringLiteral("glyph"), lunaChrome()},
        {QStringLiteral("glyph"), darkChrome()}};
    const QSizeF size(200.0, 40.0);
    for (const auto &[style, base] : cases) {
        const auto chrome = styled(base, style);
        const auto buttons = layoutDecorationButtons(chrome, size);
        QCOMPARE(buttons.size(), 3);
        for (const qreal scale : {1.0, 2.0}) {
            for (int state = 0; state < 32; ++state) {
                DecorationFrameVisual frame;
                frame.size = size;
                frame.active = (state & 1) == 0;
                frame.controlsHovered = (state & 2) != 0;
                frame.restoreGlyph = (state & 4) != 0;
                for (auto button : buttons) {
                    button.hovered = (state & 8) != 0;
                    button.pressed = (state & 16) != 0;
                    QImage expected = canvas(size, scale);
                    QImage actual = canvas(size, scale);
                    {
                        QPainter painter(&expected);
                        LegacyButtons::paintButton(painter, chrome, frame, button);
                    }
                    {
                        QPainter painter(&actual);
                        paintDecorationButton(painter, chrome, frame, button);
                    }
                    QVERIFY2(actual == expected,
                             qPrintable(QStringLiteral("%1 state %2 kind %3 at %4x")
                                            .arg(style)
                                            .arg(state)
                                            .arg(static_cast<int>(button.kind))
                                            .arg(scale)));
                }
            }
        }
    }
}

void DecorationButtonStyleTests::consoleGlyphsAreSolidAndKeepTheirMeaning()
{
    // ADR-0264: the dashed pen broke the close cross into fragments. Every
    // sample along each stroke's centre line now carries the glyph's ink,
    // and the triangle still means maximize and the square restore.
    const auto chrome = styled(lunaChrome(), QStringLiteral("glyph"));
    const qreal scale = 4.0;
    const QRectF cell(0.0, 0.0, 16.0, 16.0);
    const QPointF center = cell.adjusted(1.0, 1.0, -1.0, -1.0).center();
    const qreal radius = 14.0 * 0.30;
    const auto paint = [&](DecorationButtonKind kind, bool restore) {
        QImage image = canvas(cell.size(), scale);
        QPainter painter(&image);
        DecorationFrameVisual frame;
        frame.size = cell.size();
        frame.restoreGlyph = restore;
        paintDecorationButton(painter, chrome, frame, {kind, cell});
        return image;
    };
    const QImage cross = paint(DecorationButtonKind::Close, false);
    for (qreal t = -0.85; t <= 0.85; t += 0.05) {
        QVERIFY2(inked(cross, center + QPointF(t * radius, t * radius), chrome.close, scale),
                 qPrintable(QString::number(t)));
        QVERIFY2(inked(cross, center + QPointF(t * radius, -t * radius), chrome.close, scale),
                 qPrintable(QString::number(t)));
    }
    const QImage circle = paint(DecorationButtonKind::Minimize, false);
    for (qreal angle = 0.0; angle < 6.28; angle += 0.2) {
        QVERIFY(inked(circle, center + QPointF(std::cos(angle), std::sin(angle)) * radius,
                      chrome.minimize, scale));
    }
    const QImage triangle = paint(DecorationButtonKind::Maximize, false);
    for (qreal t = -0.9; t <= 0.9; t += 0.1) {
        QVERIFY(inked(triangle, center + QPointF(t * radius * 1.1, radius * 0.8),
                      chrome.maximize, scale));
    }
    QVERIFY(!inked(triangle, center + QPointF(-radius, -radius), chrome.maximize, scale));
    const QImage square = paint(DecorationButtonKind::Maximize, true);
    for (qreal t = -0.9; t <= 0.9; t += 0.1) {
        QVERIFY(inked(square, center + QPointF(t * radius, -radius), chrome.restore, scale));
        QVERIFY(inked(square, center + QPointF(-radius, t * radius), chrome.restore, scale));
    }
}

void DecorationButtonStyleTests::everyStylePaintsAtBothScalesOnLightAndDark()
{
    const QSizeF size(360.0, 120.0);
    for (const auto &style : decorationButtonStyles()) {
        for (const auto &base : {lightChrome(), darkChrome()}) {
            const auto chrome = styled(base, style.name);
            const QColor title = decorationTitleColor(chrome, true);
            for (const qreal scale : {1.0, 2.0}) {
                const QString where = QStringLiteral("%1 on %2 at %3x")
                                          .arg(style.name, title.name())
                                          .arg(scale);
                const QImage rest = paintedChrome(chrome, size, scale);
                QVERIFY2(rest == paintedChrome(chrome, size, scale), qPrintable(where));
                const QImage shown = paintedChrome(chrome, size, scale, true);
                // Every button is visible once its glyphs show, and the
                // pointer always changes what the hovered button looks like.
                for (const auto &button : layoutDecorationButtons(chrome, size)) {
                    QVERIFY2(hasInk(shown, button.geometry, title, scale), qPrintable(where));
                }
                QVERIFY2(paintedChrome(chrome, size, scale, true, true) != shown,
                         qPrintable(where));
            }
        }
    }
}

void DecorationButtonStyleTests::tabStyleClearsTheStripBesideItsTab()
{
    const QSizeF size(480.0, 120.0);
    const auto tab = styled(darkChrome(), QStringLiteral("tab"));
    const QColor title = decorationTitleColor(tab, true);
    const QImage image = paintedChrome(tab, size, 1.0);
    // Inside the tab the bar is painted; beside it the strip stays clear.
    QCOMPARE(QColor(image.pixel(6, 12)).name(), title.name());
    QCOMPARE(image.pixelColor(460, 12).alpha(), 0);
    // The client area below the title is untouched either way.
    QCOMPARE(image.pixelColor(240, 80).alpha(), 0);
    // A full-width style, or the tab's buttons moved to the right edge,
    // keeps the whole bar.
    QCOMPARE(QColor(paintedChrome(styled(darkChrome(), QStringLiteral("bevel")), size, 1.0)
                        .pixel(300, 12))
                 .name(),
             title.name());
    auto right = tab;
    right.buttonSide = QStringLiteral("right");
    QCOMPARE(QColor(paintedChrome(right, size, 1.0).pixel(300, 12)).name(), title.name());
}

void DecorationButtonStyleTests::appIconAndTitleWeightChangeTheCaption()
{
    const QSizeF size(360.0, 80.0);
    auto chrome = styled(darkChrome(), QStringLiteral("flat"));
    chrome.titleAlignment = QStringLiteral("left");
    QPixmap red(32, 32);
    red.fill(Qt::red);
    const QIcon icon(red);
    const QImage plain = paintedChrome(chrome, size, 1.0, false, false, icon);
    chrome.appIcon = true;
    const QImage withIcon = paintedChrome(chrome, size, 1.0, false, false, icon);
    QVERIFY(withIcon != plain);
    // The icon leads the left-aligned caption.
    bool redFound = false;
    for (int y = 4; y < 20 && !redFound; ++y) {
        for (int x = 10; x < 34; ++x) {
            const QColor pixel = withIcon.pixelColor(x, y);
            if (pixel.red() > 200 && pixel.green() < 60 && pixel.blue() < 60) {
                redFound = true;
                break;
            }
        }
    }
    QVERIFY2(redFound, "application icon missing from the caption");
    chrome.appIcon = false;
    chrome.titleWeight = QFont::Normal;
    const QImage regular = paintedChrome(chrome, size, 1.0);
    chrome.titleWeight = QFont::Black;
    QVERIFY(paintedChrome(chrome, size, 1.0) != regular);
}

void DecorationButtonStyleTests::containersPaintThroughTheSameStyles()
{
    using namespace QindaQt::HybridChrome;
    // A container request like the Appearance preview's, painted with the
    // built-in lights and then with a named style.
    const auto planFor = [](const ChromeStyle &style) {
        ChromeLayoutRequest request;
        request.containerId = QStringLiteral("preview");
        request.outerRect = QRectF(0.0, 0.0, 600.0, 300.0);
        request.containerFocused = true;
        request.style = style;
        request.tabs = {{QStringLiteral("page-a"), QStringLiteral("Alpha"), true}};
        request.members = {{QStringLiteral("member-a"), QStringLiteral("Editor"),
                            QRectF(1.0, 29.0, 598.0, 270.0), true}};
        return ChromeLayoutEngine::build(request);
    };
    const auto render = [](const ChromeRenderPlan &plan) {
        QImage image = canvas(plan.outerFrame.size(), 1.0);
        QPainter painter(&image);
        ChromeRenderer::paint(painter, plan);
        return image;
    };
    ChromePreferences preferences;
    preferences.containerButtonStyle = QStringLiteral("traffic-lights");
    const auto lights = resolveContainerStyle(QindaQt::Themes::ThemeSpec{}, preferences);
    QVERIFY(!lights.buttonPainter);
    preferences.containerButtonStyle = QStringLiteral("gel");
    const auto gel = resolveContainerStyle(QindaQt::Themes::ThemeSpec{}, preferences);
    QVERIFY(gel.buttonPainter);
    const auto lightsPlan = planFor(lights);
    const auto gelPlan = planFor(gel);
    QVERIFY(lightsPlan && gelPlan);
    QCOMPARE(gelPlan->buttons.constFirst().rect.size(), QSizeF(15.0, 15.0));
    const QImage lightsImage = render(*lightsPlan);
    const QImage gelImage = render(*gelPlan);
    QVERIFY(gelImage != lightsImage);
    QVERIFY(hasInk(gelImage, gelPlan->buttons.constFirst().rect,
                   gelPlan->style.palette.surfaceRaised, 1.0));
    // A plan built from the Settings preview's map paints the same pixels.
    const auto mapped = planFor(containerStyleFromVariantMap(containerStyleToVariantMap(gel)));
    QVERIFY(mapped);
    QCOMPARE(render(*mapped), gelImage);
}

QTEST_MAIN(DecorationButtonStyleTests)
#include "tst_decoration_button_styles.moc"
