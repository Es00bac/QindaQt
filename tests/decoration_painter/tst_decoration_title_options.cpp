// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_button_style.h"
#include "qindaqt/themes/decoration_theme_spec.h"
#include "qindaqt/themes/theme_spec.h"

#include <QTest>

#include <algorithm>

// The ADR-0264 title-bar options and style metrics: Settings1 tokens and
// their resolution into the published window chrome and the container
// style, the chrome map round trip, and the layout each option produces.
using namespace QindaQt::Decoration;

namespace {

DecorationChrome classicChrome(const QString &style = QStringLiteral("symbols"))
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
    chrome.buttonStyle = style;
    return chrome;
}

QList<DecorationButtonKind> kindsOf(const QList<DecorationButtonVisual> &buttons)
{
    QList<DecorationButtonKind> kinds;
    for (const auto &button : buttons) {
        kinds.append(button.kind);
    }
    return kinds;
}

} // namespace

class DecorationTitleOptionTests final : public QObject
{
    Q_OBJECT

private slots:
    void styleNamesMatchTheThemeAndSettingsTokens();
    void titleOptionTokensDecodeStrictlyAndRoundTrip();
    void windowOptionsResolveIntoThePublishedChrome();
    void styleMetricsAndOptionsDriveTheLayout();
    void rollUpSitsAtTheInnerEndAndNeverOnAHandlebar();
    void containerOptionsResolveIntoTheContainerStyle();
};

void DecorationTitleOptionTests::styleNamesMatchTheThemeAndSettingsTokens()
{
    // One style row per name the theme loaders accept, in the same order.
    QStringList names;
    for (const auto &style : decorationButtonStyles()) {
        names.append(style.name);
    }
    QCOMPARE(names, QindaQt::Themes::DecorationThemeTokens::buttonStyles());
    // Windows and containers offer the same list: every style but
    // "symbols", which windows paint exactly like the lights.
    auto offered = ChromePreferences::tokens(QString(ChromePreferenceKeys::WindowButtonStyle));
    QCOMPARE(offered, ChromePreferences::tokens(QString(ChromePreferenceKeys::ContainerButtonStyle)));
    QCOMPARE(offered.takeFirst(), QStringLiteral("theme"));
    QVERIFY(!offered.contains(QStringLiteral("symbols")));
    QCOMPARE(offered.size(), names.size() - 1);
    for (const QString &token : std::as_const(offered)) {
        QVERIFY2(isDecorationButtonStyle(token), qPrintable(token));
    }
    // An unknown name still paints the traffic lights, as before ADR-0264.
    QCOMPARE(decorationButtonStyle(QStringLiteral("sparkles")).name,
             QStringLiteral("traffic-lights"));
    QVERIFY(!isDecorationButtonStyle(QStringLiteral("sparkles")));
}

void DecorationTitleOptionTests::titleOptionTokensDecodeStrictlyAndRoundTrip()
{
    const auto keys = ChromePreferences::settingsKeys();
    QCOMPARE(keys.size(), 19);
    // The eleven option keys follow the original eight, in commit order.
    QCOMPARE(keys.at(8), QString(ChromePreferenceKeys::WindowButtonSize));
    QCOMPARE(keys.constLast(), QString(ChromePreferenceKeys::ContainerTitleDoubleClick));
    const ChromePreferences defaults;
    for (const QString &key : keys) {
        QCOMPARE(defaults.token(key), ChromePreferences::tokens(key).constFirst());
    }
    ChromePreferences edited;
    QVERIFY(edited.setToken(QString(ChromePreferenceKeys::WindowTitleDoubleClick),
                            QStringLiteral("roll-up")));
    QVERIFY(!edited.setToken(QString(ChromePreferenceKeys::WindowTitleDoubleClick),
                             QStringLiteral("none")));
    QVERIFY(edited.setToken(QString(ChromePreferenceKeys::ContainerTitleDoubleClick),
                            QStringLiteral("minimize")));
    QVERIFY(!edited.setToken(QString(ChromePreferenceKeys::ContainerTitleDoubleClick),
                             QStringLiteral("theme")));
    QVERIFY(edited.setToken(QString(ChromePreferenceKeys::WindowRollUpButton),
                            QStringLiteral("shown")));
    QVERIFY(edited.setToken(QString(ChromePreferenceKeys::ContainerButtonStyle),
                            QStringLiteral("pills")));
    QVERIFY(!edited.setToken(QString(ChromePreferenceKeys::WindowButtonSize),
                             QStringLiteral("huge")));
    QCOMPARE(ChromePreferences::fromSettingsValues(edited.toSettingsValues()), edited);
    // Tolerant decode: a mistyped or unknown value keeps its default.
    const auto tolerant = ChromePreferences::fromSettingsValues(
        {{QString(ChromePreferenceKeys::WindowCornerRadius), QStringLiteral("round")},
         {QString(ChromePreferenceKeys::WindowAppIcon), true}});
    QCOMPARE(tolerant, ChromePreferences{});
}

