// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include "qindaqt/themes/theme_loader.h"

#include <QDir>
#include <QImage>
#include <QPainter>
#include <QTest>

#include <algorithm>

using namespace QindaQt::Decoration;

namespace {

QindaQt::Themes::ThemeSpec classicTheme()
{
    QindaQt::Themes::ThemeSpec theme;
    theme.id = QStringLiteral("classic");
    theme.colors = {{QStringLiteral("surface"), QColor(QStringLiteral("#20242a"))},
                    {QStringLiteral("surfaceRaised"), QColor(QStringLiteral("#2c313a"))},
                    {QStringLiteral("border"), QColor(QStringLiteral("#4a5261"))},
                    {QStringLiteral("text"), QColor(QStringLiteral("#f2f4f7"))},
                    {QStringLiteral("textMuted"), QColor(QStringLiteral("#a4adba"))},
                    {QStringLiteral("accent"), QColor(QStringLiteral("#4f8cff"))}};
    return theme;
}

QindaQt::Themes::ThemeSpec lunaTheme()
{
    auto theme = classicTheme();
    theme.id = QStringLiteral("luna");
    theme.decoration.buttonStyle = QStringLiteral("glyph");
    theme.decoration.titleBarColor = QColor(QStringLiteral("#2B6FD4"));
    theme.decoration.titleBarInactiveColor = QColor(QStringLiteral("#7E96B8"));
    theme.decoration.restoreColor = QColor(QStringLiteral("#E87BD0"));
    theme.decoration.closeColor = QColor(QStringLiteral("#2D6BE4"));
    theme.decoration.minimizeColor = QColor(QStringLiteral("#E23B3B"));
    theme.decoration.maximizeColor = QColor(QStringLiteral("#3DA53D"));
    return theme;
}

QImage paintedChrome(const DecorationChrome &chrome, bool active, const QSizeF &size)
{
    QImage image(size.toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    DecorationFrameVisual frame;
    frame.size = size;
    frame.caption = QStringLiteral("Preview");
    frame.active = active;
    paintDecoration(painter, chrome, frame, layoutDecorationButtons(chrome, size));
    return image;
}

} // namespace

class DecorationPainterTests final : public QObject
{
    Q_OBJECT

private slots:
    void chromeRoundTripsThroughTheCompositorMap();
    void unauthoredLunaKeysStayAbsent();
    void buttonLayoutFollowsTheLiveDecoration();
    void titleAndCaptionColorsFollowFocusAndAuthoring();
    void paintsClassicAndLunaChromeDeterministically();
    void defaultPreferencesReproduceEveryShippedTheme();
    void authoredDecorationDrivesContainerChrome();
    void windowPreferencesArrangeButtonsAndCaptions();
    void paintsFlatButtonsAndLeftCaptions();
    void preferenceTokensAndContainerStylesRoundTrip();
    void layoutsAndPaintsTheContainedWindowHandlebar();
};

void DecorationPainterTests::chromeRoundTripsThroughTheCompositorMap()
{
    const auto chrome = DecorationChrome::fromTheme(lunaTheme());
    const auto map = chrome.toVariantMap();
    QCOMPARE(map.value(QStringLiteral("buttonStyle")).toString(), QStringLiteral("glyph"));
    QCOMPARE(map.value(QStringLiteral("titleBar")).value<QColor>(),
             QColor(QStringLiteral("#2B6FD4")));
    const auto back = DecorationChrome::fromVariantMap(map);
    QCOMPARE(back.toVariantMap(), map);
    QVERIFY(back.glyphChrome());
    QVERIFY(back.wornLuna());
    QCOMPARE(back.close, QColor(QStringLiteral("#2D6BE4")));
    QCOMPARE(back.surface, QColor(QStringLiteral("#20242a")));
}

void DecorationPainterTests::unauthoredLunaKeysStayAbsent()
{
    const auto map = DecorationChrome::fromTheme(classicTheme()).toVariantMap();
    QVERIFY(!map.contains(QStringLiteral("titleBar")));
    QVERIFY(!map.contains(QStringLiteral("titleBarInactive")));
    QVERIFY(!map.contains(QStringLiteral("restore")));
    QCOMPARE(map.value(QStringLiteral("buttonStyle")).toString(), QStringLiteral("symbols"));
    // Tolerant parsing: mistyped or missing keys never fail.
    const auto tolerant = DecorationChrome::fromVariantMap(
        {{QStringLiteral("buttonStyle"), 7}, {QStringLiteral("close"), QStringLiteral("x")}});
    QCOMPARE(tolerant.buttonStyle, QStringLiteral("symbols"));
    QVERIFY(!tolerant.close.isValid());
    QVERIFY(!tolerant.wornLuna());
}

void DecorationPainterTests::buttonLayoutFollowsTheLiveDecoration()
{
    const QSizeF size(400.0, 300.0);
    const auto classic = layoutDecorationButtons(DecorationChrome::fromTheme(classicTheme()), size);
    QCOMPARE(classic.size(), 3);
    QCOMPARE(classic.at(0).kind, DecorationButtonKind::Close);
    QCOMPARE(classic.at(0).geometry, QRectF(12.0, 5.0, 14.0, 14.0));
    QCOMPARE(classic.at(2).kind, DecorationButtonKind::Maximize);
    QCOMPARE(classic.at(2).geometry.left(), 12.0 + 2.0 * (14.0 + 8.0));

    const auto luna = layoutDecorationButtons(DecorationChrome::fromTheme(lunaTheme()), size);
    QCOMPARE(luna.size(), 3);
    QCOMPARE(luna.at(0).kind, DecorationButtonKind::Minimize);
    QCOMPARE(luna.at(2).kind, DecorationButtonKind::Close);
    QCOMPARE(luna.at(2).geometry.right(), size.width() - 14.0);
    QCOMPARE(luna.at(0).geometry.top(), (DecorationTitleHeight - 16.0) / 2.0);

    const auto captionClassic = decorationCaptionRect(
        DecorationChrome::fromTheme(classicTheme()), size, classic);
    QVERIFY(captionClassic.left() > classic.at(2).geometry.right());
    const auto captionLuna = decorationCaptionRect(DecorationChrome::fromTheme(lunaTheme()),
                                                  size, luna);
    QVERIFY(captionLuna.right() < luna.at(0).geometry.left());
    QCOMPARE(decorationBorders(false), QMarginsF(1.0, DecorationTitleHeight, 1.0, 1.0));
    QCOMPARE(decorationBorders(true), QMarginsF(0.0, DecorationTitleHeight, 0.0, 0.0));
}

void DecorationPainterTests::titleAndCaptionColorsFollowFocusAndAuthoring()
{
    const auto classic = DecorationChrome::fromTheme(classicTheme());
    QCOMPARE(decorationTitleColor(classic, true), classic.surfaceRaised);
    QCOMPARE(decorationTitleColor(classic, false), classic.surface);
    // Dark classic title: caption stays the theme text, not forced white.
    QCOMPARE(decorationCaptionColor(classic, true), QColor(Qt::white));
    const auto luna = DecorationChrome::fromTheme(lunaTheme());
    QCOMPARE(decorationTitleColor(luna, true), QColor(QStringLiteral("#2B6FD4")));
    QCOMPARE(decorationTitleColor(luna, false), QColor(QStringLiteral("#7E96B8")));
    QCOMPARE(decorationGlyphChromeColor(luna, DecorationButtonKind::Maximize, true),
             QColor(QStringLiteral("#E87BD0")));
    QCOMPARE(decorationGlyphChromeColor(luna, DecorationButtonKind::Maximize, false),
             QColor(QStringLiteral("#3DA53D")));
    // Glyph ink follows the fill's luminance: light classic traffic lights
    // take black glyphs, the saturated Luna fills take white.
    QCOMPARE(decorationButtonGlyphColor(classic, DecorationButtonKind::Close, true),
             QColor(Qt::black));
    QCOMPARE(decorationButtonGlyphColor(luna, DecorationButtonKind::Minimize, true),
             QColor(Qt::white));
    QCOMPARE(decorationWearSeed(QStringLiteral("a"), 400.0),
             decorationWearSeed(QStringLiteral("a"), 400.0));
    QVERIFY(decorationWearSeed(QStringLiteral("a"), 400.0)
            != decorationWearSeed(QStringLiteral("b"), 400.0));
}

void DecorationPainterTests::paintsClassicAndLunaChromeDeterministically()
{
    const QSizeF size(360.0, 200.0);
    const auto classic = DecorationChrome::fromTheme(classicTheme());
    const QImage first = paintedChrome(classic, true, size);
    const QImage second = paintedChrome(classic, true, size);
    QCOMPARE(first, second);
    // The title bar carries the raised surface, the client area stays clear.
    QCOMPARE(QColor(first.pixel(180, 12)).name(), classic.surfaceRaised.name());
    QCOMPARE(first.pixelColor(180, 100).alpha(), 0);
    // Classic buttons sit on the left as filled circles.
    QCOMPARE(QColor(first.pixel(19, 12)).name(), classic.close.name());

    const auto luna = DecorationChrome::fromTheme(lunaTheme());
    const QImage lunaActive = paintedChrome(luna, true, size);
    QCOMPARE(lunaActive, paintedChrome(luna, true, size));
    const QImage lunaInactive = paintedChrome(luna, false, size);
    QVERIFY(lunaActive != lunaInactive);
    // Worn Luna paint: the bar is painted, and the right-hand glyph group
    // draws over it where the classic chrome would have left plain paint.
    QVERIFY(lunaActive.pixelColor(180, 12).alpha() > 0);
    const auto buttons = layoutDecorationButtons(luna, size);
    const QRect closeRect = buttons.at(2).geometry.toRect();
    bool strokeFound = false;
    for (int y = closeRect.top(); y < closeRect.bottom() && !strokeFound; ++y) {
        for (int x = closeRect.left(); x < closeRect.right(); ++x) {
            const QColor pixel = lunaActive.pixelColor(x, y);
            if (qAbs(pixel.blue() - luna.close.blue()) < 40
                && pixel.red() < 120) {
                strokeFound = true;
                break;
            }
        }
    }
    QVERIFY2(strokeFound, "Luna close glyph stroke missing from the painted chrome");
}

namespace {

QindaQt::Themes::ThemeSpec shippedTheme(const QString &id)
{
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + id + QStringLiteral(".json"));
    return loaded.ok ? loaded.theme : QindaQt::Themes::ThemeSpec{};
}

int leftmostInk(const QImage &image, const QColor &ground, int from, int to)
{
    for (int x = from; x < to; ++x) {
        for (int y = 7; y < 18; ++y) {
            const QColor pixel = image.pixelColor(x, y);
            if (qAbs(pixel.red() - ground.red()) + qAbs(pixel.green() - ground.green())
                    + qAbs(pixel.blue() - ground.blue()) > 120) {
                return x;
            }
        }
    }
    return -1;
}

} // namespace

