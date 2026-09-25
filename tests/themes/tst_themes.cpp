// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/themes/decoration_theme_spec.h"
#include "qindaqt/themes/theme_catalog.h"
#include "qindaqt/themes/theme_loader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace QindaQt::Themes;

class ThemeTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsEveryBuiltInTheme();
    void requiresSemanticColorTokens();
    void qindaMacosDefinesDecorationFlow();
    void qindaBlissDefinesTheClassicBlueChrome();
    void experienceThemesAuthorTheirTitleBehaviour();
    void titleBehaviourDefaultsAndRejectsUnknownValues();
    void everyBuiltInThemeAuthorsAWindowDecoration();
    void rejectsInvalidDecorationValues();
    void acceptsNamedButtonStylesAndRejectsUnknownOnes();
    void catalogSwitchesTheme();
    void loadsSchemaV2SurfacesRadiiMotionAndAccent();
    void schemaV1DocumentsLoadWithSchemaV2Defaults();
    void schemaV1DocumentsRejectSchemaV2Keys();
    void rejectsInvalidSchemaV2Values();
    void everyBuiltInThemeRoundTripsItsOwnDocument();
};

void ThemeTests::loadsEveryBuiltInTheme()
{
    const auto results = ThemeLoader::fromDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    QCOMPARE(results.size(), 16);
    int schemaV2 = 0;
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.theme.colors.value(QStringLiteral("text")).isValid());
        schemaV2 += result.theme.schemaVersion == 2 ? 1 : 0;
    }
    // ADR-0206: the six theming-v2 themes author surfaces; the six originals
    // stay schema v1 and load unchanged. ADR-0268 adds four v2 experience
    // themes (Daylight, Marigold, Classic Grey, Graphite).
    QCOMPARE(schemaV2, 10);
}

void ThemeTests::qindaBlissDefinesTheClassicBlueChrome()
{
    // ADR-0268: the XP-like experience's theme keeps its id and blue title
    // colors, paints the bar clean, and uses the W19 blue tiles.
    const auto result = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-bliss.json"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.theme.name, QStringLiteral("Qinda Classic Blue"));
    QCOMPARE(result.theme.decoration.buttonPlacement, QStringLiteral("right"));
    QCOMPARE(result.theme.decoration.buttonStyle, QStringLiteral("blue-tiles"));
    QVERIFY(!result.theme.decoration.titleWear);
    QVERIFY(result.theme.decoration.titleBarColor.isValid());
    QVERIFY(result.theme.decoration.titleBarInactiveColor.isValid());
    QVERIFY(result.theme.decoration.restoreColor.isValid());
    // ADR-0129: a declared decoration object is authored.
    QVERIFY(result.theme.decoration.authored);
    QCOMPARE(result.theme.decoration.closeColor,
             QColor(QStringLiteral("#2d6be4")));
    QCOMPARE(result.theme.decoration.minimizeColor,
             QColor(QStringLiteral("#e23b3b")));
    QCOMPARE(result.theme.decoration.maximizeColor,
             QColor(QStringLiteral("#3da53d")));
    QCOMPARE(result.theme.decoration.restoreColor,
             QColor(QStringLiteral("#e87bd0")));

}

void ThemeTests::everyBuiltInThemeAuthorsAWindowDecoration()
{
    const auto results = ThemeLoader::fromDirectory(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    QSet<QString> visualFamilies;
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY2(result.theme.decoration.authored,
                 qPrintable(result.theme.id + QStringLiteral(" has no decoration")));
        visualFamilies.insert(result.theme.decoration.buttonPlacement
                              + QLatin1Char('/')
                              + result.theme.decoration.buttonStyle
                              + QLatin1Char('/')
                              + result.theme.decoration.tabDirection);
    }
    QVERIFY2(visualFamilies.size() >= 4,
             "the bundled themes must provide several genuinely distinct window decorations");
}

