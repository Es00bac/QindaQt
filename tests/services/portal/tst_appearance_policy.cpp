// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/portal/appearance_theme_catalog.h"

#include "qindaqt/themes/theme_spec.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Services::Portal;

namespace {

QVector<QindaQt::Themes::ThemeSpec> builtIns()
{
    QString error;
    const auto loaded = loadPortalAppearanceThemes(
        {QStringLiteral(QINDAQT_PORTAL_SOURCE_DIR "/data/themes")}, &error);
    if (!loaded.has_value()) {
        qFatal("%s", qPrintable(error));
    }
    return *loaded;
}

QVariantMap validSettings(QString theme = QStringLiteral("qinda-dark"),
                          QString scheme = QStringLiteral("system"),
                          bool highContrast = false,
                          bool reducedTransparency = false)
{
    return {{QString::fromLatin1(kThemeSetting), std::move(theme)},
            {QString::fromLatin1(kColorSchemeSetting), std::move(scheme)},
            {QString::fromLatin1(kHighContrastSetting), highContrast},
            {QString::fromLatin1(kReducedTransparencySetting),
             reducedTransparency}};
}

} // namespace

class AppearancePolicyTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void projectsStandardValuesFromSettingsAndQst();
    void rejectsMalformedSnapshotsAtomically();
    void validatesCatalogAndOpaqueAccentBoundary();
    void boundsThemeDiscoveryAndPreservesPrecedence();
};

void AppearancePolicyTests::projectsStandardValuesFromSettingsAndQst()
{
    AppearancePolicyProjector projector(builtIns());
    QVERIFY2(projector.isValid(), qPrintable(projector.catalogError()));

    const auto dark = projector.project(validSettings());
    QVERIFY2(dark.ok(), qPrintable(dark.diagnostic));
    QCOMPARE(dark.policy->colorScheme, PortalColorScheme::NoPreference);
    QCOMPARE(dark.policy->contrast, PortalContrast::NoPreference);
    // QindaPunk Nightfall accent #D98A32 from data/themes/qinda-dark.json;
    // the QST projection keeps an opaque theme accent verbatim.
    QVERIFY(qAbs(dark.policy->accentColor.red - (217.0 / 255.0)) < 0.00001);
    QVERIFY(qAbs(dark.policy->accentColor.green - (138.0 / 255.0)) < 0.00001);
    QVERIFY(qAbs(dark.policy->accentColor.blue - (50.0 / 255.0)) < 0.00001);

    const auto light = projector.project(validSettings(
        QStringLiteral("qinda-light"), QStringLiteral("light"), true));
    QVERIFY2(light.ok(), qPrintable(light.diagnostic));
    QCOMPARE(light.policy->colorScheme, PortalColorScheme::PreferLight);
    QCOMPARE(light.policy->contrast, PortalContrast::PreferHigh);

    const auto highContrast = projector.project(validSettings(
        QStringLiteral("qinda-high-contrast"), QStringLiteral("dark"), false));
    QVERIFY2(highContrast.ok(), qPrintable(highContrast.diagnostic));
    QCOMPARE(highContrast.policy->colorScheme, PortalColorScheme::PreferDark);
    QCOMPARE(highContrast.policy->contrast, PortalContrast::PreferHigh);
    QVERIFY(qAbs(highContrast.policy->accentColor.red - 1.0) < 0.00001);
}