void DecorationPainterTests::defaultPreferencesReproduceEveryShippedTheme()
{
    // ADR-0129: untouched preferences must leave both chrome sets exactly as
    // they rendered before preferences existed, for every shipped theme.
    const QDir directory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    const auto files = directory.entryList({QStringLiteral("*.json")}, QDir::Files);
    QVERIFY(files.size() >= 6);
    const ChromePreferences defaults;
    for (const QString &file : files) {
        const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(directory.filePath(file));
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        const auto &theme = loaded.theme;
        const auto base = DecorationChrome::fromTheme(theme);
        QCOMPARE(resolveWindowChrome(theme, defaults).toVariantMap(), base.toVariantMap());
        QCOMPARE(effectiveButtonSide(base),
                 base.glyphChrome() ? DecorationButtonSide::Right : DecorationButtonSide::Left);
        if (!theme.decoration.authored || theme.id == QLatin1String("qinda-macos")) {
            const auto container = resolveContainerStyle(theme, defaults);
            const auto macos = QindaQt::HybridChrome::ChromeStyle::qindaMacOS(
                chromePaletteForTheme(theme));
            QCOMPARE(container.buttonSide, macos.buttonSide);
            QCOMPARE(container.tabDirection, macos.tabDirection);
            QCOMPARE(container.buttonStyle, macos.buttonStyle);
            QCOMPARE(container.hoverGlyphs, macos.hoverGlyphs);
            QCOMPARE(container.palette.surface, macos.palette.surface);
        }
    }
}

