// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0230 (Gap 1): bounded readers for Steam's ACF/VDF metadata. The files
// are attacker-adjacent - synced from content servers, not authored by us -
// so malformed inputs must be rejected, never guessed. Fixtures are tiny
// synthetic documents; no real game data.

#include "qindaqt/compositor/steamappidentity.h"

#include <QtTest>

using namespace QindaQt::Compositor;

namespace {

[[nodiscard]] QByteArray modernLibraryFolders(const QList<QByteArray> &paths)
{
    QByteArray vdf = "\"libraryfolders\"\n{\n";
    for (int i = 0; i < paths.size(); ++i) {
        vdf += "\t\"" + QByteArray::number(i) + "\"\n\t{\n";
        vdf += "\t\t\"path\"\t\t\"" + paths.at(i) + "\"\n";
        vdf += "\t\t\"label\"\t\t\"\"\n";
        vdf += "\t}\n";
    }
    vdf += "}\n";
    return vdf;
}

[[nodiscard]] QByteArray appManifest(const QByteArray &name)
{
    QByteArray acf = "\"AppState\"\n{\n";
    acf += "\t\"appid\"\t\t\"620\"\n";
    acf += "\t\"name\"\t\t\"" + name + "\"\n";
    acf += "\t\"StateFlags\"\t\t\"4\"\n";
    acf += "}\n";
    return acf;
}

} // namespace

class SteamAppIdentityTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void steamIdsComeOnlyFromSteamKeys();
    void modernLibraryFoldersYieldEveryRoot();
    void legacyFlatLibraryFoldersStillParse();
    void escapedSeparatorsDecodeInPaths();
    void libraryFoldersRejectMalformedDocuments();
    void libraryFoldersRespectTheNestingCap();
    void libraryFoldersRejectOversizedDocuments();
    void manifestNamesResolve();
    void manifestNamesDecodeEscapes();
    void manifestWithoutUsableNameYieldsEmpty();
    void manifestNamesOutsideAppStateAreIgnored();
    void manifestNamesWithControlCharactersAreRefused();
    void manifestRejectsMalformedDocuments();
    void manifestRejectsOversizedDocuments();
};

void SteamAppIdentityTests::steamIdsComeOnlyFromSteamKeys()
{
    QCOMPARE(steamAppIdFromClass(QStringLiteral("steam_app_620")),
             std::optional<quint64>(620));
    QCOMPARE(steamAppIdFromClass(QStringLiteral("steam_app_0")),
             std::optional<quint64>(0));
    QCOMPARE(steamAppIdFromClass(QStringLiteral("STEAM_APP_42")),
             std::optional<quint64>(42));
    QCOMPARE(steamAppIdFromClass(QStringLiteral("  steam_app_7 ")),
             std::optional<quint64>(7));
    QVERIFY(!steamAppIdFromClass(QStringLiteral("steam_app_")).has_value());
    QVERIFY(!steamAppIdFromClass(QStringLiteral("steam_app_12a")).has_value());
    QVERIFY(!steamAppIdFromClass(QStringLiteral("xsteam_app_1")).has_value());
    QVERIFY(!steamAppIdFromClass(QStringLiteral("explorer.exe")).has_value());
    QVERIFY(!steamAppIdFromClass(QString()).has_value());
    // An overflowing digit run is refused, not wrapped.
    QVERIFY(!steamAppIdFromClass(
                 QStringLiteral("steam_app_99999999999999999999999999"))
                 .has_value());
}

void SteamAppIdentityTests::modernLibraryFoldersYieldEveryRoot()
{
    QStringList paths;
    QVERIFY(parseSteamLibraryFolders(
        modernLibraryFolders({QByteArray("/home/user/.steam/steam"),
                              QByteArray("/mnt/games/SteamLibrary")}),
        &paths));
    QCOMPARE(paths,
             (QStringList{QStringLiteral("/home/user/.steam/steam"),
                          QStringLiteral("/mnt/games/SteamLibrary")}));
}

void SteamAppIdentityTests::legacyFlatLibraryFoldersStillParse()
{
    const QByteArray vdf =
        "\"libraryfolders\"\n{\n"
        "\t\"0\"\t\"/home/user/.steam/steam\"\n"
        "\t\"1\"\t\"/mnt/games/SteamLibrary\"\n"
        "}\n";
    QStringList paths;
    QVERIFY(parseSteamLibraryFolders(vdf, &paths));
    QCOMPARE(paths.size(), 2);
}

void SteamAppIdentityTests::escapedSeparatorsDecodeInPaths()
{
    QStringList paths;
    QVERIFY(parseSteamLibraryFolders(
        modernLibraryFolders({QByteArray("D:\\\\SteamLibrary")}), &paths));
    QCOMPARE(paths, (QStringList{QStringLiteral("D:\\SteamLibrary")}));
}

