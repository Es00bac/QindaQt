// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/themes/decoration_theme_loader.h"
#include "qindaqt/themes/theme_loader.h"

#include <QSet>
#include <QtTest>

using namespace QindaQt::Themes;

class DecorationThemeTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsEveryBuiltInDecorationTheme();
    void glassAuthorsATranslucentMaterialAndLunaClassicAuthorsColors();
    void colorsAreOptionalAndDeferToTheColorTheme();
    void rejectsInvalidValues();
    void catalogLoadsDirectoriesAndFindsById();
    void everyBuiltInColorThemeReferencesAShippedDecorationOrNone();
};

void DecorationThemeTests::loadsEveryBuiltInDecorationTheme()
{
    const auto results = DecorationThemeLoader::fromDirectory(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/decorations"));
    QCOMPARE(results.size(), 6);
    QSet<QString> ids;
    QSet<QString> families;
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error));
        ids.insert(result.theme.id);
        families.insert(result.theme.decoration.buttonPlacement + QLatin1Char('/')
                        + result.theme.decoration.buttonStyle + QLatin1Char('/')
                        + QString::number(result.theme.cornerRadius));
        const auto map = result.theme.toVariantMap();
        QCOMPARE(map.value(QStringLiteral("id")).toString(), result.theme.id);
        QVERIFY(map.contains(QStringLiteral("material")));
    }
    QCOMPARE(ids, QSet<QString>({QStringLiteral("glass"), QStringLiteral("slate"),
                                QStringLiteral("paper"), QStringLiteral("aurora"),
                                QStringLiteral("studio"), QStringLiteral("luna-classic")}));
    // Six distinct personalities (ADR-0207), not one arrangement six times.
    QCOMPARE(families.size(), 6);
}

void DecorationThemeTests::glassAuthorsATranslucentMaterialAndLunaClassicAuthorsColors()
{
    const auto glass = DecorationThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/decorations/glass.json"));
    QVERIFY2(glass.ok, qPrintable(glass.error));
    QCOMPARE(glass.theme.decoration.buttonPlacement, QStringLiteral("left"));
    QCOMPARE(glass.theme.decoration.buttonStyle, QStringLiteral("traffic-lights"));
    QVERIFY(glass.theme.decoration.hoverGlyphs);
    QVERIFY(glass.theme.titleMaterial.authored);
    QCOMPARE(glass.theme.titleMaterial.opacity, 0.84);
    QVERIFY(glass.theme.titleMaterial.blur);
    QVERIFY(glass.theme.titleMaterial.highlight);
    QCOMPARE(glass.theme.cornerRadius, 14);
    QCOMPARE(glass.theme.shadowExtent, 20.0);
    QCOMPARE(glass.theme.shadowOpacity, 0.28);
    QCOMPARE(glass.theme.memberHandleStyle, QStringLiteral("grip"));
    QCOMPARE(glass.theme.containerBadgeStyle, QStringLiteral("pill"));
    QVERIFY(!glass.theme.hasAuthoredColors());

    const auto luna = DecorationThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/decorations/luna-classic.json"));
    QVERIFY2(luna.ok, qPrintable(luna.error));
    QVERIFY(luna.theme.hasAuthoredColors());
    QCOMPARE(luna.theme.decoration.titleBarColor, QColor(QStringLiteral("#2B6FD4")));
    QCOMPARE(luna.theme.decoration.buttonStyle, QStringLiteral("glyph"));
    QCOMPARE(luna.theme.titleMaterial.opacity, 1.0);
}

