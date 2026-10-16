// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/themes/theme_catalog.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Themes;

namespace {

// A schema-valid theme: the shipped Smoked Plum document with its identity
// replaced. Reading the real document keeps these rows honest about what the
// loader accepts.
QByteArray themeDocument(const QString &id, const QString &name)
{
    QFile source(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!source.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonObject object = QJsonDocument::fromJson(source.readAll()).object();
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    return QJsonDocument(object).toJson();
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

QStringList idsOf(const ThemeCatalog &catalog)
{
    QStringList ids;
    for (const ThemeSpec &theme : catalog.themes()) {
        ids.append(theme.id);
    }
    return ids;
}

QString nameOf(const ThemeCatalog &catalog, const QString &id)
{
    for (const ThemeSpec &theme : catalog.themes()) {
        if (theme.id == id) {
            return theme.name;
        }
    }
    return {};
}

} // namespace

// AGENT-CONTRACT (ADR-0393): the shell merges every XDG theme directory. These
// rows pin the layering: the first directory with themes is a strict base, a
// later directory overrides by id in place and cannot take the catalog down.
class ThemeCatalogMergeTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void laterDirectoriesOverrideByIdInPlaceAndAppendNewIds();
    void aStaleUserSubsetNeverHidesTheInstalledSet();
    void theSavedThemeResolvesThroughAStaleUserDirectory();
    void invalidOverrideEntriesAreSkippedAndReported();
    void anInvalidBaseEntryFailsTheLoad();
    void duplicateIdsInOneDirectoryFailTheBaseAndAreSkippedInAnOverride();
    void emptyAndMissingDirectoriesAreIgnoredButSomeThemeIsRequired();
    void aFailedLoadKeepsThePreviousCatalog();

private:
    [[nodiscard]] QString dir(const QString &name) const
    {
        return m_root.filePath(name);
    }
    [[nodiscard]] bool theme(const QString &directory, const QString &id,
                             const QString &name) const
    {
        return writeFile(dir(directory) + QLatin1Char('/') + id + QStringLiteral(".json"),
                         themeDocument(id, name));
    }

    QTemporaryDir m_root;
};

void ThemeCatalogMergeTest::init()
{
    QVERIFY(m_root.isValid());
    QDir(m_root.path()).removeRecursively();
    QVERIFY(QDir().mkpath(m_root.path()));
}

void ThemeCatalogMergeTest::laterDirectoriesOverrideByIdInPlaceAndAppendNewIds()
{
    QVERIFY(theme(QStringLiteral("system"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    QVERIFY(theme(QStringLiteral("system"), QStringLiteral("beta"), QStringLiteral("Beta")));
    QVERIFY(theme(QStringLiteral("system"), QStringLiteral("gamma"), QStringLiteral("Gamma")));
    QVERIFY(theme(QStringLiteral("user"), QStringLiteral("beta"), QStringLiteral("My Beta")));
    QVERIFY(theme(QStringLiteral("user"), QStringLiteral("delta"), QStringLiteral("Delta")));

    ThemeCatalog catalog;
    QString error;
    QStringList skipped;
    QVERIFY2(catalog.loadDirectories({dir(QStringLiteral("system")), dir(QStringLiteral("user"))},
                                     &error, &skipped),
             qPrintable(error));
    // Installed order is kept, an override replaces in place, a new id lands
    // at the end, and the first theme is selected like loadDirectory.
    QCOMPARE(idsOf(catalog), QStringList({QStringLiteral("alpha"), QStringLiteral("beta"),
                                          QStringLiteral("gamma"), QStringLiteral("delta")}));
    QCOMPARE(nameOf(catalog, QStringLiteral("beta")), QStringLiteral("My Beta"));
    QCOMPARE(nameOf(catalog, QStringLiteral("alpha")), QStringLiteral("Alpha"));
    QVERIFY(skipped.isEmpty());
    QCOMPARE(catalog.currentIndex(), 0);
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(), QStringLiteral("alpha"));
}

void ThemeCatalogMergeTest::aStaleUserSubsetNeverHidesTheInstalledSet()
{
    // 28 installed themes, and a user directory that was populated in
    // September with 19 of them: the catalog must still offer all 28.
    QStringList installed;
    for (int index = 0; index < 28; ++index) {
        const QString id = QStringLiteral("installed-%1").arg(index, 2, 10, QLatin1Char('0'));
        installed.append(id);
        QVERIFY(theme(QStringLiteral("usr-share"), id, QStringLiteral("Installed %1").arg(index)));
    }
    for (int index = 0; index < 19; ++index) {
        QVERIFY(theme(QStringLiteral("home-share"), installed.at(index),
                      QStringLiteral("Stale %1").arg(index)));
    }
    ThemeCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectories(
                 {dir(QStringLiteral("usr-share")), dir(QStringLiteral("home-share"))}, &error),
             qPrintable(error));
    QCOMPARE(catalog.themes().size(), 28);
    QCOMPARE(idsOf(catalog), installed);
    // The user's copies win where they exist, and only there.
    QCOMPARE(nameOf(catalog, installed.at(3)), QStringLiteral("Stale 3"));
    QCOMPARE(nameOf(catalog, installed.at(25)), QStringLiteral("Installed 25"));
}

void ThemeCatalogMergeTest::theSavedThemeResolvesThroughAStaleUserDirectory()
{
    // The 2026-10-16 incident: qinda-corner-violet-dark lives only in the
    // installed set, the user's stale directory lacks it, and the saved
    // preference was rejected at every login.
    QVERIFY(theme(QStringLiteral("usr-share"), QStringLiteral("qinda-dark"),
                  QStringLiteral("Smoked Plum")));
    QVERIFY(theme(QStringLiteral("usr-share"), QStringLiteral("qinda-corner-violet-dark"),
                  QStringLiteral("Corner Violet Dark")));
    QVERIFY(theme(QStringLiteral("home-share"), QStringLiteral("qinda-dark"),
                  QStringLiteral("Smoked Plum (September)")));

    ThemeCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectories(
                 {dir(QStringLiteral("usr-share")), dir(QStringLiteral("home-share"))}, &error),
             qPrintable(error));
    QVERIFY(catalog.selectById(QStringLiteral("qinda-corner-violet-dark")));
    QCOMPARE(catalog.current().value(QStringLiteral("name")).toString(),
             QStringLiteral("Corner Violet Dark"));

    // The old behavior, for contrast: loading only the first directory found
    // (the user's) cannot select the saved theme.
    ThemeCatalog firstOnly;
    QVERIFY2(firstOnly.loadDirectory(dir(QStringLiteral("home-share")), &error),
             qPrintable(error));
    QVERIFY(!firstOnly.selectById(QStringLiteral("qinda-corner-violet-dark")));
}

void ThemeCatalogMergeTest::invalidOverrideEntriesAreSkippedAndReported()
{
    QVERIFY(theme(QStringLiteral("system"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    QVERIFY(theme(QStringLiteral("user"), QStringLiteral("good"), QStringLiteral("Good")));
    QVERIFY(writeFile(dir(QStringLiteral("user")) + QStringLiteral("/broken.json"),
                      QByteArray("{ this is not json")));
    QVERIFY(writeFile(dir(QStringLiteral("user")) + QStringLiteral("/incomplete.json"),
                      QByteArray(R"({"schemaVersion": 1, "id": "incomplete"})")));

    ThemeCatalog catalog;
    QString error;
    QStringList skipped;
    QVERIFY2(catalog.loadDirectories({dir(QStringLiteral("system")), dir(QStringLiteral("user"))},
                                     &error, &skipped),
             qPrintable(error));
    QCOMPARE(idsOf(catalog), QStringList({QStringLiteral("alpha"), QStringLiteral("good")}));
    QCOMPARE(skipped.size(), 2);
    QVERIFY(skipped.join(QLatin1Char('\n')).contains(QStringLiteral("broken.json")));
    QVERIFY(skipped.join(QLatin1Char('\n')).contains(QStringLiteral("incomplete.json")));
}

void ThemeCatalogMergeTest::anInvalidBaseEntryFailsTheLoad()
{
    QVERIFY(theme(QStringLiteral("system"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    QVERIFY(writeFile(dir(QStringLiteral("system")) + QStringLiteral("/broken.json"),
                      QByteArray("{ nope")));
    QVERIFY(theme(QStringLiteral("user"), QStringLiteral("good"), QStringLiteral("Good")));

    ThemeCatalog catalog;
    QString error;
    QStringList skipped;
    QVERIFY(!catalog.loadDirectories({dir(QStringLiteral("system")), dir(QStringLiteral("user"))},
                                     &error, &skipped));
    QVERIFY(error.contains(QStringLiteral("broken.json")));
    QVERIFY(catalog.themes().isEmpty());
}

void ThemeCatalogMergeTest::duplicateIdsInOneDirectoryFailTheBaseAndAreSkippedInAnOverride()
{
    // Two files in one directory claiming the same id.
    QVERIFY(writeFile(dir(QStringLiteral("system")) + QStringLiteral("/a.json"),
                      themeDocument(QStringLiteral("same"), QStringLiteral("First"))));
    QVERIFY(writeFile(dir(QStringLiteral("system")) + QStringLiteral("/b.json"),
                      themeDocument(QStringLiteral("same"), QStringLiteral("Second"))));
    ThemeCatalog strict;
    QString error;
    QVERIFY(!strict.loadDirectories({dir(QStringLiteral("system"))}, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate theme id: same")));

    QVERIFY(theme(QStringLiteral("base"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    ThemeCatalog lenient;
    QStringList skipped;
    QVERIFY2(lenient.loadDirectories({dir(QStringLiteral("base")), dir(QStringLiteral("system"))},
                                     &error, &skipped),
             qPrintable(error));
    QCOMPARE(idsOf(lenient), QStringList({QStringLiteral("alpha"), QStringLiteral("same")}));
    QCOMPARE(nameOf(lenient, QStringLiteral("same")), QStringLiteral("First"));
    QCOMPARE(skipped.size(), 1);
}

void ThemeCatalogMergeTest::emptyAndMissingDirectoriesAreIgnoredButSomeThemeIsRequired()
{
    QVERIFY(QDir().mkpath(dir(QStringLiteral("empty"))));
    ThemeCatalog none;
    QString error;
    QVERIFY(!none.loadDirectories({dir(QStringLiteral("empty")), dir(QStringLiteral("absent"))},
                                  &error));
    QVERIFY(error.contains(QStringLiteral("no theme JSON files found")));
    QVERIFY(!none.loadDirectories({}, &error));

    QVERIFY(theme(QStringLiteral("system"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    ThemeCatalog some;
    QVERIFY2(some.loadDirectories({dir(QStringLiteral("absent")), dir(QStringLiteral("system")),
                                   dir(QStringLiteral("empty"))},
                                  &error),
             qPrintable(error));
    QCOMPARE(idsOf(some), QStringList{QStringLiteral("alpha")});
}

void ThemeCatalogMergeTest::aFailedLoadKeepsThePreviousCatalog()
{
    QVERIFY(theme(QStringLiteral("system"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    ThemeCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectories({dir(QStringLiteral("system"))}, &error), qPrintable(error));
    QSignalSpy items(&catalog, &ThemeCatalog::itemsChanged);

    QVERIFY(QDir().mkpath(dir(QStringLiteral("bad"))));
    QVERIFY(writeFile(dir(QStringLiteral("bad")) + QStringLiteral("/broken.json"),
                      QByteArray("{ nope")));
    QVERIFY(!catalog.loadDirectories({dir(QStringLiteral("bad"))}, &error));
    QCOMPARE(items.size(), 0);
    QCOMPARE(idsOf(catalog), QStringList{QStringLiteral("alpha")});
}

QTEST_GUILESS_MAIN(ThemeCatalogMergeTest)
#include "tst_theme_catalog_merge.moc"