void DecorationPainterTests::authoredDecorationDrivesContainerChrome()
{
    using namespace QindaQt::HybridChrome;
    const auto bliss = shippedTheme(QStringLiteral("qinda-bliss"));
    QVERIFY(bliss.decoration.authored);
    ChromePreferences preferences;
    auto style = resolveContainerStyle(bliss, preferences);
    QCOMPARE(style.buttonSide, ButtonSide::Right);
    QCOMPARE(style.tabDirection, TabVisualDirection::LeftToRight);
    QCOMPARE(style.buttonStyle, ButtonStyle::Symbols);
    QVERIFY(!style.hoverGlyphs);

    preferences.containerButtonStyle = QStringLiteral("traffic-lights");
    preferences.containerButtonSide = QStringLiteral("left");
    preferences.containerTabOrder = QStringLiteral("right-to-left");
    style = resolveContainerStyle(bliss, preferences);
    QCOMPARE(style.buttonStyle, ButtonStyle::TrafficLights);
    QCOMPARE(style.buttonSide, ButtonSide::Left);
    QCOMPARE(style.tabDirection, TabVisualDirection::RightToLeft);
    QVERIFY(style.hoverGlyphs); // the theme authors hover glyphs for lights
    preferences.containerButtonGlyphs = QStringLiteral("always");
    QVERIFY(!resolveContainerStyle(bliss, preferences).hoverGlyphs);
    preferences.containerButtonStyle = QStringLiteral("flat");
    preferences.containerButtonGlyphs = QStringLiteral("hover");
    style = resolveContainerStyle(bliss, preferences);
    QCOMPARE(style.buttonStyle, ButtonStyle::Symbols);
    QVERIFY(style.hoverGlyphs);
}

