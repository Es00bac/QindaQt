// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QImage>
#include <QPainter>
#include <QTest>

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

QTEST_MAIN(DecorationPainterTests)
#include "tst_decoration_painter.moc"