void ThemeTests::qindaMacosDefinesDecorationFlow()
{
    const auto result = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-macos.json"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.theme.name, QStringLiteral("Qinda Mist"));
    QCOMPARE(result.theme.decoration.buttonPlacement, QStringLiteral("left"));
    QCOMPARE(result.theme.decoration.tabDirection, QStringLiteral("right-to-left"));
    QCOMPARE(result.theme.decoration.buttonStyle, QStringLiteral("traffic-lights"));
    QVERIFY(result.theme.decoration.hoverGlyphs);
}

void ThemeTests::rejectsInvalidDecorationValues()
{
    constexpr auto invalid = R"json({
        "schemaVersion": 1,
        "id": "bad-decoration",
        "name": "Bad Decoration",
        "variant": "dark",
        "colors": {
            "canvas": "#101010", "surface": "#202020", "surfaceRaised": "#303030",
            "border": "#404040", "text": "#ffffff", "textMuted": "#aaaaaa",
            "accent": "#80c0b0", "accentText": "#102020", "danger": "#ff6060"
        },
        "decoration": {"buttonPlacement": "middle"}
    })json";
    const auto result = ThemeLoader::fromJson(invalid, QStringLiteral("fixture"));
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("decoration")));
}

void ThemeTests::acceptsNamedButtonStylesAndRejectsUnknownOnes()
{
    // ADR-0264: a color theme's decoration block may name any style in the
    // shared list; an unknown name still fails the whole theme.
    const auto theme = [](const QString &style) {
        return QByteArray(R"json({
            "schemaVersion": 1, "id": "styled", "name": "Styled", "variant": "dark",
            "colors": {
                "canvas": "#101010", "surface": "#202020", "surfaceRaised": "#303030",
                "border": "#404040", "text": "#ffffff", "textMuted": "#aaaaaa",
                "accent": "#80c0b0", "accentText": "#102020", "danger": "#ff6060"
            },
            "decoration": {"buttonStyle": ")json")
            + style.toUtf8() + "\"}}";
    };
    for (const QString &style : DecorationThemeTokens::buttonStyles()) {
        const auto result = ThemeLoader::fromJson(theme(style), QStringLiteral("fixture"));
        QVERIFY2(result.ok, qPrintable(style + QStringLiteral(": ") + result.error));
        QCOMPARE(result.theme.decoration.buttonStyle, style);
    }
    const auto unknown = ThemeLoader::fromJson(theme(QStringLiteral("sparkles")),
                                               QStringLiteral("fixture"));
    QVERIFY(!unknown.ok);
    QVERIFY(unknown.error.contains(QStringLiteral("decoration")));
}

// ADR-0268: each experience theme carries its W19 button style and the
// title-bar behaviour its desktop expects; no theme name is a vendor's.
void ThemeTests::experienceThemesAuthorTheirTitleBehaviour()
{
    const struct {
        const char *id;
        const char *style;
        const char *doubleClick;
        const char *minimize;
        bool wear;
    } experiences[] = {
        {"qinda-bliss", "blue-tiles", "", "minimize", false},
        {"qinda-daylight", "wide", "", "minimize", true},
        {"qinda-marigold", "tab", "roll-up", "minimize", false},
        {"qinda-classic-grey", "bevel", "", "roll-up", false},
        {"qinda-graphite", "bold", "", "minimize", false},
    };
    for (const auto &experience : experiences) {
        const QString id = QString::fromLatin1(experience.id);
        const auto result = ThemeLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + id + QStringLiteral(".json"));
        QVERIFY2(result.ok, qPrintable(result.error));
        const auto &decoration = result.theme.decoration;
        QCOMPARE(decoration.buttonStyle, QString::fromLatin1(experience.style));
        QCOMPARE(decoration.titleDoubleClick, QString::fromLatin1(experience.doubleClick));
        QCOMPARE(decoration.minimizeAction, QString::fromLatin1(experience.minimize));
        QCOMPARE(decoration.titleWear, experience.wear);
        // The keys round-trip only where the document authors them.
        const auto map = decoration.toVariantMap();
        QCOMPARE(map.contains(QStringLiteral("titleDoubleClick")),
                 !decoration.titleDoubleClick.isEmpty());
        QCOMPARE(map.contains(QStringLiteral("minimizeAction")),
                 decoration.minimizeAction != QLatin1String("minimize"));
        QCOMPARE(map.contains(QStringLiteral("titleWear")), !decoration.titleWear);
    }

    const auto results = ThemeLoader::fromDirectory(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error));
        for (const char *mark : {"Windows", "Microsoft", "Luna", "Bliss", "macOS", "Apple",
                                 "BeOS", "NeXT"}) {
            QVERIFY2(!result.theme.name.contains(QString::fromLatin1(mark)),
                     qPrintable(result.theme.id + QStringLiteral(" names ")
                                + QString::fromLatin1(mark)));
        }
    }
}