void DecorationPainterTests::windowPreferencesArrangeButtonsAndCaptions()
{
    const QSizeF size(400.0, 300.0);
    ChromePreferences preferences;
    preferences.windowButtonSide = QStringLiteral("right");
    auto chrome = applyWindowPreferences(DecorationChrome::fromTheme(classicTheme()), preferences);
    auto buttons = layoutDecorationButtons(chrome, size);
    QCOMPARE(buttons.size(), 3);
    QCOMPARE(buttons.at(0).kind, DecorationButtonKind::Minimize);
    QCOMPARE(buttons.at(2).kind, DecorationButtonKind::Close);
    QCOMPARE(buttons.at(2).geometry.right(), size.width() - 14.0);
    QCOMPARE(buttons.at(0).geometry.size(), QSizeF(14.0, 14.0));
    QVERIFY(decorationCaptionRect(chrome, size, buttons).right() < buttons.at(0).geometry.left());

    preferences.windowButtons = QStringLiteral("minimize-close");
    chrome = applyWindowPreferences(DecorationChrome::fromTheme(classicTheme()), preferences);
    buttons = layoutDecorationButtons(chrome, size);
    QCOMPARE(buttons.size(), 2);
    QVERIFY(std::none_of(buttons.cbegin(), buttons.cend(), [](const auto &button) {
        return button.kind == DecorationButtonKind::Maximize;
    }));
    preferences.windowButtons = QStringLiteral("close");
    chrome = applyWindowPreferences(DecorationChrome::fromTheme(classicTheme()), preferences);
    QCOMPARE(layoutDecorationButtons(chrome, size).size(), 1);

    // Flat symbols default to the right edge and use the 16 px glyph cell.
    ChromePreferences flat;
    flat.windowButtonStyle = QStringLiteral("flat");
    chrome = applyWindowPreferences(DecorationChrome::fromTheme(classicTheme()), flat);
    QVERIFY(chrome.flatChrome());
    buttons = layoutDecorationButtons(chrome, size);
    QCOMPARE(effectiveButtonSide(chrome), DecorationButtonSide::Right);
    QCOMPARE(buttons.at(0).geometry.size(), QSizeF(16.0, 16.0));

    // The published map carries only non-default arrangement and survives a
    // round trip through the compositor property.
    preferences.windowTitleAlignment = QStringLiteral("left");
    chrome = applyWindowPreferences(DecorationChrome::fromTheme(classicTheme()), preferences);
    const auto map = chrome.toVariantMap();
    QCOMPARE(map.value(QStringLiteral("buttonSide")).toString(), QStringLiteral("right"));
    QCOMPARE(map.value(QStringLiteral("buttons")).toString(), QStringLiteral("close"));
    QCOMPARE(map.value(QStringLiteral("titleAlignment")).toString(), QStringLiteral("left"));
    QCOMPARE(DecorationChrome::fromVariantMap(map).toVariantMap(), map);
    const auto untouched = DecorationChrome::fromTheme(classicTheme()).toVariantMap();
    QVERIFY(!untouched.contains(QStringLiteral("buttonSide")));
    QVERIFY(!untouched.contains(QStringLiteral("buttons")));
    QVERIFY(!untouched.contains(QStringLiteral("titleAlignment")));
    const auto tolerant = DecorationChrome::fromVariantMap(
        {{QStringLiteral("buttonSide"), QStringLiteral("up")},
         {QStringLiteral("buttons"), 3}});
    QVERIFY(tolerant.buttonSide.isEmpty());
    QCOMPARE(tolerant.buttons, QStringLiteral("all"));
}

