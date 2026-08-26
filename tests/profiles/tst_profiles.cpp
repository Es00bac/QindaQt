// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_test_fixtures.h"

#include "qindaqt/profiles/profile_catalog.h"
#include "qindaqt/profiles/profile_loader.h"

#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Profiles;
using namespace QindaQt::Profiles::TestFixtures;

class ProfileTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsEveryBuiltInProfile();
    void rejectsInvalidPanelGeometry();
    void reportsFileReadFailure();
    void reportsPostOpenReadFailure();
    void catalogSelectsByStableId();
    void catalogRejectsDuplicateIdsAtomically();
    void macosProfileUsesQindaMacosTheme();
};

void ProfileTests::loadsEveryBuiltInProfile()
{
    const QString directory = QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles");
    const auto results = ProfileLoader::fromDirectory(directory);
    QVERIFY2(results.size() >= 9, "The built-in profile set unexpectedly shrank");

    QSet<QString> ids;
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));
        QVERIFY(!result.profile.panels.isEmpty());
        QVERIFY(!ids.contains(result.profile.id));
        ids.insert(result.profile.id);
    }
}

void ProfileTests::rejectsInvalidPanelGeometry()
{
    constexpr auto invalid = R"json({
        "schemaVersion": 1,
        "id": "bad",
        "name": "Bad",
        "panels": [{
            "id": "oversized",
            "edge": "top",
            "rows": 99,
            "thickness": 12,
            "length": 2.0,
            "applets": []
        }]
    })json";
    const auto result = ProfileLoader::fromJson(invalid, QStringLiteral("fixture"));
    QVERIFY(!result.ok);
    QCOMPARE(result.error.code, ProfileErrorCode::OutOfRange);
    QCOMPARE(result.error.path, QStringLiteral("/panels/0/rows"));
}

void ProfileTests::reportsFileReadFailure()
{
    const QString path = QStringLiteral("/definitely-absent/qindaqt-profile.json");
    const LoadResult result = ProfileLoader::fromFile(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error.code, ProfileErrorCode::FileReadFailed);
    QCOMPARE(result.error.origin, path);
    QVERIFY(!result.error.message.isEmpty());
}

void ProfileTests::reportsPostOpenReadFailure()
{
#if defined(Q_OS_LINUX)
    const QString path = QStringLiteral("/proc/self/mem");
    QFile probe(path);
    if (!probe.open(QIODevice::ReadOnly)) {
        QSKIP("This Linux environment does not expose a readable /proc/self/mem handle");
    }
    (void)probe.readAll();
    if (probe.error() == QFileDevice::NoError) {
        QSKIP("This Linux environment does not produce a post-open read error for /proc/self/mem");
    }

    const LoadResult result = ProfileLoader::fromFile(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error.code, ProfileErrorCode::FileReadFailed);
    QCOMPARE(result.error.origin, path);
    QVERIFY(!result.error.message.isEmpty());
#else
    QSKIP("QindaQt's readable-but-failing file regression uses Linux procfs");
#endif
}

void ProfileTests::catalogSelectsByStableId()
{
    ProfileCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));
    QVERIFY(catalog.selectById(QStringLiteral("unity-inspired")));
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(), QStringLiteral("unity-inspired"));
    QVERIFY(!catalog.selectById(QStringLiteral("missing-profile")));
}

void ProfileTests::catalogRejectsDuplicateIdsAtomically()
{
    ProfileCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));
    const QVariantMap previous = catalog.current();

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    for (const QString &name : {QStringLiteral("one.json"), QStringLiteral("two.json")}) {
        QFile file(directory.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QJsonObject profile = validProfileObject();
        profile.insert(QStringLiteral("id"), QStringLiteral("repeated-profile"));
        const QByteArray encoded = encode(profile);
        QCOMPARE(file.write(encoded), static_cast<qint64>(encoded.size()));
    }

    QVERIFY(!catalog.loadDirectory(directory.path(), &error));
    QVERIFY(error.contains(QStringLiteral("duplicate profile id")));
    QCOMPARE(catalog.current(), previous);
}

void ProfileTests::macosProfileUsesQindaMacosTheme()
{
    const auto result = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/macos-inspired.json"));
    QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));
    QCOMPARE(result.profile.defaultTheme, QStringLiteral("qinda-macos"));
}

QTEST_GUILESS_MAIN(ProfileTests)
#include "tst_profiles.moc"