void AppearancePolicyTests::rejectsMalformedSnapshotsAtomically()
{
    AppearancePolicyProjector projector(builtIns());
    auto missing = validSettings();
    missing.remove(QString::fromLatin1(kHighContrastSetting));
    QCOMPARE(projector.project(missing).error,
             AppearanceProjectionError::MalformedSnapshot);

    auto extra = validSettings();
    extra.insert(QStringLiteral("appearance.unrelated"), true);
    QCOMPARE(projector.project(extra).error,
             AppearanceProjectionError::MalformedSnapshot);

    auto wrongBoolean = validSettings();
    wrongBoolean.insert(QString::fromLatin1(kHighContrastSetting), 1);
    QCOMPARE(projector.project(wrongBoolean).error,
             AppearanceProjectionError::MalformedSnapshot);

    auto wrongScheme = validSettings();
    wrongScheme.insert(QString::fromLatin1(kColorSchemeSetting),
                       QStringLiteral("sepia"));
    QCOMPARE(projector.project(wrongScheme).error,
             AppearanceProjectionError::MalformedSnapshot);

    QCOMPARE(projector.project(validSettings(QStringLiteral("missing"))).error,
             AppearanceProjectionError::UnknownTheme);
}

void AppearancePolicyTests::validatesCatalogAndOpaqueAccentBoundary()
{
    AppearancePolicyProjector empty({});
    QVERIFY(!empty.isValid());
    QCOMPARE(empty.project(validSettings()).error,
             AppearanceProjectionError::InvalidCatalog);

    auto duplicateThemes = builtIns();
    duplicateThemes.append(duplicateThemes.first());
    AppearancePolicyProjector duplicate(std::move(duplicateThemes));
    QVERIFY(!duplicate.isValid());

    auto translucentThemes = builtIns();
    auto &theme = translucentThemes.first();
    const QString id = theme.id;
    QColor accent = theme.colors.value(QStringLiteral("accent"));
    accent.setAlpha(128);
    theme.colors.insert(QStringLiteral("accent"), accent);
    AppearancePolicyProjector translucent(std::move(translucentThemes));
    QVERIFY(translucent.isValid());
    QCOMPARE(translucent.project(validSettings(id)).error,
             AppearanceProjectionError::NonOpaqueAccent);
    const auto flattened =
        translucent.project(validSettings(id, QStringLiteral("system"), false,
                                           true));
    QVERIFY2(flattened.ok(), qPrintable(flattened.diagnostic));
}

void AppearancePolicyTests::boundsThemeDiscoveryAndPreservesPrecedence()
{
    QString error;
    QStringList tooMany;
    for (int index = 0; index < 17; ++index) {
        tooMany.append(QStringLiteral("/tmp/qindaqt-portal-%1").arg(index));
    }
    QVERIFY(!loadPortalAppearanceThemes(tooMany, &error).has_value());

    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    const QByteArray base = R"JSON({
      "schemaVersion":1,"id":"same","name":"First","variant":"dark",
      "colors":{"canvas":"#000000","surface":"#111111",
      "surfaceRaised":"#222222","border":"#ffffff","text":"#ffffff",
      "textMuted":"#dddddd","accent":"#ff0000","accentText":"#000000",
      "danger":"#ff00ff"}})JSON";
    QFile firstFile(first.filePath(QStringLiteral("theme.json")));
    QVERIFY(firstFile.open(QIODevice::WriteOnly));
    QCOMPARE(firstFile.write(base), base.size());
    firstFile.close();

    QByteArray secondDocument = base;
    secondDocument.replace("\"First\"", "\"Second\"");
    QFile secondFile(second.filePath(QStringLiteral("theme.json")));
    QVERIFY(secondFile.open(QIODevice::WriteOnly));
    QCOMPARE(secondFile.write(secondDocument), secondDocument.size());
    secondFile.close();

    const auto catalog = loadPortalAppearanceThemes(
        {first.path(), second.path()}, &error);
    QVERIFY2(catalog.has_value(), qPrintable(error));
    QCOMPARE(catalog->size(), 1);
    QCOMPARE(catalog->first().name, QStringLiteral("First"));

    QFile malformed(second.filePath(QStringLiteral("bad.json")));
    QVERIFY(malformed.open(QIODevice::WriteOnly));
    QVERIFY(malformed.write("{") == 1);
    malformed.close();
    QVERIFY(!loadPortalAppearanceThemes({first.path(), second.path()}, &error)
                 .has_value());
}

QTEST_GUILESS_MAIN(AppearancePolicyTests)
#include "tst_appearance_policy.moc"