void DecorationPainterTests::paintsFlatButtonsAndLeftCaptions()
{
    const QSizeF size(360.0, 200.0);
    ChromePreferences preferences;
    preferences.windowButtonStyle = QStringLiteral("flat");
    auto centered = applyWindowPreferences(DecorationChrome::fromTheme(classicTheme()), preferences);
    preferences.windowTitleAlignment = QStringLiteral("left");
    auto left = applyWindowPreferences(DecorationChrome::fromTheme(classicTheme()), preferences);

    const QImage leftImage = paintedChrome(left, true, size);
    QCOMPARE(leftImage, paintedChrome(left, true, size));
    const QImage centeredImage = paintedChrome(centered, true, size);
    const QColor title = decorationTitleColor(left, true);
    const int leftInk = leftmostInk(leftImage, title, 2, int(size.width() / 2));
    const int centerInk = leftmostInk(centeredImage, title, 2, int(size.width() / 2));
    QVERIFY2(leftInk >= 0 && leftInk < 40, qPrintable(QString::number(leftInk)));
    QVERIFY2(centerInk < 0 || centerInk > leftInk + 40, qPrintable(QString::number(centerInk)));

    // Flat glyphs paint in caption ink inside their cells without hover.
    const auto buttons = layoutDecorationButtons(left, size);
    const QRect close = buttons.constLast().geometry.toRect();
    bool ink = false;
    for (int y = close.top(); y <= close.bottom() && !ink; ++y) {
        for (int x = close.left(); x <= close.right(); ++x) {
            if (qAbs(leftImage.pixelColor(x, y).lightness() - title.lightness()) > 80) {
                ink = true;
                break;
            }
        }
    }
    QVERIFY2(ink, "flat close glyph missing");
}