void SteamAppIdentityTests::libraryFoldersRejectMalformedDocuments()
{
    QStringList paths;
    // Unterminated string.
    QVERIFY(!parseSteamLibraryFolders(
        QByteArray("\"libraryfolders\" { \"0\" \"/broken"), &paths));
    // Missing closing brace.
    QVERIFY(!parseSteamLibraryFolders(
        QByteArray("\"libraryfolders\" { \"0\" \"/x\" "), &paths));
    // Bare token where a quoted string belongs.
    QVERIFY(!parseSteamLibraryFolders(
        QByteArray("\"libraryfolders\" { 0 \"/x\" }"), &paths));
    // Trailing garbage after a complete document.
    QVERIFY(!parseSteamLibraryFolders(
        QByteArray("\"libraryfolders\" { } junk"), &paths));
    // The required root object is absent.
    QVERIFY(!parseSteamLibraryFolders(QByteArray("\"other\" { }"), &paths));
    QVERIFY(paths.isEmpty());
}

void SteamAppIdentityTests::libraryFoldersRespectTheNestingCap()
{
    QByteArray vdf;
    for (int i = 0; i < 16; ++i) {
        vdf += "\"k\" { ";
    }
    QStringList paths;
    QVERIFY(!parseSteamLibraryFolders(vdf, &paths));
}

void SteamAppIdentityTests::libraryFoldersRejectOversizedDocuments()
{
    QByteArray vdf = "\"libraryfolders\"\n{\n";
    for (int i = 0; i < 9000 && vdf.size() <= kMaxVdfBytes; ++i) {
        vdf += "\t\"" + QByteArray::number(i) + "\"\t\"/x\"\n";
    }
    vdf += "}";
    QVERIFY(vdf.size() > kMaxVdfBytes);
    QStringList paths;
    QVERIFY(!parseSteamLibraryFolders(vdf, &paths));
}

void SteamAppIdentityTests::manifestNamesResolve()
{
    QCOMPARE(parseSteamAppManifestName(appManifest("Portal 2")),
             QStringLiteral("Portal 2"));
}

void SteamAppIdentityTests::manifestNamesDecodeEscapes()
{
    QCOMPARE(parseSteamAppManifestName(appManifest("Half-Life 2 \\\"GOTY\\\"")),
             QStringLiteral("Half-Life 2 \"GOTY\""));
}

void SteamAppIdentityTests::manifestWithoutUsableNameYieldsEmpty()
{
    // Empty name.
    QVERIFY(parseSteamAppManifestName(appManifest("")).isEmpty());
    // No name key at all.
    QVERIFY(parseSteamAppManifestName(
                QByteArray("\"AppState\" { \"appid\" \"620\" }"))
                .isEmpty());
    // A name that is an object, not a string.
    QVERIFY(parseSteamAppManifestName(
                QByteArray("\"AppState\" { \"name\" { \"x\" \"y\" } }"))
                .isEmpty());
}

void SteamAppIdentityTests::manifestNamesOutsideAppStateAreIgnored()
{
    const QByteArray acf =
        "\"name\" \"Decoy\"\n"
        "\"AppState\" { \"appid\" \"620\" }\n";
    QVERIFY(parseSteamAppManifestName(acf).isEmpty());
    const QByteArray wrongRoot = "\"Other\" { \"name\" \"Decoy\" }";
    QVERIFY(parseSteamAppManifestName(wrongRoot).isEmpty());
}

void SteamAppIdentityTests::manifestNamesWithControlCharactersAreRefused()
{
    QVERIFY(parseSteamAppManifestName(appManifest("Bad\tName")).isEmpty());
    QVERIFY(parseSteamAppManifestName(appManifest("Bad\nName")).isEmpty());
}

void SteamAppIdentityTests::manifestRejectsMalformedDocuments()
{
    QVERIFY(parseSteamAppManifestName(QByteArray("\"AppState\" { \"name\" \"x\""))
                .isEmpty());
    QVERIFY(parseSteamAppManifestName(QByteArray("not a vdf file")).isEmpty());
    QVERIFY(parseSteamAppManifestName(QByteArray()).isEmpty());
}

void SteamAppIdentityTests::manifestRejectsOversizedDocuments()
{
    QByteArray acf = "\"AppState\" { \"name\" \"";
    acf += QByteArray(int(kMaxVdfBytes), 'x');
    acf += "\" }";
    QVERIFY(acf.size() > kMaxVdfBytes);
    QVERIFY(parseSteamAppManifestName(acf).isEmpty());
}

QTEST_GUILESS_MAIN(SteamAppIdentityTests)

#include "tst_steamappidentity.moc"
