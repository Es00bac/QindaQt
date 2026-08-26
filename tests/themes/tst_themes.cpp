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
    void rejectsInvalidDecorationValues();
    void catalogSwitchesTheme();
};

void ThemeTests::loadsEveryBuiltInTheme()
{
    const auto results = ThemeLoader::fromDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    QCOMPARE(results.size(), 5);
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.theme.colors.value(QStringLiteral("text")).isValid());
    }
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
