// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/screensaver_catalog.h>

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Session::DesktopControls;

namespace {

// Writes a desktop entry with the exact shape of one shipped saver package.
// The fixtures replicate all five installed entries so the predicate is
// proven against what discovery actually meets, not against an idealized
// marker none of them carry.
void writeEntry(const QDir &directory, const QString &fileName,
                const QString &contents)
{
    QFile file(directory.absoluteFilePath(fileName));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(contents.toUtf8());
}

const auto kPatrol = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Qinda Patrol\n"
    "Comment=A kind-of-cute autonomous cyberpunk screensaver\n"
    "Exec=qinda-patrol --preview\n"
    "Icon=qinda-patrol\n"
    "Keywords=QindaQt;Screensaver;Penguin;\n"
    "Actions=Screensaver;\n"
    "\n"
    "[Desktop Action Screensaver]\n"
    "Name=Start night patrol\n"
    "Exec=qinda-patrol --screensaver\n");

const auto kReef = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Circuit Reef\n"
    "Comment=Kind of Quiet: a cyberpunk aquarium for QindaQt\n"
    "Exec=circuit-reef --windowed\n"
    "Icon=studio.qinda.CircuitReef\n"
    "Keywords=QindaQt;Screensaver;Aquarium;\n"
    "Actions=Private;\n"
    "\n"
    "[Desktop Action Private]\n"
    "Name=Start Without Telemetry\n"
    "Exec=circuit-reef --fullscreen --all-screens --private\n");

const auto kBrawl = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Prism Brawl\n"
    "Comment=Autonomous cyber-animal platform-fighting screensaver\n"
    "Exec=prism-brawl --windowed\n"
    "Actions=Quiet;\n"
    "Keywords=QindaQt;screensaver;cyberpunk;\n"
    "\n"
    "[Desktop Action Quiet]\n"
    "Name=Lower-power visual\n"
    "Exec=prism-brawl --all-screens --eco --mute\n");

const auto kCircuit = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Prism Circuit\n"
    "GenericName=Autonomous 3D Racing Screensaver\n"
    "Comment=CyberPengu and friends on a prismatic cyberpunk circuit\n"
    "Exec=prism-circuit --windowed\n"
    "Actions=Fullscreen;\n"
    "\n"
    "[Desktop Action Fullscreen]\n"
    "Name=Fullscreen Screensaver\n"
    "Exec=prism-circuit --all-screens\n");

const auto kStarward = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Starward Reimagined\n"
    "Comment=CyberPengu and Ducke, beyond the shipping lanes\n"
    "Exec=starward --windowed\n"
    "Actions=Fullscreen;\n"
    "\n"
    "[Desktop Action Fullscreen]\n"
    "Name=Fullscreen screensaver\n"
    "Exec=starward --all-screens\n");

} // namespace

class ScreensaverCatalogTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void discoversEveryShippedSaverShape();
    void entryCarriesFixedArgumentsAndLockTruth();
    void rejectsNonSaverEntries();
    void requiresTheLaunchContract();
    void reservedTokensAreNeverDiscovered();
    void firstDirectoryWinsADuplicateToken();
    void unknownDiscoveredSaverGetsThePlainContract();
    void userWritableLocationIsNeverScanned();
    void fixedArgumentAndSceneTables();

private:
    QTemporaryDir m_root{QStringLiteral("qindaqt-catalog-XXXXXX")};
};

void ScreensaverCatalogTest::discoversEveryShippedSaverShape()
{
    QVERIFY(m_root.isValid());
    const QDir directory(m_root->path());
    writeEntry(directory, QStringLiteral("org.qindaqt.Patrol.desktop"), kPatrol);
    writeEntry(directory, QStringLiteral("studio.qinda.CircuitReef.desktop"), kReef);
    writeEntry(directory, QStringLiteral("studio.qinda.PrismBrawl.desktop"), kBrawl);
    writeEntry(directory, QStringLiteral("studio.qinda.PrismCircuit.desktop"), kCircuit);
    writeEntry(directory, QStringLiteral("studio.qinda.Starward.desktop"), kStarward);

    const DesktopEntryScreensaverCatalog catalog({m_root->path()});
    QStringList tokens;
    for (const ScreensaverCatalogEntry &entry : catalog.entries()) {
        tokens.append(entry.token);
    }
    QCOMPARE(tokens.size(), 5);
    QVERIFY(tokens.contains(QStringLiteral("qinda-patrol")));
    QVERIFY(tokens.contains(QStringLiteral("circuit-reef")));
    QVERIFY(tokens.contains(QStringLiteral("prism-brawl")));
    QVERIFY(tokens.contains(QStringLiteral("prism-circuit")));
    QVERIFY(tokens.contains(QStringLiteral("starward")));

    const auto reef = catalog.entry(QStringLiteral("circuit-reef"));
    QVERIFY(reef.has_value());
    QCOMPARE(reef->name, QStringLiteral("Circuit Reef"));
    QCOMPARE(reef->iconName, QStringLiteral("studio.qinda.CircuitReef"));
    QVERIFY(!catalog.entry(QStringLiteral("qinda-patrol")).value().comment.isEmpty());
    QVERIFY(!catalog.entry(QStringLiteral("missing")).has_value());
}

