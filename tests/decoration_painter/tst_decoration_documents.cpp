// SPDX-License-Identifier: GPL-3.0-or-later
// Decoration documents over color themes (ADR-0207): selection precedence,
// material carriage through the chrome map, and the byte-identical default.
#include "qindaqt/decoration_painter/decoration_painter.h"
#include "qindaqt/themes/decoration_theme_loader.h"
#include "qindaqt/themes/theme_loader.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Decoration;

namespace {

const QString DataRoot = QStringLiteral(QINDAQT_SOURCE_DIR "/data");

Themes::ThemeSpec loadTheme(const QString &id)
{
    const auto result = Themes::ThemeLoader::fromFile(
        DataRoot + QStringLiteral("/themes/") + id + QStringLiteral(".json"));
    if (!result.ok) {
        qFatal("theme %s: %s", qPrintable(id), qPrintable(result.error));
    }
    return result.theme;
}

QVector<Themes::DecorationThemeSpec> installedDocuments()
{
    return Themes::DecorationThemeLoader::loadDirectories(
               {DataRoot + QStringLiteral("/decorations")})
        .value_or(QVector<Themes::DecorationThemeSpec>{});
}

} // namespace

class DecorationDocumentTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void schemaV1ThemesResolveTheShippedChrome();
    void themePairingSelectsItsDocument();
    void userChoiceOverridesThePairing();
    void unknownChoiceFallsBackToThePairing();
    void documentMaterialRidesTheChromeMap();
    void containerStyleCarriesTheDocumentMaterial();
    void userArrangementStillWinsOverTheDocument();
    void lunaDocumentKeepsWornBarsOverLunaThemes();
};

void DecorationDocumentTests::schemaV1ThemesResolveTheShippedChrome()
{
    const auto theme = loadTheme(QStringLiteral("qinda-dark"));
    QCOMPARE(theme.schemaVersion, 1);
    const auto documents = installedDocuments();
    QVERIFY(!selectDecorationTheme(theme, documents, QStringLiteral("theme")).has_value());
    const auto chrome = resolveWindowChrome(theme, std::nullopt, ChromePreferences{});
    QCOMPARE(chrome.toVariantMap(), resolveWindowChrome(theme, ChromePreferences{}).toVariantMap());
    QCOMPARE(chrome.titleOpacity, 1.0);
    QVERIFY(!chrome.titleBlur);
    QCOMPARE(chrome.cornerRadius, DecorationCornerRadius);
    QCOMPARE(chrome.shadowExtent, 12.0);
    // No material keys leak into the published map for a v1 theme.
    const auto map = chrome.toVariantMap();
    for (const auto key : {"titleOpacity", "titleBlur", "titleHighlight", "titleTint",
                           "cornerRadius", "shadowExtent", "shadowOpacity", "handleStyle"}) {
        QVERIFY2(!map.contains(QString::fromLatin1(key)), key);
    }
    QCOMPARE(containerStyleToVariantMap(
                 resolveContainerStyle(theme, std::nullopt, ChromePreferences{})),
             containerStyleToVariantMap(resolveContainerStyle(theme, ChromePreferences{})));
    QCOMPARE(resolveContainerStyle(theme, ChromePreferences{}).material,
             HybridChrome::ChromeMaterial{});
}

void DecorationDocumentTests::themePairingSelectsItsDocument()
{
    const auto theme = loadTheme(QStringLiteral("qinda-glass-dark"));
    QCOMPARE(theme.decorationTheme, QStringLiteral("glass"));
    const auto chosen = selectDecorationTheme(theme, installedDocuments(), QStringLiteral("theme"));
    QVERIFY(chosen.has_value());
    QCOMPARE(chosen->id, QStringLiteral("glass"));
    // An empty preference behaves as "theme".
    QCOMPARE(selectDecorationTheme(theme, installedDocuments(), QString())->id,
             QStringLiteral("glass"));
}

