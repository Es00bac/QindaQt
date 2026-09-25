// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include "download_allowlist.h"
#include "job_fakes.h"
#include "prefix_paths.h"
#include "store_recipes.h"

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

class tst_store_recipes : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void tableHasTheSixLaunchers() {
    QStringList ids;
    for (const StoreRecipe &recipe : storeRecipes()) {
      ids.append(recipe.id);
    }
    QCOMPARE(ids, QStringList({QStringLiteral("battlenet"), QStringLiteral("ea"),
                               QStringLiteral("ubisoft"), QStringLiteral("egs-launcher"),
                               QStringLiteral("gog-galaxy"), QStringLiteral("amazon-games")}));
    QVERIFY(findStoreRecipe(QStringLiteral("battlenet")).has_value());
    QVERIFY(!findStoreRecipe(QStringLiteral("microsoft-store")).has_value());
  }

  void everyRecipeValidates() {
    QSet<QString> prefixes;
    for (const StoreRecipe &recipe : storeRecipes()) {
      const QStringList problems = validateStoreRecipe(recipe);
      QVERIFY2(problems.isEmpty(), qPrintable(recipe.id + QStringLiteral(": ") + problems.join(QStringLiteral("; "))));
      QVERIFY2(isAllowedDownloadUrl(recipe.installerUrl), qPrintable(recipe.id));
      QVERIFY(!recipe.launcherExecutableCandidates.isEmpty());
      QVERIFY(isSafePrefixDirName(recipe.defaultPrefixDirName));
      QVERIFY2(!prefixes.contains(recipe.defaultPrefixDirName), qPrintable(recipe.id));
      prefixes.insert(recipe.defaultPrefixDirName);
      QCOMPARE(recipe.umuId, QStringLiteral("umu-0"));
    }
  }

  void umuStoresMatchUmuProtonfixes_data() {
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("store");
    QTest::newRow("battlenet") << QStringLiteral("battlenet") << QStringLiteral("battlenet");
    QTest::newRow("ea") << QStringLiteral("ea") << QStringLiteral("ea");
    QTest::newRow("ubisoft") << QStringLiteral("ubisoft") << QStringLiteral("ubisoft");
    QTest::newRow("epic") << QStringLiteral("egs-launcher") << QStringLiteral("egs");
    QTest::newRow("gog") << QStringLiteral("gog-galaxy") << QStringLiteral("gog");
    QTest::newRow("amazon") << QStringLiteral("amazon-games") << QStringLiteral("amazon");
  }
  void umuStoresMatchUmuProtonfixes() {
    QFETCH(QString, id);
    QFETCH(QString, store);
    QCOMPARE(findStoreRecipe(id)->umuStore, store);
  }

  void onlyEpicIsAnMsi() {
    for (const StoreRecipe &recipe : storeRecipes()) {
      QCOMPARE(recipe.installerKind == InstallerKind::Msi, recipe.id == QStringLiteral("egs-launcher"));
    }
    QCOMPARE(installerCommand(InstallerKind::Msi, QStringLiteral("/dl/Epic.msi"), {QStringLiteral("/q")}),
             QStringList({QStringLiteral("msiexec"), QStringLiteral("/i"), QStringLiteral("/dl/Epic.msi"),
                          QStringLiteral("/q")}));
    QCOMPARE(installerCommand(InstallerKind::Exe, QStringLiteral("/dl/Setup.exe"), {}),
             QStringList{QStringLiteral("/dl/Setup.exe")});
  }

  void validationCatchesBrokenRecipes() {
    const StoreRecipe good = *findStoreRecipe(QStringLiteral("battlenet"));
    StoreRecipe r = good;
    r.installerUrl = QUrl(QStringLiteral("http://downloader.battle.net/x.exe"));
    QVERIFY(!validateStoreRecipe(r).isEmpty());
    r = good;
    r.installerUrl = QUrl(QStringLiteral("https://downloader.battle.net.evil.com/x.exe"));
    QVERIFY(!validateStoreRecipe(r).isEmpty());
    r = good;
    r.defaultPrefixDirName = QStringLiteral("../battlenet");
    QVERIFY(!validateStoreRecipe(r).isEmpty());
    r = good;
    r.launcherExecutableCandidates.clear();
    QVERIFY(!validateStoreRecipe(r).isEmpty());
    r = good;
    r.launcherExecutableCandidates = {QStringLiteral("Battle.net\\Launcher.exe")};
    QVERIFY(!validateStoreRecipe(r).isEmpty());
    r = good;
    r.installerKind = InstallerKind::Msi;
    QVERIFY(!validateStoreRecipe(r).isEmpty());
    r = good;
    r.umuStore = QStringLiteral("microsoft");
    QVERIFY(!validateStoreRecipe(r).isEmpty());
    r = good;
    r.installerFileName = QStringLiteral("../Setup.exe");
    QVERIFY(!validateStoreRecipe(r).isEmpty());
    r = good;
    r.installerArguments = {QStringLiteral("/S\n--evil")};
    QVERIFY(!validateStoreRecipe(r).isEmpty());
  }

  void windowsPathsMapIntoThePrefix_data() {
    QTest::addColumn<QString>("windows");
    QTest::addColumn<QString>("unix");
    QTest::newRow("program files x86")
        << QStringLiteral("C:\\Program Files (x86)\\Battle.net\\Battle.net Launcher.exe")
        << QStringLiteral("/games/bn/drive_c/Program Files (x86)/Battle.net/Battle.net Launcher.exe");
    QTest::newRow("proton user")
        << QStringLiteral("C:\\users\\steamuser\\AppData\\Local\\Amazon Games\\App\\Amazon Games.exe")
        << QStringLiteral("/games/bn/drive_c/users/steamuser/AppData/Local/Amazon Games/App/Amazon Games.exe");
    QTest::newRow("forward slashes") << QStringLiteral("c:/Games/x.exe")
                                     << QStringLiteral("/games/bn/drive_c/Games/x.exe");
    QTest::newRow("other drive is never guessed") << QStringLiteral("D:\\x.exe") << QString();
    QTest::newRow("doubled separators") << QStringLiteral("C:\\\\a\\\\b.exe")
                                        << QStringLiteral("/games/bn/drive_c/a/b.exe");
    QTest::newRow("relative") << QStringLiteral("Program Files\\x.exe") << QString();
    QTest::newRow("drive relative") << QStringLiteral("C:x.exe") << QString();
    QTest::newRow("unc") << QStringLiteral("\\\\server\\share\\x.exe") << QString();
    QTest::newRow("dotdot") << QStringLiteral("C:\\..\\..\\etc\\passwd") << QString();
    QTest::newRow("dot") << QStringLiteral("C:\\.\\x.exe") << QString();
    QTest::newRow("drive only") << QStringLiteral("C:\\") << QString();
  }
  void windowsPathsMapIntoThePrefix() {
    QFETCH(QString, windows);
    QFETCH(QString, unix);
    QCOMPARE(windowsPathToPrefixPath(QStringLiteral("/games/bn/"), windows), unix);
  }

  void versionOrder() {
    QVERIFY(versionGreater(QStringLiteral("13.1"), QStringLiteral("9.9")));
    QVERIFY(versionGreater(QStringLiteral("13.10"), QStringLiteral("13.9")));
    QVERIFY(!versionGreater(QStringLiteral("13.9"), QStringLiteral("13.10")));
    QVERIFY(versionGreater(QStringLiteral("1.0.1"), QStringLiteral("1.0")));
    QVERIFY(versionGreater(QStringLiteral("b"), QStringLiteral("A")));
    QVERIFY(!versionGreater(QStringLiteral("007"), QStringLiteral("7")));
    QVERIFY(!versionGreater(QStringLiteral("7"), QStringLiteral("007")));
    QVERIFY(versionGreater(QStringLiteral("99999999999999999999"), QStringLiteral("9")));
  }

  void relativePrefixIsRefused() {
    QVERIFY(windowsPathToPrefixPath(QStringLiteral("games/bn"), QStringLiteral("C:\\x.exe")).isEmpty());
    QVERIFY(windowsPathToPrefixPath(QString(), QStringLiteral("C:\\x.exe")).isEmpty());
  }

  void candidatesResolveInOrderWithWildcards() {
    QTemporaryDir dir(QDir::homePath() + QStringLiteral("/prefix-XXXXXX"));
    const QString prefix = dir.path();
    const StoreRecipe ea = *findStoreRecipe(QStringLiteral("ea"));
    QVERIFY(firstExistingCandidate(prefix, ea.launcherExecutableCandidates).isEmpty());

    // The EA installer's versioned layout is found through the '*' segment,
    // preferring the highest version.
    const QString base = prefix + QStringLiteral("/drive_c/Program Files/Electronic Arts/EA Desktop/");
    writeFile(base + QStringLiteral("13.100.0.1/EA Desktop/EALauncher.exe"), 8);
    writeFile(base + QStringLiteral("13.200.0.1/EA Desktop/EALauncher.exe"), 8);
    QCOMPARE(firstExistingCandidate(prefix, ea.launcherExecutableCandidates),
             base + QStringLiteral("13.200.0.1/EA Desktop/EALauncher.exe"));

    // Versions compare numerically: 13.x beats 9.x although "9" > "1".
    writeFile(base + QStringLiteral("9.900.0.1/EA Desktop/EALauncher.exe"), 8);
    QCOMPARE(firstExistingCandidate(prefix, ea.launcherExecutableCandidates),
             base + QStringLiteral("13.200.0.1/EA Desktop/EALauncher.exe"));

    // An exact, earlier candidate wins over the wildcard.
    writeFile(base + QStringLiteral("EA Desktop/EALauncher.exe"), 8);
    QCOMPARE(firstExistingCandidate(prefix, ea.launcherExecutableCandidates),
             base + QStringLiteral("EA Desktop/EALauncher.exe"));

    // A directory with the launcher's name is not a launcher.
    const StoreRecipe gog = *findStoreRecipe(QStringLiteral("gog-galaxy"));
    QDir().mkpath(prefix + QStringLiteral("/drive_c/Program Files (x86)/GOG Galaxy/GalaxyClient.exe"));
    QVERIFY(firstExistingCandidate(prefix, gog.launcherExecutableCandidates).isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_store_recipes)
#include "tst_store_recipes.moc"