void DecorationThemeTests::colorsAreOptionalAndDeferToTheColorTheme()
{
    constexpr auto document = R"json({
        "schemaVersion": 1, "id": "neutral", "name": "Neutral",
        "buttonPlacement": "right", "buttonStyle": "symbols"
    })json";
    const auto result = DecorationThemeLoader::fromJson(document, QStringLiteral("fixture"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(!result.theme.decoration.closeColor.isValid());
    QVERIFY(!result.theme.decoration.titleBarColor.isValid());
    QVERIFY(!result.theme.hasAuthoredColors());
    QVERIFY(result.theme.decoration.authored);
    QVERIFY(!result.theme.titleMaterial.authored);
    QCOMPARE(result.theme.titleMaterial.opacity, 1.0);
    const auto map = result.theme.toVariantMap();
    QVERIFY(!map.contains(QStringLiteral("closeColor")));
    QVERIFY(!map.contains(QStringLiteral("description")));
}

void DecorationThemeTests::rejectsInvalidValues()
{
    const auto rejects = [](const char *body, const char *reason) {
        const auto json = QByteArray(R"json({"schemaVersion": 1, "id": "x", "name": "X",)json")
            + body + "}";
        const auto result = DecorationThemeLoader::fromJson(json, QStringLiteral("fixture"));
        QVERIFY2(!result.ok, body);
        QVERIFY2(result.error.contains(QString::fromLatin1(reason)), qPrintable(result.error));
    };
    rejects(R"("buttonPlacement": "middle")", "buttonPlacement");
    rejects(R"("buttonStyle": "sparkles")", "buttonStyle");
    rejects(R"("closeColor": "nope")", "closeColor");
    rejects(R"("material": {"opacity": 2})", "opacity");
    rejects(R"("material": {"blur": "yes"})", "material.blur");
    rejects(R"("cornerRadius": 99)", "cornerRadius");
    rejects(R"("shadow": {"extent": 100})", "extent");
    rejects(R"("memberHandle": "rope")", "memberHandle");
    rejects(R"("containerBadge": "hexagon")", "containerBadge");
    const auto badId = DecorationThemeLoader::fromJson(
        R"json({"schemaVersion": 1, "id": "../x", "name": "X"})json", QStringLiteral("fixture"));
    QVERIFY(!badId.ok);
    const auto badVersion = DecorationThemeLoader::fromJson(
        R"json({"schemaVersion": 2, "id": "x", "name": "X"})json", QStringLiteral("fixture"));
    QVERIFY(!badVersion.ok);
}

void DecorationThemeTests::catalogLoadsDirectoriesAndFindsById()
{
    QString error;
    const auto documents = DecorationThemeLoader::loadDirectories(
        {QStringLiteral(QINDAQT_SOURCE_DIR "/data/decorations"),
         QStringLiteral(QINDAQT_SOURCE_DIR "/data/decorations")},
        &error);
    QVERIFY2(documents.has_value(), qPrintable(error));
    // The same directory twice contributes each id once.
    QCOMPARE(documents->size(), 6);
    QVERIFY(DecorationThemeLoader::find(*documents, QStringLiteral("aurora")).has_value());
    QVERIFY(!DecorationThemeLoader::find(*documents, QStringLiteral("missing")).has_value());
    QVERIFY(DecorationThemeLoader::loadDirectories({QStringLiteral("/nonexistent")})
                ->isEmpty());
}

void DecorationThemeTests::everyBuiltInColorThemeReferencesAShippedDecorationOrNone()
{
    const auto documents = DecorationThemeLoader::loadDirectories(
        {QStringLiteral(QINDAQT_SOURCE_DIR "/data/decorations")});
    QVERIFY(documents.has_value());
    int referencing = 0;
    for (const auto &result : ThemeLoader::fromDirectory(
             QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"))) {
        QVERIFY2(result.ok, qPrintable(result.error));
        if (result.theme.decorationTheme.isEmpty()) {
            continue;
        }
        ++referencing;
        QVERIFY2(DecorationThemeLoader::find(*documents, result.theme.decorationTheme).has_value(),
                 qPrintable(result.theme.id + QLatin1String(" references ")
                            + result.theme.decorationTheme));
    }
    QCOMPARE(referencing, 6);
}

QTEST_GUILESS_MAIN(DecorationThemeTests)
#include "tst_decoration_themes.moc"