void ThemeTests::titleBehaviourDefaultsAndRejectsUnknownValues()
{
    const auto theme = [](const QByteArray &decoration) {
        return QByteArray(R"json({
            "schemaVersion": 1, "id": "behaving", "name": "Behaving", "variant": "dark",
            "colors": {
                "canvas": "#101010", "surface": "#202020", "surfaceRaised": "#303030",
                "border": "#404040", "text": "#ffffff", "textMuted": "#aaaaaa",
                "accent": "#80c0b0", "accentText": "#102020", "danger": "#ff6060"
            },
            "decoration": )json")
            + decoration + "}";
    };
    // Absent keys: KWin's own double-click, a real minimize, the weathered
    // bar ADR-0124 paints for an authored title color.
    const auto plain = ThemeLoader::fromJson(theme(R"({"buttonStyle": "glyph"})"),
                                             QStringLiteral("fixture"));
    QVERIFY2(plain.ok, qPrintable(plain.error));
    QVERIFY(plain.theme.decoration.titleDoubleClick.isEmpty());
    QCOMPARE(plain.theme.decoration.minimizeAction, QStringLiteral("minimize"));
    QVERIFY(plain.theme.decoration.titleWear);
    const auto map = plain.theme.decoration.toVariantMap();
    QVERIFY(!map.contains(QStringLiteral("titleDoubleClick")));
    QVERIFY(!map.contains(QStringLiteral("minimizeAction")));
    QVERIFY(!map.contains(QStringLiteral("titleWear")));

    for (const char *action : {"maximize", "roll-up", "minimize"}) {
        const auto chosen = ThemeLoader::fromJson(
            theme(QByteArray(R"({"titleDoubleClick": ")") + action + "\"}"),
            QStringLiteral("fixture"));
        QVERIFY2(chosen.ok, qPrintable(chosen.error));
        QCOMPARE(chosen.theme.decoration.titleDoubleClick, QString::fromLatin1(action));
    }
    for (const char *invalid : {R"({"titleDoubleClick": "shade"})",
                                R"({"minimizeAction": "hide"})"}) {
        const auto rejected = ThemeLoader::fromJson(theme(invalid), QStringLiteral("fixture"));
        QVERIFY2(!rejected.ok, invalid);
        QVERIFY(rejected.error.contains(QStringLiteral("decoration")));
    }
}

void ThemeTests::requiresSemanticColorTokens()
{
    constexpr auto invalid = R"json({
        "schemaVersion": 1,
        "id": "empty",
        "name": "Empty",
        "variant": "dark",
        "colors": {"canvas": "#000000"}
    })json";
    const auto result = ThemeLoader::fromJson(invalid, QStringLiteral("fixture"));
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("color token")));
}

void ThemeTests::catalogSwitchesTheme()
{
    ThemeCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"), &error),
             qPrintable(error));
    QVERIFY(catalog.selectById(QStringLiteral("qinda-dusk")));
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(), QStringLiteral("qinda-dusk"));
}

