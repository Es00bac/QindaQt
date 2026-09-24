// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_test_fixtures.h"

#include "qindaqt/profiles/profile_catalog.h"
#include "qindaqt/profiles/profile_loader.h"

#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
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
    void stockProfileResolvesNonemptyDesktopInventory();
    void theDefaultProfilePlacesTheStreamingApplet();
    void familiarExperiencesPairTheirThemesAndMenus();
    void macDockKeepsTheFileManagerFirstAndTheTrashLast();
    void fileManagerHintDefaultsRoundTripsAndRejectsBlank();
};

void ProfileTests::loadsEveryBuiltInProfile()
{
    const QString directory = QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles");
    const auto results = ProfileLoader::fromDirectory(directory);
    QVERIFY2(results.size() >= 11, "The built-in profile set unexpectedly shrank");

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
    const auto sectionless = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/xfce-inspired.json"));
    QVERIFY2(sectionless.ok, qPrintable(sectionless.error.diagnostic()));
    QVERIFY(!sectionless.profile.toJson().toVariantMap()
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

void ProfileTests::stockProfileResolvesNonemptyDesktopInventory()
{
    const auto result = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/qindaqt.json"));
    QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));

    // The QindaQt signature layout uses three edges (ADR-0223): a thin top
    // command bar for the focused window and its menu, a left smart shelf for
    // windows and containers, and a bottom-right instrument strip. The
    // workflow hint keeps naming the shelf, which is still the shelf — it
    // moved from the bottom edge to the left one.
    QCOMPARE(result.profile.defaultTheme, QStringLiteral("qinda-dark"));
    QCOMPARE(result.profile.panels.size(), 3);
    QCOMPARE(result.profile.workflow.launcher, QStringLiteral("smart-shelf"));
    QCOMPARE(result.profile.workflow.menu, QStringLiteral("global"));
    QVERIFY(result.profile.workflow.globalMenu);

    QVERIFY2(!result.profile.desktopApplets.isEmpty(),
             "The stock qindaqt profile must resolve a nonempty desktop inventory");
    QCOMPARE(result.profile.desktopApplets.constFirst().plugin,
             QStringLiteral("desktop-icons"));
}

// An applet nobody places is an applet nobody has. The OBS applet shipped in
// O10 with its manifest, capabilities, policy grant and controller all correct
// and no stock profile putting it on a panel, so the 2026-09-18 end-to-end
// check found nothing to connect: obs-websocket logged no client for the whole
// session.
//
// It sits in the flagship profile only, beside the other QindaQt hardware chips
// (audio, bluetooth, power, smart-lights). The desktop-imitation profiles
// deliberately carry none of those, and a chip reading "OBS is not running" on
// a GNOME-imitation top bar for a user who never installed OBS is clutter, not
// discovery. Existing user layouts add it through Meta+right-click.
void ProfileTests::theDefaultProfilePlacesTheStreamingApplet()
{
    const auto result = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/qindaqt.json"));
    QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));

    qsizetype obsCount = 0;
    QString zone;
    for (const auto &panel : result.profile.panels) {
        for (const auto &applet : panel.applets) {
            if (applet.plugin != QLatin1String("obs")) {
                continue;
            }
            ++obsCount;
            zone = applet.settings.value(QStringLiteral("zone")).toString();
        }
    }

    QCOMPARE(obsCount, 1);
    QCOMPARE(zone, QStringLiteral("end"));
}

// ADR-0268: a desktop experience is a layout, the theme it pairs with (which
// carries the W19 button style), whether the global menu shows, and the File
// Manager arrangement it expects. What the user reads names no vendor.
void ProfileTests::familiarExperiencesPairTheirThemesAndMenus()
{
    const struct {
        const char *profile;
        const char *theme;
        bool globalMenu;
        const char *fileManager;
    } experiences[] = {
        {"macos-inspired", "qinda-macos", true, "finder"},
        {"qinda-bliss", "qinda-bliss", false, "explorer"},
        {"windows-modern", "qinda-daylight", false, "explorer"},
        {"beos-inspired", "qinda-marigold", false, "finder"},
        {"win31-inspired", "qinda-classic-grey", false, "explorer"},
        {"nextstep-inspired", "qinda-graphite", false, "finder"},
    };
    for (const auto &experience : experiences) {
        const QString id = QString::fromLatin1(experience.profile);
        const auto result = ProfileLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/") + id + QStringLiteral(".json"));
        QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));
        QCOMPARE(result.profile.id, id);
        QCOMPARE(result.profile.defaultTheme, QString::fromLatin1(experience.theme));
        QCOMPARE(result.profile.workflow.globalMenu, experience.globalMenu);
        QCOMPARE(result.profile.workflow.fileManager, QString::fromLatin1(experience.fileManager));
    }

    // "Windows" mid-sentence is the product; a sentence may still begin with
    // the plural of window.
    const QRegularExpression product(QStringLiteral("[a-z,] Windows\\b"));
    const auto results = ProfileLoader::fromDirectory(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"));
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));
        const QString &name = result.profile.name;
        const QString &description = result.profile.description;
        QVERIFY2(!name.contains(QStringLiteral("Windows"))
                     && !product.match(description).hasMatch(),
                 qPrintable(result.profile.id));
        for (const char *mark : {"Microsoft", "Luna", "Bliss", "macOS", "Apple", "Finder",
                                 "BeOS", "NeXT"}) {
            const QString word = QString::fromLatin1(mark);
            QVERIFY2(!name.contains(word) && !description.contains(word),
                     qPrintable(result.profile.id + QStringLiteral(" names ") + word));
        }
    }

    // Program Groups has no task bar: a minimized window becomes a desktop
    // icon (its theme's minimize rolls up, ADR-0203), so no task list shows.
    const auto classic = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/win31-inspired.json"));
    QVERIFY2(classic.ok, qPrintable(classic.error.diagnostic()));
    QCOMPARE(classic.profile.workflow.taskList, QStringLiteral("hidden"));
    // Corner Bar holds the top-right corner only.
    const auto corner = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/beos-inspired.json"));
    QVERIFY2(corner.ok, qPrintable(corner.error.diagnostic()));
    QCOMPARE(corner.profile.panels.size(), 1);
    QCOMPARE(static_cast<int>(corner.profile.panels.constFirst().edge),
             static_cast<int>(Edge::Top));
    QCOMPARE(static_cast<int>(corner.profile.panels.constFirst().alignment),
             static_cast<int>(Alignment::End));
    QVERIFY(corner.profile.panels.constFirst().length < 1.0);
}