void DecorationDocumentTests::userChoiceOverridesThePairing()
{
    const auto theme = loadTheme(QStringLiteral("qinda-glass-dark"));
    const auto chosen = selectDecorationTheme(theme, installedDocuments(),
                                              QStringLiteral("luna-classic"));
    QVERIFY(chosen.has_value());
    QCOMPARE(chosen->id, QStringLiteral("luna-classic"));
}

void DecorationDocumentTests::unknownChoiceFallsBackToThePairing()
{
    const auto theme = loadTheme(QStringLiteral("qinda-paper"));
    const auto chosen = selectDecorationTheme(theme, installedDocuments(),
                                              QStringLiteral("not-installed"));
    QVERIFY(chosen.has_value());
    QCOMPARE(chosen->id, QStringLiteral("paper"));
    // A v1 theme with an unknown choice resolves to no document at all.
    QVERIFY(!selectDecorationTheme(loadTheme(QStringLiteral("qinda-light")),
                                   installedDocuments(), QStringLiteral("not-installed"))
                 .has_value());
}

void DecorationDocumentTests::documentMaterialRidesTheChromeMap()
{
    const auto theme = loadTheme(QStringLiteral("qinda-glass-dark"));
    const auto documents = installedDocuments();
    const auto glass = Themes::DecorationThemeLoader::find(documents, QStringLiteral("glass"));
    QVERIFY(glass.has_value());
    const auto chrome = resolveWindowChrome(theme, glass, ChromePreferences{});
    QVERIFY(chrome.titleOpacity < 1.0);
    QVERIFY(chrome.titleBlur);
    QVERIFY(chrome.titleHighlight);
    QCOMPARE(chrome.cornerRadius, glass->cornerRadius);
    QCOMPARE(chrome.shadowExtent, glass->shadowExtent);
    QCOMPARE(chrome.handleStyle, glass->memberHandleStyle);
    // The compositor publishes the chrome as a variant map; the decoration
    // plugin decodes the same material on the other side.
    const auto decoded = DecorationChrome::fromVariantMap(chrome.toVariantMap());
    QCOMPARE(decoded.titleOpacity, chrome.titleOpacity);
    QCOMPARE(decoded.titleBlur, chrome.titleBlur);
    QCOMPARE(decoded.titleHighlight, chrome.titleHighlight);
    QCOMPARE(decoded.titleTint, chrome.titleTint);
    QCOMPARE(decoded.cornerRadius, chrome.cornerRadius);
    QCOMPARE(decoded.shadowExtent, chrome.shadowExtent);
    QCOMPARE(decoded.shadowOpacity, chrome.shadowOpacity);
    QCOMPARE(decoded.handleStyle, chrome.handleStyle);
    const auto style = decorationVisualStyleFor(chrome, false);
    QCOMPARE(style.cornerRadius, chrome.cornerRadius);
    QCOMPARE(style.shadowExtent, chrome.shadowExtent);
    QCOMPARE(decorationFrameRadius(chrome, true), 0.0);
    QCOMPARE(decorationFrameRadius(chrome, false), chrome.cornerRadius);
    // Out-of-range or mistyped material values decode to the defaults.
    QVariantMap poisoned = chrome.toVariantMap();
    poisoned.insert(QStringLiteral("titleOpacity"), 4.0);
    poisoned.insert(QStringLiteral("cornerRadius"), QStringLiteral("round"));
    poisoned.insert(QStringLiteral("handleStyle"), QStringLiteral("lever"));
    const auto tolerant = DecorationChrome::fromVariantMap(poisoned);
    QCOMPARE(tolerant.titleOpacity, 1.0);
    QCOMPARE(tolerant.cornerRadius, DecorationCornerRadius);
    QCOMPARE(tolerant.handleStyle, QStringLiteral("grip"));
}

