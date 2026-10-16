// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applets/manifest_catalog.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Applets;

namespace {

// A schema-valid manifest: the shipped clock with its identity replaced.
QByteArray manifestDocument(const QString &id, const QString &name)
{
    QFile source(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets/clock.json"));
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

QStringList idsOf(const ManifestCatalog &catalog)
{
    QStringList ids;
    for (const AppletManifest &manifest : catalog.manifests()) {
        ids.append(manifest.id);
    }
    return ids;
}

} // namespace

// AGENT-CONTRACT (ADR-0393): applet manifests merge every XDG data directory
// with the same layering as themes: a strict base, then override layers that
// replace by id in place and can never take the catalog down.
class ManifestCatalogMergeTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void laterDirectoriesOverrideByIdAndAppendNewIds();
    void aStaleUserSubsetNeverHidesTheInstalledSet();
    void invalidOverrideEntriesAreSkippedAndReported();
    void anInvalidBaseEntryFailsTheLoadAndKeepsThePreviousCatalog();
    void missingAndEmptyDirectoriesAreIgnoredButSomeManifestIsRequired();
    void theShippedCatalogMergesOverItselfWithoutChange();

private:
    [[nodiscard]] QString dir(const QString &name) const { return m_root.filePath(name); }
    [[nodiscard]] bool manifest(const QString &directory, const QString &id,
                                const QString &name) const
    {
        return writeFile(dir(directory) + QLatin1Char('/') + id + QStringLiteral(".json"),
                         manifestDocument(id, name));
    }

    QTemporaryDir m_root;
};

void ManifestCatalogMergeTest::init()
{
    QVERIFY(m_root.isValid());
    QDir(m_root.path()).removeRecursively();
    QVERIFY(QDir().mkpath(m_root.path()));
}

void ManifestCatalogMergeTest::laterDirectoriesOverrideByIdAndAppendNewIds()
{
    QVERIFY(manifest(QStringLiteral("system"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    QVERIFY(manifest(QStringLiteral("system"), QStringLiteral("beta"), QStringLiteral("Beta")));
    QVERIFY(manifest(QStringLiteral("user"), QStringLiteral("beta"), QStringLiteral("My Beta")));
    QVERIFY(manifest(QStringLiteral("user"), QStringLiteral("gamma"), QStringLiteral("Gamma")));

    ManifestCatalog catalog;
    QString error;
    QStringList skipped;
    QVERIFY2(catalog.loadDirectories({dir(QStringLiteral("system")), dir(QStringLiteral("user"))},
                                     &error, &skipped),
             qPrintable(error));
    QCOMPARE(idsOf(catalog), QStringList({QStringLiteral("alpha"), QStringLiteral("beta"),
                                          QStringLiteral("gamma")}));
    QCOMPARE(catalog.findById(u"beta")->name, QStringLiteral("My Beta"));
    QCOMPARE(catalog.findById(u"alpha")->name, QStringLiteral("Alpha"));
    QVERIFY(skipped.isEmpty());
}

void ManifestCatalogMergeTest::aStaleUserSubsetNeverHidesTheInstalledSet()
{
    QStringList installed;
    for (int index = 0; index < 11; ++index) {
        const QString id = QStringLiteral("installed-%1").arg(index, 2, 10, QLatin1Char('0'));
        installed.append(id);
        QVERIFY(manifest(QStringLiteral("usr-share"), id, QStringLiteral("Installed")));
    }
    QVERIFY(manifest(QStringLiteral("home-share"), installed.at(2), QStringLiteral("Stale")));

    ManifestCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectories(
                 {dir(QStringLiteral("usr-share")), dir(QStringLiteral("home-share"))}, &error),
             qPrintable(error));
    QCOMPARE(idsOf(catalog), installed);
    QCOMPARE(catalog.findById(installed.at(2))->name, QStringLiteral("Stale"));
}

void ManifestCatalogMergeTest::invalidOverrideEntriesAreSkippedAndReported()
{
    QVERIFY(manifest(QStringLiteral("system"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    QVERIFY(writeFile(dir(QStringLiteral("user")) + QStringLiteral("/broken.json"),
                      QByteArray("{ this is not json")));
    QVERIFY(writeFile(dir(QStringLiteral("user")) + QStringLiteral("/incomplete.json"),
                      QByteArray(R"({"schemaVersion": 1, "id": "incomplete"})")));
    QVERIFY(writeFile(dir(QStringLiteral("user")) + QStringLiteral("/a-twin.json"),
                      manifestDocument(QStringLiteral("twin"), QStringLiteral("First"))));
    QVERIFY(writeFile(dir(QStringLiteral("user")) + QStringLiteral("/b-twin.json"),
                      manifestDocument(QStringLiteral("twin"), QStringLiteral("Second"))));

    ManifestCatalog catalog;
    QString error;
    QStringList skipped;
    QVERIFY2(catalog.loadDirectories({dir(QStringLiteral("system")), dir(QStringLiteral("user"))},
                                     &error, &skipped),
             qPrintable(error));
    QCOMPARE(idsOf(catalog), QStringList({QStringLiteral("alpha"), QStringLiteral("twin")}));
    QCOMPARE(catalog.findById(u"twin")->name, QStringLiteral("First"));
    QCOMPARE(skipped.size(), 3);
}

void ManifestCatalogMergeTest::anInvalidBaseEntryFailsTheLoadAndKeepsThePreviousCatalog()
{
    QVERIFY(manifest(QStringLiteral("good"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    ManifestCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectories({dir(QStringLiteral("good"))}, &error), qPrintable(error));

    QVERIFY(writeFile(dir(QStringLiteral("bad")) + QStringLiteral("/broken.json"),
                      QByteArray("{ nope")));
    QVERIFY(manifest(QStringLiteral("user"), QStringLiteral("extra"), QStringLiteral("Extra")));
    QVERIFY(!catalog.loadDirectories({dir(QStringLiteral("bad")), dir(QStringLiteral("user"))},
                                     &error));
    QVERIFY(error.contains(QStringLiteral("broken.json")));
    // Atomic, like loadDirectory: the previous catalog is untouched.
    QCOMPARE(idsOf(catalog), QStringList{QStringLiteral("alpha")});
}

void ManifestCatalogMergeTest::missingAndEmptyDirectoriesAreIgnoredButSomeManifestIsRequired()
{
    QVERIFY(QDir().mkpath(dir(QStringLiteral("empty"))));
    ManifestCatalog none;
    QString error;
    QVERIFY(!none.loadDirectories({dir(QStringLiteral("empty")), dir(QStringLiteral("absent"))},
                                  &error));
    QVERIFY(error.contains(QStringLiteral("No applet manifests found")));
    QVERIFY(!none.loadDirectories({}, &error));

    QVERIFY(manifest(QStringLiteral("system"), QStringLiteral("alpha"), QStringLiteral("Alpha")));
    ManifestCatalog some;
    QVERIFY2(some.loadDirectories({dir(QStringLiteral("absent")), dir(QStringLiteral("system")),
                                   dir(QStringLiteral("empty"))},
                                  &error),
             qPrintable(error));
    QCOMPARE(idsOf(some), QStringList{QStringLiteral("alpha")});
}

void ManifestCatalogMergeTest::theShippedCatalogMergesOverItselfWithoutChange()
{
    // The production layering when a user directory is a stale copy of the
    // shipped one: ids, count and order are exactly the shipped catalog's.
    const QString shipped = QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets");
    ManifestCatalog single;
    QString error;
    QVERIFY2(single.loadDirectory(shipped, &error), qPrintable(error));
    ManifestCatalog merged;
    QStringList skipped;
    QVERIFY2(merged.loadDirectories({shipped, shipped}, &error, &skipped), qPrintable(error));
    QCOMPARE(idsOf(merged), idsOf(single));
    QVERIFY(skipped.isEmpty());
}

QTEST_GUILESS_MAIN(ManifestCatalogMergeTest)
#include "tst_catalog_merge.moc"