void ThemeTests::loadsSchemaV2SurfacesRadiiMotionAndAccent()
{
    const auto result = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-glass-light.json"));
    QVERIFY2(result.ok, qPrintable(result.error));
    const auto &theme = result.theme;
    QCOMPARE(theme.schemaVersion, 2);
    QCOMPARE(theme.decorationTheme, QStringLiteral("glass"));
    QCOMPARE(theme.accentMode, QStringLiteral("fixed"));
    const auto panel = theme.surface(QString(SurfaceNames::Panel));
    QVERIFY(panel.authored);
    QCOMPARE(panel.opacity, 0.78);
    QVERIFY(panel.blur);
    QVERIFY(panel.highlight);
    QCOMPARE(panel.tint, QColor(QStringLiteral("#F1F4F8")));
    QCOMPARE(panel.border, 0.6);
    // desktopIcons is authored fully transparent: icons sit on the wallpaper.
    QCOMPARE(theme.surface(QString(SurfaceNames::DesktopIcons)).opacity, 0.0);
    QCOMPARE(theme.surfaceRadius(QString(SurfaceNames::Panel)), 16);
    QCOMPARE(theme.surfaceRadius(QString(SurfaceNames::DesktopIcons)), theme.cornerRadius);
    const auto rollup = theme.motion(QString(MotionNames::Rollup));
    QVERIFY(rollup.authored);
    QCOMPARE(rollup.duration, 200);
    QCOMPARE(rollup.easing, QStringLiteral("emphasized"));
    // The round trip carries only authored v2 entries.
    const auto map = theme.toVariantMap();
    QVERIFY(map.contains(QStringLiteral("surfaces")));
    QVERIFY(map.contains(QStringLiteral("motion")));
    QCOMPARE(map.value(QStringLiteral("decorationTheme")).toString(), QStringLiteral("glass"));
    QCOMPARE(map.value(QStringLiteral("accent")).toMap().value(QStringLiteral("mode")).toString(),
             QStringLiteral("fixed"));
}

void ThemeTests::schemaV1DocumentsLoadWithSchemaV2Defaults()
{
    const auto result = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dusk.json"));
    QVERIFY2(result.ok, qPrintable(result.error));
    const auto &theme = result.theme;
    QCOMPARE(theme.schemaVersion, 1);
    QVERIFY(theme.surfaces.isEmpty());
    QVERIFY(theme.decorationTheme.isEmpty());
    // Unauthored surfaces reproduce v1: opaque, borders on, blur following
    // blurEnabled for panels, popups and menus only (ADR-0120).
    const auto panel = theme.surface(QString(SurfaceNames::Panel));
    QVERIFY(!panel.authored);
    QCOMPARE(panel.opacity, 1.0);
    QCOMPARE(panel.blur, theme.blurEnabled);
    QVERIFY(!theme.surface(QString(SurfaceNames::Decoration)).blur);
    QCOMPARE(theme.surfaceRadius(QString(SurfaceNames::Menu)), theme.cornerRadius);
    QCOMPARE(theme.motion(QString(MotionNames::Popup)).duration, theme.motionDuration);
    QCOMPARE(theme.motion(QString(MotionNames::Popup)).easing, QStringLiteral("standard"));
    // The v1 map never grows v2 keys.
    const auto map = theme.toVariantMap();
    QVERIFY(!map.contains(QStringLiteral("surfaces")));
    QVERIFY(!map.contains(QStringLiteral("accent")));
    QVERIFY(!map.contains(QStringLiteral("decorationTheme")));
}

void ThemeTests::schemaV1DocumentsRejectSchemaV2Keys()
{
    constexpr auto document = R"json({
        "schemaVersion": 1,
        "id": "v1-with-surfaces",
        "name": "V1",
        "variant": "dark",
        "colors": {
            "canvas": "#101010", "surface": "#202020", "surfaceRaised": "#303030",
            "border": "#404040", "text": "#ffffff", "textMuted": "#aaaaaa",
            "accent": "#80c0b0", "accentText": "#102020", "danger": "#ff6060"
        },
        "surfaces": {"panel": {"opacity": 0.5}}
    })json";
    const auto result = ThemeLoader::fromJson(document, QStringLiteral("fixture"));
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("schema version 1")));
}

