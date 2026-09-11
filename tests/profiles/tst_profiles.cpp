// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_test_fixtures.h"

#include "qindaqt/profiles/profile_catalog.h"
#include "qindaqt/profiles/profile_loader.h"

#include <QFile>
#include <QJsonDocument>
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
    void catalogMergesDirectoriesWithLaterPrecedence();
    void catalogMergeRejectsDuplicatesWithinOneDirectory();
    void macosProfileUsesQindaMacosTheme();
    void blissProfileCarriesDesktopSection();
    void rejectsDuplicateIdsAcrossPanelsAndDesktop();
    void everyBuiltInProfileHasOneNotificationCenter();
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

void ProfileTests::catalogMergesDirectoriesWithLaterPrecedence()
{
    QTemporaryDir systemDirectory;
    QTemporaryDir userDirectory;
    QVERIFY(systemDirectory.isValid());
    QVERIFY(userDirectory.isValid());

    const auto writeProfile = [](QTemporaryDir &directory, const QString &fileName,
                                 const QString &id, const QString &name) {
        QFile file(directory.filePath(fileName));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QJsonObject profile = validProfileObject();
        profile.insert(QStringLiteral("id"), id);
        profile.insert(QStringLiteral("name"), name);
        const QByteArray encoded = encode(profile);
        QCOMPARE(file.write(encoded), static_cast<qint64>(encoded.size()));
    };
    writeProfile(systemDirectory, QStringLiteral("stock.json"),
                 QStringLiteral("stock"), QStringLiteral("Stock"));
    writeProfile(systemDirectory, QStringLiteral("shared.json"),
                 QStringLiteral("shared"), QStringLiteral("System shared"));
    // A partial user catalog must not shadow the remaining stock profiles.
    writeProfile(userDirectory, QStringLiteral("shared.json"),
                 QStringLiteral("shared"), QStringLiteral("User shared"));

    ProfileCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectories({systemDirectory.path(), userDirectory.path()},
                                     &error),
             qPrintable(error));
    QCOMPARE(catalog.profiles().size(), 2);
    QVERIFY(catalog.selectById(QStringLiteral("shared")));
    QCOMPARE(catalog.current().value(QStringLiteral("name")).toString(),
             QStringLiteral("User shared"));
    QVERIFY(catalog.selectById(QStringLiteral("stock")));
    QCOMPARE(catalog.current().value(QStringLiteral("name")).toString(),
             QStringLiteral("Stock"));
}

void ProfileTests::catalogMergeRejectsDuplicatesWithinOneDirectory()
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

    // Duplicate ids inside one contributing directory fail the whole merge
    // atomically even though cross-directory override is legal.
    QVERIFY(!catalog.loadDirectories(
        {QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), directory.path()},
        &error));
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

void ProfileTests::blissProfileCarriesDesktopSection()
{
    const auto result = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/qinda-bliss.json"));
    QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));
    QCOMPARE(result.profile.defaultTheme, QStringLiteral("qinda-bliss"));
    QCOMPARE(result.profile.desktopApplets.size(), 1);
    QCOMPARE(result.profile.desktopApplets.constFirst().plugin,
             QStringLiteral("desktop-icons"));
    QCOMPARE(result.profile.desktopApplets.constFirst()
                 .settings.value(QStringLiteral("placement")).toString(),
             QStringLiteral("left"));

    // The desktop section round-trips through the strict serializer so user
    // edits never silently drop it (ADR-0125).
    const auto roundTrip = ProfileLoader::fromJson(
        QJsonDocument(result.profile.toJson()).toJson(), QStringLiteral("round-trip"));
    QVERIFY2(roundTrip.ok, qPrintable(roundTrip.error.diagnostic()));
    QCOMPARE(roundTrip.profile.desktopApplets.size(), 1);
    QCOMPARE(roundTrip.profile.desktopApplets.constFirst().id,
             QStringLiteral("desktop-icons"));

    // Profiles without the section serialize without the key at all.
    const auto classic = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/windows-classic.json"));
    QVERIFY2(classic.ok, qPrintable(classic.error.diagnostic()));
    QVERIFY(!classic.profile.toJson().toVariantMap()
                 .contains(QStringLiteral("desktop")));
}

void ProfileTests::rejectsDuplicateIdsAcrossPanelsAndDesktop()
{
    constexpr auto invalid = R"json({
        "schemaVersion": 1,
        "id": "dupe",
        "name": "Dupe",
        "panels": [{
            "id": "bar",
            "edge": "bottom",
            "applets": [{"id": "tray", "plugin": "clock"}]
        }],
        "desktop": {
            "applets": [{"id": "tray", "plugin": "desktop-icons"}]
        }
    })json";
    const auto result = ProfileLoader::fromJson(invalid, QStringLiteral("fixture"));
    QVERIFY(!result.ok);
    QCOMPARE(result.error.code, ProfileErrorCode::DuplicateAppletId);
}

void ProfileTests::everyBuiltInProfileHasOneNotificationCenter()
{
    const QString directory = QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles");
    const auto results = ProfileLoader::fromDirectory(directory);
    QVERIFY2(!results.isEmpty(), "The built-in profile directory must not be empty");

    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));

        qsizetype notificationCenterCount = 0;
        for (const auto &panel : result.profile.panels) {
            for (const auto &applet : panel.applets) {
                notificationCenterCount +=
                    applet.plugin == QLatin1String("notification-center") ? 1 : 0;
            }
        }

        QCOMPARE(notificationCenterCount, 1);
    }
}

QTEST_GUILESS_MAIN(ProfileTests)
#include "tst_profiles.moc"
