// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_preview.h"
#include "qindaqt/apps/settings_appearance/appearance_theme_catalog.h"

#include "qindaqt/themes/theme_loader.h"

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#include <optional>

using namespace QindaQt::Apps::SettingsAppearance;
using QindaQt::Themes::ThemeLoader;
using QindaQt::Themes::ThemeSpec;

namespace {

std::optional<QVector<ThemeSpec>> loadBuiltInThemes(QString *error)
{
    QVector<ThemeSpec> themes;
    for (const auto *file : {"/data/themes/qinda-dark.json",
                             "/data/themes/qinda-light.json",
                             "/data/themes/qinda-dusk.json",
                             "/data/themes/qinda-high-contrast.json",
                             "/data/themes/qinda-macos.json"}) {
        const auto loaded =
            ThemeLoader::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR)
                                  + QLatin1String(file));
        if (!loaded.ok) {
            if (error != nullptr) {
                *error = loaded.error;
            }
            return std::nullopt;
        }
        themes.append(loaded.theme);
    }
    return themes;
}

} // namespace

class AppearancePreviewTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void configuredThemeWinsOverSchemePreference();
    void schemePreferenceResolvesMissingTheme();
    void systemSchemeFollowsPlatformValue();
    void everyBuiltInThemeProducesCompletePreviewMaps();
    void highContrastVariantSetsQstInput();
    void userDirectoryMergesWithoutHidingBuiltIns();

private:
    QVector<ThemeSpec> m_themes;
};

void AppearancePreviewTests::initTestCase()
{
    QString error;
    const auto loaded = loadBuiltInThemes(&error);
    QVERIFY2(loaded.has_value(), qPrintable(error));
    m_themes = *loaded;
    QCOMPARE(m_themes.size(), 5);
    QSet<QString> ids;
    for (const ThemeSpec &theme : m_themes) {
        ids.insert(theme.id);
    }
    QCOMPARE(ids, QSet<QString>({QStringLiteral("qinda-dark"),
                                QStringLiteral("qinda-light"),
                                QStringLiteral("qinda-dusk"),
                                QStringLiteral("qinda-high-contrast"),
                                QStringLiteral("qinda-macos")}));
}

void AppearancePreviewTests::configuredThemeWinsOverSchemePreference()
{
    AppearancePreview preview(m_themes);
    AppearanceValues values;
    values.themeId = QStringLiteral("qinda-dusk");
    values.colorScheme = ColorSchemePreference::Light;

    const auto resolution = preview.resolve(values, Qt::ColorScheme::Light);
    QVERIFY(resolution.configuredInstalled);
    QVERIFY(resolution.fallbackThemeId.isEmpty());
    QCOMPARE(preview.themes().at(resolution.themeIndex).id,
             QStringLiteral("qinda-dusk"));
}

void AppearancePreviewTests::schemePreferenceResolvesMissingTheme()
{
    AppearancePreview preview(m_themes);
    AppearanceValues values;
    values.themeId = QStringLiteral("ghost-theme");
    values.colorScheme = ColorSchemePreference::Dark;

    const auto resolution = preview.resolve(values, Qt::ColorScheme::Light);
    QVERIFY(!resolution.configuredInstalled);
    QCOMPARE(resolution.fallbackThemeId, QStringLiteral("qinda-dark"));
    QCOMPARE(preview.themes().at(resolution.themeIndex).id,
             QStringLiteral("qinda-dark"));

    values.colorScheme = ColorSchemePreference::Light;
    const auto lightResolution =
        preview.resolve(values, Qt::ColorScheme::Dark);
    QCOMPARE(lightResolution.fallbackThemeId, QStringLiteral("qinda-light"));
}

void AppearancePreviewTests::systemSchemeFollowsPlatformValue()
{
    AppearancePreview preview(m_themes);
    AppearanceValues values;
    values.themeId = QStringLiteral("ghost-theme");
    values.colorScheme = ColorSchemePreference::System;

    QCOMPARE(preview.resolve(values, Qt::ColorScheme::Dark).fallbackThemeId,
             QStringLiteral("qinda-dark"));
    QCOMPARE(preview.resolve(values, Qt::ColorScheme::Light).fallbackThemeId,
             QStringLiteral("qinda-light"));
}

void AppearancePreviewTests::everyBuiltInThemeProducesCompletePreviewMaps()
{
    AppearancePreview preview(m_themes);
    const auto &maps = preview.previewMaps();
    QCOMPARE(static_cast<int>(maps.size()), m_themes.size());

    // AGENT-GUARD: ThemeCard treats a map missing bg/accent/fg/outline roles
    // as an unavailable preview, so every installed theme must publish the
    // complete published-generation shape here.
    for (const auto &map : maps) {
        QVERIFY(!map.isEmpty());
        QVERIFY(map.contains(QStringLiteral("bg")));
        QVERIFY(map.contains(QStringLiteral("fg")));
        QVERIFY(map.contains(QStringLiteral("accent")));
        QVERIFY(map.contains(QStringLiteral("outline")));
        const auto background = map.value(QStringLiteral("bg")).toMap();
        QVERIFY(background.contains(QStringLiteral("base")));
        QVERIFY(background.contains(QStringLiteral("raised")));
        QVERIFY(map.value(QStringLiteral("accent")).toMap().contains(
            QStringLiteral("default")));
        QVERIFY(map.value(QStringLiteral("fg")).toMap().contains(
            QStringLiteral("default")));
        QVERIFY(map.value(QStringLiteral("outline")).toMap().contains(
            QStringLiteral("strong")));
    }
}

void AppearancePreviewTests::highContrastVariantSetsQstInput()
{
    AppearancePreview preview(m_themes);
    AppearanceValues values;
    values.fontPointSize = 14.0;

    int highContrastIndex = -1;
    for (int index = 0; index < preview.themes().size(); ++index) {
        if (preview.themes().at(index).variant == QStringLiteral("high-contrast")) {
            highContrastIndex = index;
        }
    }
    QVERIFY(highContrastIndex >= 0);
    const auto inputs = preview.accessibilityInputs(
        values, preview.themes().at(highContrastIndex));
    QVERIFY(inputs.highContrast);
    QCOMPARE(inputs.basePointSize, 14.0);
    QCOMPARE(inputs.textScale, 1.0);

    const auto darkTheme = preview.themes().at(0);
    QVERIFY(!preview.accessibilityInputs(values, darkTheme).highContrast);
}

void AppearancePreviewTests::userDirectoryMergesWithoutHidingBuiltIns()
{
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const QString userTheme = userDirectory.filePath(
        QStringLiteral("qinda-macos.json"));
    QVERIFY(QFile::copy(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-macos.json"),
        userTheme));

    QString error;
    const auto merged = loadAppearanceThemeDirectories(
        {userDirectory.path(),
         QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes")},
        &error);
    QVERIFY2(merged.has_value(), qPrintable(error));
    QCOMPARE(merged->size(), 5);
    QSet<QString> ids;
    for (const ThemeSpec &theme : *merged) {
        ids.insert(theme.id);
    }
    QVERIFY(ids.contains(QStringLiteral("qinda-dark")));
    QVERIFY(ids.contains(QStringLiteral("qinda-light")));
    QVERIFY(ids.contains(QStringLiteral("qinda-dusk")));
    QVERIFY(ids.contains(QStringLiteral("qinda-high-contrast")));
    QVERIFY(ids.contains(QStringLiteral("qinda-macos")));
}

QTEST_GUILESS_MAIN(AppearancePreviewTests)
#include "tst_appearance_preview.moc"
