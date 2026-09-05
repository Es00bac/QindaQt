// SPDX-License-Identifier: GPL-3.0-or-later
#include "shelliconconfiguration.h"

#include "qindaqt/themes/theme_catalog.h"

#include <QProcessEnvironment>
#include <QtTest>

using QindaQt::Shell::ShellIconConfiguration;

class ShellIconConfigurationTests final : public QObject {
    Q_OBJECT

private slots:
    void configuredThemeIsSelected();
    void missingHintUsesThemeDarkness();
    void hostileHintFailsClosed();
    void dataRootsAreExplicitAndOrdered();
};

void ShellIconConfigurationTests::configuredThemeIsSelected()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    const QString directory = QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes");
    QVERIFY2(themes.loadDirectory(directory, &error), qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("qinda-dark")));
    QString name;
    QVERIFY2(ShellIconConfiguration::selectedThemeName(
                 themes, directory, &name, &error), qPrintable(error));
    QCOMPARE(name, QStringLiteral("breeze-dark"));
}

void ShellIconConfigurationTests::missingHintUsesThemeDarkness()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    const QString directory = QStringLiteral(
        QINDAQT_SOURCE_DIR "/tests/shell/testdata/icon-themes");
    QVERIFY2(themes.loadDirectory(directory, &error), qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("default-light")));
    QString name;
    QVERIFY2(ShellIconConfiguration::selectedThemeName(
                 themes, directory, &name, &error), qPrintable(error));
    QCOMPARE(name, QStringLiteral("breeze"));
}

void ShellIconConfigurationTests::hostileHintFailsClosed()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    const QString directory = QStringLiteral(
        QINDAQT_SOURCE_DIR "/tests/shell/testdata/icon-themes");
    QVERIFY2(themes.loadDirectory(directory, &error), qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("bad-icon-theme")));
    QString name;
    QVERIFY(!ShellIconConfiguration::selectedThemeName(
        themes, directory, &name, &error));
    QVERIFY(name.isEmpty());
    QVERIFY(error.contains(QStringLiteral("invalid iconTheme")));
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