void ScreensaverCatalogTest::entryCarriesFixedArgumentsAndLockTruth()
{
    QVERIFY(m_root.isValid());
    const QDir directory(m_root->path());
    writeEntry(directory, QStringLiteral("org.qindaqt.Patrol.desktop"), kPatrol);
    writeEntry(directory, QStringLiteral("studio.qinda.Starward.desktop"), kStarward);

    const DesktopEntryScreensaverCatalog catalog({m_root->path()});
    const auto patrol = catalog.entry(QStringLiteral("qinda-patrol"));
    QVERIFY(patrol.has_value());
    QCOMPARE(patrol->arguments,
             (QStringList{QStringLiteral("--screensaver"),
                          QStringLiteral("--no-metrics")}));
    QVERIFY(patrol->showsOnLockScreen);

    const auto starward = catalog.entry(QStringLiteral("starward"));
    QVERIFY(starward.has_value());
    QCOMPARE(starward->arguments, QStringList{QStringLiteral("--screensaver")});
    QVERIFY(!starward->showsOnLockScreen);
}

void ScreensaverCatalogTest::rejectsNonSaverEntries()
{
    QVERIFY(m_root.isValid());
    const QDir directory(m_root->path());
    // Not an application at all.
    writeEntry(directory, QStringLiteral("a.desktop"),
               QStringLiteral("[Desktop Entry]\nType=Link\nName=Not an app\n"
                              "Comment=screensaver documentation\n"
                              "Exec=qinda-patrol --screensaver\n"));
    // Hidden entries are deleted-but-present; they must not come back.
    writeEntry(directory, QStringLiteral("b.desktop"),
               QStringLiteral("[Desktop Entry]\nType=Application\nHidden=true\n"
                              "Name=Hidden Patrol\nComment=screensaver\n"
                              "Exec=qinda-patrol --screensaver\n"
                              "[Desktop Action Screensaver]\nName=Run\n"
                              "Exec=qinda-patrol --screensaver\n"));
    // No screensaver attestation anywhere: an ordinary app.
    writeEntry(directory, QStringLiteral("c.desktop"),
               QStringLiteral("[Desktop Entry]\nType=Application\n"
                              "Name=Text Editor\nComment=Edit text files\n"
                              "Exec=qinda-editor\n"));
    // No Exec at all cannot be launched.
    writeEntry(directory, QStringLiteral("d.desktop"),
               QStringLiteral("[Desktop Entry]\nType=Application\n"
                              "Name=Broken Saver\nComment=screensaver\n"));

    const DesktopEntryScreensaverCatalog catalog({m_root->path()});
    QVERIFY(catalog.entries().isEmpty());
}

void ScreensaverCatalogTest::requiresTheLaunchContract()
{
    QVERIFY(m_root.isValid());
    const QDir directory(m_root->path());
    // Claims to be a screensaver but offers no all-outputs action: the
    // launcher would start it with flags it never documented.
    writeEntry(directory, QStringLiteral("a.desktop"),
               QStringLiteral("[Desktop Entry]\nType=Application\n"
                              "Name=Shy Saver\nComment=A screensaver\n"
                              "Exec=shy-saver --windowed\n"));
    // The action runs a different program entirely.
    writeEntry(directory, QStringLiteral("b.desktop"),
               QStringLiteral("[Desktop Entry]\nType=Application\n"
                              "Name=Proxy Saver\nComment=A screensaver\n"
                              "Exec=proxy-saver --windowed\n"
                              "[Desktop Action Fullscreen]\nName=Go\n"
                              "Exec=some-other-program --all-screens\n"));

    const DesktopEntryScreensaverCatalog catalog({m_root->path()});
    QVERIFY(catalog.entries().isEmpty());
}

void ScreensaverCatalogTest::reservedTokensAreNeverDiscovered()
{
    QVERIFY(m_root.isValid());
    const QDir directory(m_root->path());
    // A program literally named "blank" would collide with the reserved
    // lock-screen token; discovery refuses it.
    writeEntry(directory, QStringLiteral("a.desktop"),
               QStringLiteral("[Desktop Entry]\nType=Application\n"
                              "Name=Blank Impostor\nComment=screensaver\n"
                              "Exec=blank --windowed\n"
                              "[Desktop Action Fullscreen]\nName=Go\n"
                              "Exec=blank --all-screens\n"));

    const DesktopEntryScreensaverCatalog catalog({m_root->path()});
    QVERIFY(catalog.entries().isEmpty());
}