void DecorationTitleOptionTests::windowOptionsResolveIntoThePublishedChrome()
{
    // Untouched options publish nothing new: the map stays byte-identical.
    const auto untouched = applyWindowPreferences(classicChrome(), ChromePreferences{});
    const auto plainMap = untouched.toVariantMap();
    for (const char *key : {"buttonScale", "spacingScale", "titleHeight", "titleWeight",
                            "appIcon", "rollUpButton", "titleDoubleClick"}) {
        QVERIFY2(!plainMap.contains(QString::fromLatin1(key)), key);
    }
    QCOMPARE(plainMap, classicChrome().toVariantMap());

    ChromePreferences preferences;
    preferences.windowButtonStyle = QStringLiteral("chunky");
    preferences.windowButtonSize = QStringLiteral("large");
    preferences.windowButtonSpacing = QStringLiteral("tight");
    preferences.windowTitleHeight = QStringLiteral("tall");
    preferences.windowCornerRadius = QStringLiteral("square");
    preferences.windowTitleWeight = QStringLiteral("bold");
    preferences.windowAppIcon = QStringLiteral("shown");
    preferences.windowRollUpButton = QStringLiteral("shown");
    preferences.windowTitleDoubleClick = QStringLiteral("roll-up");
    const auto chrome = applyWindowPreferences(classicChrome(), preferences);
    QCOMPARE(chrome.buttonStyle, QStringLiteral("chunky"));
    QCOMPARE(chrome.buttonScale, 1.25);
    QCOMPARE(chrome.spacingScale, 0.5);
    // Heights are relative to the style's own bar: chunky's 34 plus 6.
    QCOMPARE(chrome.titleHeight, 40.0);
    QCOMPARE(chrome.cornerRadius, 0.0);
    QCOMPARE(chrome.titleWeight, static_cast<int>(QFont::Bold));
    QVERIFY(chrome.appIcon);
    QVERIFY(chrome.rollUpButton);
    QCOMPARE(chrome.titleDoubleClick, QStringLiteral("roll-up"));
    QCOMPARE(decorationBorders(chrome, false), QMarginsF(1.0, 40.0, 1.0, 1.0));
    const auto map = chrome.toVariantMap();
    QCOMPARE(DecorationChrome::fromVariantMap(map).toVariantMap(), map);

    // The decoration reads the map tolerantly: bad values keep defaults.
    const auto tolerant = DecorationChrome::fromVariantMap(
        {{QStringLiteral("titleHeight"), QStringLiteral("tall")},
         {QStringLiteral("buttonScale"), 9.0},
         {QStringLiteral("titleDoubleClick"), QStringLiteral("shade")}});
    QCOMPARE(tolerant.titleHeight, 0.0);
    QCOMPARE(tolerant.buttonScale, 1.0);
    QVERIFY(tolerant.titleDoubleClick.isEmpty());
}

void DecorationTitleOptionTests::styleMetricsAndOptionsDriveTheLayout()
{
    const QSizeF size(400.0, 300.0);
    // Chunky brings its own taller bar and touch-sized cells.
    auto chunky = classicChrome(QStringLiteral("chunky"));
    QCOMPARE(decorationTitleHeight(chunky), 34.0);
    auto buttons = layoutDecorationButtons(chunky, size);
    QCOMPARE(buttons.constFirst().geometry.size(), QSizeF(26.0, 26.0));
    QCOMPARE(buttons.constFirst().geometry.top(), 4.0);
    QCOMPARE(effectiveButtonSide(chunky), DecorationButtonSide::Right);
    QCOMPARE(buttons.constLast().geometry.right(), size.width() - 14.0);

    // Wide cells fill the bar, sit flush with the edge and touch.
    const auto wide = classicChrome(QStringLiteral("wide"));
    buttons = layoutDecorationButtons(wide, size);
    QCOMPARE(buttons.constFirst().geometry.size(), QSizeF(36.0, 24.0));
    QCOMPARE(buttons.constLast().geometry.right(), size.width());
    QCOMPARE(buttons.at(1).geometry.left(), buttons.at(0).geometry.right());

    // A joined capsule never spreads, whatever the spacing option says.
    auto pills = classicChrome(QStringLiteral("pills"));
    pills.spacingScale = 1.5;
    QCOMPARE(decorationButtonGap(pills), 0.0);

    // Size, spacing and height options on the classic lights.
    auto lights = classicChrome();
    lights.buttonScale = 0.8;
    lights.spacingScale = 1.5;
    lights.titleHeight = 20.0;
    buttons = layoutDecorationButtons(lights, size);
    QCOMPARE(buttons.constFirst().geometry, QRectF(12.0, 4.5, 11.0, 11.0));
    QCOMPARE(buttons.at(1).geometry.left() - buttons.at(0).geometry.right(), 12.0);
    QCOMPARE(decorationCaptionRect(lights, size, buttons).height(), 20.0);
    // A cell never outgrows its bar.
    lights.buttonScale = 2.0;
    QCOMPARE(decorationButtonCell(lights).height(), 20.0);
}