void DecorationPainterTests::preferenceTokensAndContainerStylesRoundTrip()
{
    const auto keys = ChromePreferences::settingsKeys();
    QCOMPARE(keys.size(), 8);
    const ChromePreferences defaults;
    for (const QString &key : keys) {
        const auto allowed = ChromePreferences::tokens(key);
        QVERIFY2(!allowed.isEmpty(), qPrintable(key));
        QCOMPARE(defaults.token(key), allowed.constFirst());
    }
    ChromePreferences edited;
    QVERIFY(edited.setToken(QStringLiteral("appearance.containerButtonSide"),
                            QStringLiteral("left")));
    QVERIFY(!edited.setToken(QStringLiteral("appearance.containerButtonSide"),
                             QStringLiteral("middle")));
    QVERIFY(!edited.setToken(QStringLiteral("appearance.unknown"), QStringLiteral("theme")));
    QCOMPARE(ChromePreferences::fromSettingsValues(edited.toSettingsValues()), edited);
    const auto tolerant = ChromePreferences::fromSettingsValues(
        {{QStringLiteral("appearance.windowButtons"), QStringLiteral("sideways")},
         {QStringLiteral("appearance.windowButtonSide"), 1}});
    QCOMPARE(tolerant, ChromePreferences{});

    const auto style = resolveContainerStyle(shippedTheme(QStringLiteral("qinda-bliss")), edited);
    const auto back = containerStyleFromVariantMap(containerStyleToVariantMap(style));
    QCOMPARE(back.buttonSide, style.buttonSide);
    QCOMPARE(back.tabDirection, style.tabDirection);
    QCOMPARE(back.buttonStyle, style.buttonStyle);
    QCOMPARE(back.hoverGlyphs, style.hoverGlyphs);
    QCOMPARE(back.palette.surface, style.palette.surface);
    QCOMPARE(back.palette.close, style.palette.close);
}

void DecorationPainterTests::layoutsAndPaintsTheContainedWindowHandlebar()
{
    const QSizeF size(320.0, 200.0);
    const auto chrome = DecorationChrome::fromTheme(classicTheme());
    auto buttons = layoutMemberHandleButtons(chrome, size);
    // ADR-0131: classic chrome keeps stoplights on the left in miniature
    // cells; the "more" control takes the opposite end.
    QCOMPARE(buttons.size(), 4);
    QCOMPARE(buttons.at(0).kind, DecorationButtonKind::Close);
    QCOMPARE(buttons.at(0).geometry,
             QRectF(DecorationMiniButtonInset, 1.0, DecorationMiniButtonCell,
                    DecorationMiniButtonCell));
    QCOMPARE(buttons.constLast().kind, DecorationButtonKind::More);
    QCOMPARE(buttons.constLast().geometry.right(), size.width() - DecorationMiniButtonInset);
    for (const auto &button : buttons) {
        QVERIFY(button.geometry.bottom() <= DecorationMemberHandleHeight);
    }

    // Glyph chrome mirrors the arrangement, and the visible set applies.
    ChromePreferences preferences;
    preferences.windowButtons = QStringLiteral("close");
    const auto luna = applyWindowPreferences(DecorationChrome::fromTheme(lunaTheme()), preferences);
    buttons = layoutMemberHandleButtons(luna, size);
    QCOMPARE(buttons.size(), 2);
    QCOMPARE(buttons.at(0).kind, DecorationButtonKind::Close);
    QCOMPARE(buttons.at(0).geometry.right(), size.width() - DecorationMiniButtonInset);
    QCOMPARE(buttons.at(1).kind, DecorationButtonKind::More);
    QCOMPARE(buttons.at(1).geometry.left(), DecorationMiniButtonInset);

    QImage image(size.toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        DecorationFrameVisual frame;
        frame.size = size;
        frame.active = true;
        frame.memberHandle = true;
        paintMemberHandle(painter, chrome, frame);
        for (const auto &button : layoutMemberHandleButtons(chrome, size)) {
            paintDecorationButton(painter, chrome, frame, button);
        }
    }
    // The handle is title-colored, the client area below stays clear, the
    // grip marks the center, and the close stoplight fills its dot.
    QCOMPARE(QColor(image.pixel(80, 7)).name(), decorationTitleColor(chrome, true).name());
    QCOMPARE(image.pixelColor(160, 100).alpha(), 0);
    QVERIFY(QColor(image.pixel(160, 7)).name() != decorationTitleColor(chrome, true).name());
    QCOMPARE(QColor(image.pixel(12, 7)).name(), chrome.close.name());
}

QTEST_MAIN(DecorationPainterTests)
#include "tst_decoration_painter.moc"