// ADR-0268 (plan W12): the Mac-style dock keeps the File Manager as its
// permanent first tile and the Trash as its permanent last, and the menu bar
// keeps the system menu (the Nest mark, ADR-0263's W18) at its far left.
void ProfileTests::macDockKeepsTheFileManagerFirstAndTheTrashLast()
{
    const auto result = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/macos-inspired.json"));
    QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));
    QCOMPARE(result.profile.panels.size(), 2);
    QCOMPARE(result.profile.panels.constFirst().applets.constFirst().plugin,
             QStringLiteral("system-menu"));
    const PanelSpec &dock = result.profile.panels.at(1);
    QCOMPARE(dock.id, QStringLiteral("dock"));
    QStringList order;
    for (const auto &applet : dock.applets) {
        order.append(applet.plugin + QLatin1Char(':')
                     + applet.settings.value(QStringLiteral("items")).toString());
    }
    QCOMPARE(order, QStringList({QStringLiteral("quick-launch:file-manager"),
                                 QStringLiteral("launcher:"),
                                 QStringLiteral("quick-launch:others"),
                                 QStringLiteral("task-list:"),
                                 QStringLiteral("quick-launch:trash")}));
}

void ProfileTests::fileManagerHintDefaultsRoundTripsAndRejectsBlank()
{
    // Absent: the File Manager's own default arrangement, Finder.
    QJsonObject profile = validProfileObject();
    const auto absent = ProfileLoader::fromJson(encode(profile), QStringLiteral("absent"));
    QVERIFY2(absent.ok, qPrintable(absent.error.diagnostic()));
    QCOMPARE(absent.profile.workflow.fileManager, QStringLiteral("finder"));

    // Present: kept through the strict serializer, so a saved copy of the
    // layout keeps it (AGENT-CONTRACT: W11s reads it as the default style).
    profile.insert(QStringLiteral("workflow"),
                   QJsonObject{{QStringLiteral("fileManager"), QStringLiteral("commander")}});
    const auto present = ProfileLoader::fromJson(encode(profile), QStringLiteral("present"));
    QVERIFY2(present.ok, qPrintable(present.error.diagnostic()));
    QCOMPARE(present.profile.workflow.fileManager, QStringLiteral("commander"));
    const auto roundTrip = ProfileLoader::fromJson(
        QJsonDocument(present.profile.toJson()).toJson(), QStringLiteral("round-trip"));
    QVERIFY2(roundTrip.ok, qPrintable(roundTrip.error.diagnostic()));
    QCOMPARE(roundTrip.profile.workflow.fileManager, QStringLiteral("commander"));

    // Blank or mistyped: rejected like every other workflow hint.
    profile.insert(QStringLiteral("workflow"),
                   QJsonObject{{QStringLiteral("fileManager"), QStringLiteral(" ")}});
    const auto blank = ProfileLoader::fromJson(encode(profile), QStringLiteral("blank"));
    QVERIFY(!blank.ok);
    QCOMPARE(blank.error.code, ProfileErrorCode::InvalidValue);
    QCOMPARE(blank.error.path, QStringLiteral("/workflow/fileManager"));
    profile.insert(QStringLiteral("workflow"),
                   QJsonObject{{QStringLiteral("fileManager"), true}});
    const auto mistyped = ProfileLoader::fromJson(encode(profile), QStringLiteral("mistyped"));
    QVERIFY(!mistyped.ok);
    QCOMPARE(mistyped.error.code, ProfileErrorCode::InvalidFieldType);
}

QTEST_GUILESS_MAIN(ProfileTests)
#include "tst_profiles.moc"
