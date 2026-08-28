// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_values.h"

#include <QtTest>

using namespace QindaQt::Apps::SettingsAppearance;

namespace {

QVariantMap validMap()
{
    return AppearanceValues{}.toVariantMap();
}

} // namespace

class AppearanceValuesTests final : public QObject {
    Q_OBJECT

private slots:
    void tokenRoundTripsCoverEveryEnumeratedValue();
    void decodeAcceptsCanonicalSchemaShapes();
    void decodeRejectsWrongTypesAndUnknownTokens();
    void validationRequiresInstalledThemesAndBounds();
    void scopedKeysMatchSchemaKeys();
};

void AppearanceValuesTests::tokenRoundTripsCoverEveryEnumeratedValue()
{
    for (const auto scheme : {ColorSchemePreference::System,
                              ColorSchemePreference::Light,
                              ColorSchemePreference::Dark}) {
        QCOMPARE(colorSchemeFromToken(colorSchemeToken(scheme)), scheme);
    }
    for (const auto mode : {WallpaperMode::Scaled, WallpaperMode::Centered,
                            WallpaperMode::Tiled}) {
        QCOMPARE(wallpaperModeFromToken(wallpaperModeToken(mode)), mode);
    }
    for (const auto hinting : {FontHinting::None, FontHinting::Slight,
                               FontHinting::Medium, FontHinting::Full}) {
        QCOMPARE(fontHintingFromToken(fontHintingToken(hinting)), hinting);
    }
    for (const auto order : {SubpixelOrder::None, SubpixelOrder::Rgb,
                             SubpixelOrder::Bgr, SubpixelOrder::Vrgb,
                             SubpixelOrder::Vbgr}) {
        QCOMPARE(subpixelOrderFromToken(subpixelOrderToken(order)), order);
    }
    QCOMPARE(colorSchemeFromToken(QStringLiteral("sepia")), std::nullopt);
    QCOMPARE(wallpaperModeFromToken(QStringLiteral("zoom")), std::nullopt);
    QCOMPARE(fontHintingFromToken(QStringLiteral("extreme")), std::nullopt);
    QCOMPARE(subpixelOrderFromToken(QStringLiteral("crt")), std::nullopt);
}

void AppearanceValuesTests::decodeAcceptsCanonicalSchemaShapes()
{
    auto values = validMap();
    values[QLatin1String(AppearanceKeys::FontPointSize)] = 12; // integer form
    values[QLatin1String(AppearanceKeys::UiScale)] = 1.5;

    const auto decoded =
        AppearanceValues::fromVariantMap(values);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->fontPointSize, 12.0);
    QCOMPARE(decoded->uiScale, 1.5);
    QCOMPARE(decoded->colorScheme, ColorSchemePreference::System);
    QCOMPARE(decoded->fontHinting, FontHinting::Slight);

    // Round trip through the canonical wire shape is stable.
    const auto reparsed =
        AppearanceValues::fromVariantMap(decoded->toVariantMap());
    QVERIFY(reparsed.has_value());
    QVERIFY(*reparsed == *decoded);
}

void AppearanceValuesTests::decodeRejectsWrongTypesAndUnknownTokens()
{
    QString error;

    auto values = validMap();
    values[QLatin1String(AppearanceKeys::Theme)] = 7;
    QVERIFY(!AppearanceValues::fromVariantMap(values, &error).has_value());
    QVERIFY(error.contains(QLatin1String(AppearanceKeys::Theme)));

    values = validMap();
    values[QLatin1String(AppearanceKeys::ColorScheme)] =
        QStringLiteral("sepia");
    QVERIFY(!AppearanceValues::fromVariantMap(values, &error).has_value());
    QVERIFY(error.contains(QLatin1String(AppearanceKeys::ColorScheme)));

    values = validMap();
    values[QLatin1String(AppearanceKeys::FontAntialiasing)] =
        QStringLiteral("yes");
    QVERIFY(!AppearanceValues::fromVariantMap(values, &error).has_value());
    QVERIFY(error.contains(QLatin1String(AppearanceKeys::FontAntialiasing)));

    values = validMap();
    values[QLatin1String(AppearanceKeys::FontPointSize)] = 72.0;
    QVERIFY(!AppearanceValues::fromVariantMap(values, &error).has_value());
    QVERIFY(error.contains(QLatin1String(AppearanceKeys::FontPointSize)));

    values = validMap();
    values[QLatin1String(AppearanceKeys::UiScale)] = 0.25;
    QVERIFY(!AppearanceValues::fromVariantMap(values, &error).has_value());
    QVERIFY(error.contains(QLatin1String(AppearanceKeys::UiScale)));

    values = validMap();
    values.remove(QLatin1String(AppearanceKeys::WallpaperMode));
    QVERIFY(!AppearanceValues::fromVariantMap(values, &error).has_value());
    QVERIFY(error.contains(QLatin1String(AppearanceKeys::WallpaperMode)));
}

void AppearanceValuesTests::validationRequiresInstalledThemesAndBounds()
{
    const QSet<QString> installed{QStringLiteral("qinda-dark"),
                                  QStringLiteral("qinda-light")};

    AppearanceValues values;
    values.themeId = QStringLiteral("qinda-light");
    const auto valid = validateAppearanceDraft(values, installed);
    QVERIFY(valid.valid);
    QVERIFY(valid.fieldErrors.isEmpty());

    values.themeId = QStringLiteral("ghost-theme");
    auto result = validateAppearanceDraft(values, installed);
    QVERIFY(!result.valid);
    QVERIFY(result.fieldErrors.value(QLatin1String(AppearanceKeys::Theme))
                .toString()
                .contains(QStringLiteral("not installed")));

    values.themeId.clear();
    result = validateAppearanceDraft(values, installed);
    QVERIFY(!result.valid);
    QVERIFY(result.fieldErrors.contains(QLatin1String(AppearanceKeys::Theme)));

    values.fontFamily.clear();
    result = validateAppearanceDraft(values, installed);
    QVERIFY(result.fieldErrors.contains(
        QLatin1String(AppearanceKeys::FontFamily)));

    values.fontFamily = QStringLiteral("Inter");
    values.fontPointSize = 40.0;
    result = validateAppearanceDraft(values, installed);
    QVERIFY(result.fieldErrors.contains(
        QLatin1String(AppearanceKeys::FontPointSize)));

    values.fontPointSize = 12.0;
    values.uiScale = 4.0;
    result = validateAppearanceDraft(values, installed);
    QVERIFY(result.fieldErrors.contains(QLatin1String(AppearanceKeys::UiScale)));
}

void AppearanceValuesTests::scopedKeysMatchSchemaKeys()
{
    // AGENT-GUARD: The scoped keys are a public contract with
    // data/settings/schema-v2.json and the qindaqt-settings route scope.
    const auto keys = AppearanceKeys::scopedKeys();
    QCOMPARE(keys.size(), 10);
    QCOMPARE(keys.count(QLatin1String(AppearanceKeys::Theme)), 1);
    QVERIFY(keys.contains(QLatin1String(AppearanceKeys::FontSubpixelOrder)));
    QVERIFY(keys.contains(QLatin1String(AppearanceKeys::WallpaperMode)));
    QVERIFY(!keys.contains(QStringLiteral("appearance.accentColor")));
}

QTEST_GUILESS_MAIN(AppearanceValuesTests)
#include "tst_appearance_values.moc"
