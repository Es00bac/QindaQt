// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/themes/theme_catalog.h"
#include "qindaqt/themes/theme_loader.h"

#include <QtTest>

using namespace QindaQt::Themes;

class ThemeTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsEveryBuiltInTheme();
    void requiresSemanticColorTokens();
    void qindaMacosDefinesDecorationFlow();
    void qindaBlissDefinesWornLunaChrome();
    void rejectsInvalidDecorationValues();
    void catalogSwitchesTheme();
};

void ThemeTests::loadsEveryBuiltInTheme()
{
    const auto results = ThemeLoader::fromDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    QCOMPARE(results.size(), 6);
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.theme.colors.value(QStringLiteral("text")).isValid());
    }
}

void ThemeTests::qindaBlissDefinesWornLunaChrome()
{
    const auto result = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-bliss.json"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.theme.name, QStringLiteral("QindaQt Bliss"));
    QCOMPARE(result.theme.decoration.buttonPlacement, QStringLiteral("right"));
    QCOMPARE(result.theme.decoration.buttonStyle, QStringLiteral("glyph"));
    QVERIFY(result.theme.decoration.titleBarColor.isValid());
    QVERIFY(result.theme.decoration.titleBarInactiveColor.isValid());
    QVERIFY(result.theme.decoration.restoreColor.isValid());
    // ADR-0129: a declared decoration object is authored; a theme without one
    // keeps the Qinda macOS container arrangement.
    QVERIFY(result.theme.decoration.authored);
    const auto unauthored = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(unauthored.ok, qPrintable(unauthored.error));
    QVERIFY(!unauthored.theme.decoration.authored);
    QCOMPARE(result.theme.decoration.closeColor,
             QColor(QStringLiteral("#2d6be4")));
    QCOMPARE(result.theme.decoration.minimizeColor,
             QColor(QStringLiteral("#e23b3b")));
    QCOMPARE(result.theme.decoration.maximizeColor,
             QColor(QStringLiteral("#3da53d")));
    QCOMPARE(result.theme.decoration.restoreColor,
             QColor(QStringLiteral("#e87bd0")));

    // Themes without authored Luna fields keep the classic rendering: the
    // optional colors stay invalid and round-trip drops the absent keys.
    const auto classic = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-light.json"));
    QVERIFY2(classic.ok, qPrintable(classic.error));
    QVERIFY(!classic.theme.decoration.titleBarColor.isValid());
    QVERIFY(!classic.theme.decoration.restoreColor.isValid());
    QVERIFY(!classic.theme.decoration.toVariantMap()
                 .contains(QStringLiteral("titleBarColor")));
}

void ThemeTests::qindaMacosDefinesDecorationFlow()
{
    const auto result = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-macos.json"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.theme.name, QStringLiteral("Qinda macOS"));
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

QTEST_GUILESS_MAIN(ThemeTests)
#include "tst_themes.moc"