void DecorationTitleOptionTests::rollUpSitsAtTheInnerEndAndNeverOnAHandlebar()
{
    auto left = classicChrome();
    left.rollUpButton = true;
    QCOMPARE(decorationButtonKinds(left),
             (QList<DecorationButtonKind>{DecorationButtonKind::Close,
                                          DecorationButtonKind::Minimize,
                                          DecorationButtonKind::Maximize,
                                          DecorationButtonKind::RollUp}));
    auto right = classicChrome(QStringLiteral("flat"));
    right.rollUpButton = true;
    right.buttons = QStringLiteral("minimize-close");
    QCOMPARE(kindsOf(layoutDecorationButtons(right, QSizeF(400.0, 300.0))),
             (QList<DecorationButtonKind>{DecorationButtonKind::RollUp,
                                          DecorationButtonKind::Minimize,
                                          DecorationButtonKind::Close}));
    // A contained window's handlebar keeps its stoplights and "more" only:
    // its wheel already rolls the whole container.
    const auto handle = layoutMemberHandle(right, QSizeF(320.0, 200.0));
    QVERIFY(std::none_of(handle.buttons.cbegin(), handle.buttons.cend(),
                         [](const DecorationButtonVisual &button) {
                             return button.kind == DecorationButtonKind::RollUp;
                         }));
}

void DecorationTitleOptionTests::containerOptionsResolveIntoTheContainerStyle()
{
    using QindaQt::HybridChrome::ButtonStyle;
    using QindaQt::HybridChrome::TitleDoubleClickAction;
    const QindaQt::Themes::ThemeSpec plain;
    ChromePreferences preferences;
    // Defaults keep the built-in plates, the metrics and an inert title row.
    auto style = resolveContainerStyle(plain, preferences);
    QVERIFY(!style.buttonPainter);
    QVERIFY(style.buttonSize.isEmpty());
    QCOMPARE(style.buttonSpacing, -1.0);
    QCOMPARE(style.titleDoubleClick, TitleDoubleClickAction::None);

    preferences.containerButtonStyle = QStringLiteral("gel");
    preferences.containerButtonSize = QStringLiteral("large");
    preferences.containerButtonSpacing = QStringLiteral("roomy");
    preferences.containerTitleDoubleClick = QStringLiteral("roll-up");
    style = resolveContainerStyle(plain, preferences);
    QVERIFY(style.buttonPainter);
    QCOMPARE(style.namedButtonStyle, QStringLiteral("gel"));
    QCOMPARE(style.buttonStyle, ButtonStyle::Symbols);
    QVERIFY(style.hoverGlyphs);
    QCOMPARE(style.buttonSize, QSizeF(19.0, 19.0));
    QCOMPARE(style.buttonSpacing, 11.0);
    QCOMPARE(style.titleDoubleClick, TitleDoubleClickAction::RollUp);
    const auto back = containerStyleFromVariantMap(containerStyleToVariantMap(style));
    QVERIFY(back.buttonPainter);
    QCOMPARE(back.namedButtonStyle, style.namedButtonStyle);
    QCOMPARE(back.buttonSize, style.buttonSize);
    QCOMPARE(back.buttonSpacing, style.buttonSpacing);
    QCOMPARE(back.titleDoubleClick, style.titleDoubleClick);

    // The user's "glyph" paints named glyph buttons, while a theme that
    // authors "glyph" keeps the flat symbols its containers always had.
    preferences = {};
    preferences.containerButtonStyle = QStringLiteral("glyph");
    QVERIFY(resolveContainerStyle(plain, preferences).buttonPainter);
    QindaQt::Themes::ThemeSpec authored;
    authored.decoration.authored = true;
    authored.decoration.buttonStyle = QStringLiteral("glyph");
    style = resolveContainerStyle(authored, ChromePreferences{});
    QVERIFY(!style.buttonPainter);
    QCOMPARE(style.buttonStyle, ButtonStyle::Symbols);
    // A theme that authors a W19 style paints it on containers too.
    authored.decoration.buttonStyle = QStringLiteral("bevel");
    QCOMPARE(resolveContainerStyle(authored, ChromePreferences{}).namedButtonStyle,
             QStringLiteral("bevel"));
    // Flat and lights stay the built-in plates, and the capsule stays joined.
    preferences.containerButtonStyle = QStringLiteral("flat");
    QVERIFY(!resolveContainerStyle(plain, preferences).buttonPainter);
    preferences.containerButtonStyle = QStringLiteral("pills");
    preferences.containerButtonSpacing = QStringLiteral("roomy");
    QCOMPARE(resolveContainerStyle(plain, preferences).buttonSpacing, 0.0);
}

QTEST_MAIN(DecorationTitleOptionTests)
#include "tst_decoration_title_options.moc"