void ScreensaverCatalogTest::firstDirectoryWinsADuplicateToken()
{
    QVERIFY(m_root.isValid());
    const QDir first(m_root->path());
    const QDir second(QDir(m_root->path()).absoluteFilePath(QStringLiteral("second")));
    QVERIFY(QDir().mkpath(second.absolutePath()));
    writeEntry(first, QStringLiteral("org.qindaqt.Patrol.desktop"), kPatrol);
    writeEntry(second, QStringLiteral("studio.qinda.PrismBrawl.desktop"),
               QString(kBrawl).replace(QStringLiteral("prism-brawl"),
                                       QStringLiteral("qinda-patrol")));

    const DesktopEntryScreensaverCatalog catalog(
        {first.absolutePath(), second.absolutePath()});
    QCOMPARE(catalog.entries().size(), 1);
    QCOMPARE(catalog.entries().constFirst().name, QStringLiteral("Qinda Patrol"));
}

void ScreensaverCatalogTest::unknownDiscoveredSaverGetsThePlainContract()
{
    QVERIFY(m_root.isValid());
    const QDir directory(m_root->path());
    // A saver the house tables do not know still appears, with the plain
    // --screensaver contract and no lock-screen claim (ADR-0226).
    writeEntry(directory, QStringLiteral("a.desktop"),
               QStringLiteral("[Desktop Entry]\nType=Application\n"
                              "Name=New Saver\nComment=A shiny screensaver\n"
                              "Exec=new-saver --windowed\n"
                              "[Desktop Action Fullscreen]\nName=Go\n"
                              "Exec=new-saver --all-screens\n"));

    const DesktopEntryScreensaverCatalog catalog({m_root->path()});
    const auto entry = catalog.entry(QStringLiteral("new-saver"));
    QVERIFY(entry.has_value());
    QCOMPARE(entry->name, QStringLiteral("New Saver"));
    QCOMPARE(entry->arguments, QStringList{QStringLiteral("--screensaver")});
    QVERIFY(!entry->showsOnLockScreen);
}

void ScreensaverCatalogTest::userWritableLocationIsNeverScanned()
{
    // AGENT-GUARD: persistence resolves through this catalog, so a token can
    // never name a program planted in the user-writable applications
    // directory. The catalog's system directory list must exclude it.
    const QString writable = QStandardPaths::writableLocation(
        QStandardPaths::ApplicationsLocation);
    QVERIFY(!writable.isEmpty());
    QVERIFY(!DesktopEntryScreensaverCatalog::systemApplicationDirectories()
                 .contains(writable));
}

void ScreensaverCatalogTest::fixedArgumentAndSceneTables()
{
    // AGENT-CONTRACT: every saver is started with --screensaver, which each
    // one documents as covering every connected output, plus the flag that
    // stops what an unattended screen must not do. These are the launcher's
    // only command lines, asserted as a set.
    const QList<QPair<QString, QStringList>> expected{
        {QStringLiteral("qinda-patrol"),
         {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")}},
        {QStringLiteral("circuit-reef"),
         {QStringLiteral("--screensaver"), QStringLiteral("--private")}},
        {QStringLiteral("prism-circuit"),
         {QStringLiteral("--screensaver"), QStringLiteral("--mute")}},
        {QStringLiteral("prism-brawl"),
         {QStringLiteral("--screensaver"), QStringLiteral("--mute")}},
        {QStringLiteral("starward"), {QStringLiteral("--screensaver")}},
    };
    for (const auto &[token, arguments] : expected) {
        QCOMPARE(DesktopEntryScreensaverCatalog::launchArguments(token), arguments);
    }

    // The greeter draws a saver by importing its QML module (ADR-0216). The
    // three SDL/OpenGL savers ship none, and saying otherwise would hand the
    // locker a wallpaper plugin with nothing to draw.
    QVERIFY(DesktopEntryScreensaverCatalog::shipsLockScreenScene(
        QStringLiteral("qinda-patrol")));
    QVERIFY(DesktopEntryScreensaverCatalog::shipsLockScreenScene(
        QStringLiteral("circuit-reef")));
    QVERIFY(!DesktopEntryScreensaverCatalog::shipsLockScreenScene(
        QStringLiteral("prism-circuit")));
    QVERIFY(!DesktopEntryScreensaverCatalog::shipsLockScreenScene(
        QStringLiteral("prism-brawl")));
    QVERIFY(!DesktopEntryScreensaverCatalog::shipsLockScreenScene(
        QStringLiteral("starward")));
    QVERIFY(!DesktopEntryScreensaverCatalog::shipsLockScreenScene(
        QStringLiteral("a-saver-that-does-not-exist")));
}

QTEST_MAIN(ScreensaverCatalogTest)
#include "tst_screensaver_catalog.moc"