void ThemeTests::rejectsInvalidSchemaV2Values()
{
    const auto build = [](const char *extra) {
        return QByteArray(R"json({
            "schemaVersion": 2,
            "id": "v2-fixture",
            "name": "V2",
            "variant": "dark",
            "colors": {
                "canvas": "#101010", "surface": "#202020", "surfaceRaised": "#303030",
                "border": "#404040", "text": "#ffffff", "textMuted": "#aaaaaa",
                "accent": "#80c0b0", "accentText": "#102020", "danger": "#ff6060"
            },)json") + extra + "}";
    };
    const auto rejects = [&build](const char *extra, const char *reason) {
        const auto result = ThemeLoader::fromJson(build(extra), QStringLiteral("fixture"));
        QVERIFY2(!result.ok, extra);
        QVERIFY2(result.error.contains(QString::fromLatin1(reason)), qPrintable(result.error));
    };
    rejects(R"("surfaces": {"sidebar": {"opacity": 0.5}})", "unknown or malformed surface");
    rejects(R"("surfaces": {"panel": {"opacity": 1.5}})", "opacity");
    rejects(R"("surfaces": {"panel": {"tint": "not-a-color"}})", "tint");
    rejects(R"("radii": {"panel": 40})", "radii.panel");
    rejects(R"("motion": {"popup": {"easing": "bouncy"}})", "easing");
    rejects(R"("motion": {"popup": {"duration": 5000}})", "duration");
    rejects(R"("motion": {"flip": {"duration": 100}})", "unknown or malformed motion");
    rejects(R"("accent": {"mode": "rainbow"})", "accent.mode");
    rejects(R"("decorationTheme": "../escape")", "decorationTheme");
    // A minimal v2 document with no optional section is valid.
    const auto minimal = ThemeLoader::fromJson(build(R"("cornerRadius": 8)"),
                                               QStringLiteral("fixture"));
    QVERIFY2(minimal.ok, qPrintable(minimal.error));
    QCOMPARE(minimal.theme.surface(QString(SurfaceNames::Panel)).opacity, 1.0);
}

void ThemeTests::everyBuiltInThemeRoundTripsItsOwnDocument()
{
    // Strict round trip: every key the document authors comes back from
    // toVariantMap with the same value, so the v2 sections cannot decay into
    // silently dropped fields.
    const QDir directory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    for (const auto &file : directory.entryList({QStringLiteral("*.json")}, QDir::Files)) {
        QFile source(directory.filePath(file));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const auto document = QJsonDocument::fromJson(source.readAll()).object();
        const auto loaded = ThemeLoader::fromFile(directory.filePath(file));
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        const auto map = loaded.theme.toVariantMap();
        for (auto it = document.constBegin(); it != document.constEnd(); ++it) {
            QVERIFY2(map.contains(it.key()), qPrintable(file + QLatin1String(": ") + it.key()));
            if (it.key() == QLatin1String("surfaces") || it.key() == QLatin1String("motion")
                || it.key() == QLatin1String("radii")) {
                const auto authored = it.value().toObject();
                const auto loadedSection = map.value(it.key()).toMap();
                QCOMPARE(loadedSection.keys().size(), authored.keys().size());
                for (auto entry = authored.constBegin(); entry != authored.constEnd(); ++entry) {
                    QVERIFY2(loadedSection.contains(entry.key()),
                             qPrintable(file + QLatin1String(": ") + it.key() + QLatin1Char('.')
                                        + entry.key()));
                }
            }
        }
    }
}

QTEST_GUILESS_MAIN(ThemeTests)
#include "tst_themes.moc"