void DecorationDocumentTests::containerStyleCarriesTheDocumentMaterial()
{
    const auto theme = loadTheme(QStringLiteral("qinda-glass-light"));
    const auto documents = installedDocuments();
    const auto glass = Themes::DecorationThemeLoader::find(documents, QStringLiteral("glass"));
    QVERIFY(glass.has_value());
    const auto style = resolveContainerStyle(theme, glass, ChromePreferences{});
    QVERIFY(style.material.opacity < 1.0);
    QVERIFY(style.material.highlight);
    QCOMPARE(style.material.squareBadge, glass->containerBadgeStyle == QLatin1String("square"));
    const auto roundTrip = containerStyleFromVariantMap(containerStyleToVariantMap(style));
    QCOMPARE(roundTrip.material, style.material);
    // The theme's own container surface applies without a document too.
    const auto themed = resolveContainerStyle(theme, std::nullopt, ChromePreferences{});
    QCOMPARE(themed.material, containerMaterialForTheme(theme));
}

void DecorationDocumentTests::userArrangementStillWinsOverTheDocument()
{
    const auto theme = loadTheme(QStringLiteral("qinda-slate"));
    const auto documents = installedDocuments();
    const auto luna = Themes::DecorationThemeLoader::find(documents,
                                                          QStringLiteral("luna-classic"));
    QVERIFY(luna.has_value());
    QCOMPARE(luna->decoration.buttonPlacement, QStringLiteral("right"));
    ChromePreferences preferences;
    preferences.windowButtonSide = QStringLiteral("left");
    preferences.containerButtonStyle = QStringLiteral("traffic-lights");
    const auto chrome = resolveWindowChrome(theme, luna, preferences);
    QCOMPARE(chrome.buttonSide, QStringLiteral("left"));
    QCOMPARE(chrome.buttonStyle, luna->decoration.buttonStyle);
    const auto style = resolveContainerStyle(theme, luna, preferences);
    QCOMPARE(style.buttonStyle, HybridChrome::ButtonStyle::TrafficLights);
}

void DecorationDocumentTests::lunaDocumentKeepsWornBarsOverLunaThemes()
{
    const auto documents = installedDocuments();
    const auto luna = Themes::DecorationThemeLoader::find(documents,
                                                          QStringLiteral("luna-classic"));
    QVERIFY(luna.has_value());
    // The classic document authors the worn Luna bars itself, so it turns
    // them on over a theme that never had them; the material stays opaque
    // and unblurred, the frame square, the badge square.
    const auto slate = resolveWindowChrome(loadTheme(QStringLiteral("qinda-slate")), luna,
                                           ChromePreferences{});
    QVERIFY(slate.wornLuna());
    QCOMPARE(slate.titleBar, luna->decoration.titleBarColor);
    QCOMPARE(slate.titleOpacity, 1.0);
    QVERIFY(!slate.titleBlur);
    QCOMPARE(slate.cornerRadius, luna->cornerRadius);
    QCOMPARE(slate.handleStyle, QStringLiteral("plain"));
    QVERIFY(resolveContainerStyle(loadTheme(QStringLiteral("qinda-slate")), luna,
                                  ChromePreferences{})
                .material.squareBadge);
    // A material document over a worn Luna theme owns the title surface:
    // the glass bar replaces the worn one instead of blending under it.
    const auto glass = Themes::DecorationThemeLoader::find(documents, QStringLiteral("glass"));
    QVERIFY(glass.has_value());
    const auto bliss = loadTheme(QStringLiteral("qinda-bliss"));
    QVERIFY(resolveWindowChrome(bliss, std::nullopt, ChromePreferences{}).wornLuna());
    const auto glassed = resolveWindowChrome(bliss, glass, ChromePreferences{});
    QVERIFY(!glassed.wornLuna());
    QVERIFY(glassed.titleOpacity < 1.0);
}

QTEST_GUILESS_MAIN(DecorationDocumentTests)
#include "tst_decoration_documents.moc"
