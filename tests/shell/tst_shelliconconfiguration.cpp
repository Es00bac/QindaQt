// SPDX-License-Identifier: GPL-3.0-or-later
#include "shelliconconfiguration.h"

#include "qindaqt/themes/theme_catalog.h"
#include "qindaqt/themes/theme_loader.h"

#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QtTest>

using QindaQt::Shell::ShellIconConfiguration;

class ShellIconConfigurationTests final : public QObject {
    Q_OBJECT

private slots:
    void configuredThemeIsSelected();
    void missingHintUsesThemeDarkness();
    void hostileHintFailsCatalogLoad();
    void largeCatalogThemeIsNotReparsed();
    void leadingPunctuationIsSchemaValid();
    void dataRootsAreExplicitAndOrdered();
    void invalidDataHomeUsesSpecificationDefault();
};

void ShellIconConfigurationTests::configuredThemeIsSelected()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    const QString directory = QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes");
    QVERIFY2(themes.loadDirectory(directory, &error), qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("qinda-dark")));
    QString name;
    QVERIFY2(ShellIconConfiguration::selectedThemeName(themes, &name, &error),
             qPrintable(error));
    QCOMPARE(name, QStringLiteral("breeze-dark"));
}

void ShellIconConfigurationTests::missingHintUsesThemeDarkness()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    const QString directory = QStringLiteral(
        QINDAQT_SOURCE_DIR "/tests/shell/testdata/icon-themes-valid");
    QVERIFY2(themes.loadDirectory(directory, &error), qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("default-light")));
    QString name;
    QVERIFY2(ShellIconConfiguration::selectedThemeName(themes, &name, &error),
             qPrintable(error));
    QCOMPARE(name, QStringLiteral("breeze"));
}

void ShellIconConfigurationTests::hostileHintFailsCatalogLoad()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    const QString directory = QStringLiteral(
        QINDAQT_SOURCE_DIR "/tests/shell/testdata/icon-themes-invalid");
    QVERIFY(!themes.loadDirectory(directory, &error));
    QVERIFY(error.contains(QStringLiteral("invalid iconTheme")));
}

void ShellIconConfigurationTests::largeCatalogThemeIsNotReparsed()
{
    const QString directory = QStringLiteral(QINDAQT_TEST_SCRATCH "/large");
    QVERIFY(QDir(directory).removeRecursively() || !QDir(directory).exists());
    QVERIFY(QDir().mkpath(directory));
    QFile file(QDir(directory).filePath(QStringLiteral("large.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QByteArray json = R"json({
      "schemaVersion": 1, "id": "large", "name": "Large", "variant": "dark",
      "iconTheme": "breeze-dark", "colors": {
        "canvas": "#101010", "surface": "#202020", "surfaceRaised": "#303030",
        "border": "#404040", "text": "#ffffff", "textMuted": "#aaaaaa",
        "accent": "#80c0b0", "accentText": "#102020", "danger": "#ff6060"
      }
    })json";
    json.append(300 * 1024, ' ');
    QCOMPARE(file.write(json), json.size());
    file.close();

    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    QVERIFY2(themes.loadDirectory(directory, &error), qPrintable(error));
    QString name;
    QVERIFY2(ShellIconConfiguration::selectedThemeName(themes, &name, &error),
             qPrintable(error));
    QCOMPARE(name, QStringLiteral("breeze-dark"));
}

void ShellIconConfigurationTests::leadingPunctuationIsSchemaValid()
{
    constexpr auto json = R"json({
      "schemaVersion": 1, "id": "punctuation", "name": "Punctuation", "variant": "dark",
      "iconTheme": "_private", "colors": {
        "canvas": "#101010", "surface": "#202020", "surfaceRaised": "#303030",
        "border": "#404040", "text": "#ffffff", "textMuted": "#aaaaaa",
        "accent": "#80c0b0", "accentText": "#102020", "danger": "#ff6060"
      }
    })json";
    const auto loaded = QindaQt::Themes::ThemeLoader::fromJson(
        json, QStringLiteral("punctuation fixture"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QCOMPARE(loaded.theme.iconTheme, QStringLiteral("_private"));
}

void ShellIconConfigurationTests::invalidDataHomeUsesSpecificationDefault()
{
    for (const QString &configured : {QString(), QStringLiteral("relative")}) {
        QProcessEnvironment environment;
        environment.insert(QStringLiteral("XDG_DATA_HOME"), configured);
        const auto roots = ShellIconConfiguration::dataRoots(
            environment, QStringLiteral("/home/fixture"));
        QCOMPARE(roots.dataHome, QStringLiteral("/home/fixture/.local/share"));
    }
}

void ShellIconConfigurationTests::dataRootsAreExplicitAndOrdered()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("XDG_DATA_HOME"),
                       QStringLiteral("/example/home"));
    environment.insert(QStringLiteral("XDG_DATA_DIRS"),
                       QStringLiteral("/first:/second:/first:relative"));
    const auto roots = ShellIconConfiguration::dataRoots(
        environment, QStringLiteral("/unused"));
    QCOMPARE(roots.dataHome, QStringLiteral("/example/home"));
    QCOMPARE(roots.dataDirectories,
             QStringList({QStringLiteral("/first"), QStringLiteral("/second")}));
}

QTEST_GUILESS_MAIN(ShellIconConfigurationTests)

#include "tst_shelliconconfiguration.moc"
