// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "proton_catalog.h"
#include "proton_fixture.h"

using namespace QindaQt::QindaLutris;
using namespace ProtonFixture;

// ADR-0275 section 2 discovery over synthetic roots only. Nothing here is a
// real Proton, and nothing is ever run. Pin resolution is tst_proton_pin.
class tst_proton_catalog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void defaultRootsNameEveryScannedRoot() {
    const QVector<ProtonRoot> roots = defaultProtonRoots(
        QStringLiteral("/home/u"), QString(),
        {QStringLiteral("/home/u/.local/share/Steam"),
         QStringLiteral("/mnt/games/SteamLibrary")});
    const QString flatpak =
        QStringLiteral("/home/u/.var/app/com.valvesoftware.Steam");
    const QVector<ProtonRoot> expected{
        {QStringLiteral("/usr/share/steam/compatibilitytools.d"),
         ProtonBuild::Origin::System},
        {QStringLiteral("/home/u/.local/share/Steam/compatibilitytools.d"),
         ProtonBuild::Origin::User},
        {QStringLiteral("/home/u/.steam/root/compatibilitytools.d"),
         ProtonBuild::Origin::User},
        {flatpak + QStringLiteral("/.local/share/Steam/compatibilitytools.d"),
         ProtonBuild::Origin::User},
        {flatpak + QStringLiteral("/data/Steam/compatibilitytools.d"),
         ProtonBuild::Origin::User},
        {QStringLiteral("/home/u/.local/share/Steam/steamapps/common"),
         ProtonBuild::Origin::Steam},
        {QStringLiteral("/mnt/games/SteamLibrary/steamapps/common"),
         ProtonBuild::Origin::Steam},
        {flatpak + QStringLiteral("/.local/share/Steam/steamapps/common"),
         ProtonBuild::Origin::Steam},
        {flatpak + QStringLiteral("/data/Steam/steamapps/common"),
         ProtonBuild::Origin::Steam},
    };
    QCOMPARE(roots, expected);
    // An explicit XDG_DATA_HOME replaces ~/.local/share; a Flatpak library
    // passed in as a Steam root is not listed twice.
    const QVector<ProtonRoot> xdg = defaultProtonRoots(
        QStringLiteral("/home/u"), QStringLiteral("/data"),
        {flatpak + QStringLiteral("/data/Steam")});
    QCOMPARE(xdg.at(1).path, QStringLiteral("/data/Steam/compatibilitytools.d"));
    QCOMPARE(xdg.size(), 7);
  }

  void discoversAcrossOriginsSystemFirst() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString system = tmp.filePath(QStringLiteral("system"));
    const QString user = tmp.filePath(QStringLiteral("user"));
    const QString steam = tmp.filePath(QStringLiteral("steam/steamapps/common"));
    const QString sysPath =
        makeBuild(system, QStringLiteral("GE-Proton11-6-x86_64"),
                  "1756415527 GE-Proton11-6", "GE-Proton11-6");
    makeBuild(system, QStringLiteral("GE-Proton10-25-x86_64"));
    makeBuild(user, QStringLiteral("GE-Proton11-7"));
    makeBuild(steam, QStringLiteral("Proton 9.0"), "1718395834 proton-9.0-2");
    // A game in steamapps/common with a `proton` file is still not Proton.
    makeBuild(steam, QStringLiteral("SomeGame"));

    const QVector<ProtonBuild> builds = discoverProtonBuilds({
        {system, ProtonBuild::Origin::System},
        {user, ProtonBuild::Origin::User},
        {steam, ProtonBuild::Origin::Steam},
    });
    QCOMPARE(names(builds),
             QStringList({QStringLiteral("GE-Proton11-6-x86_64"),
                          QStringLiteral("GE-Proton10-25-x86_64"),
                          QStringLiteral("GE-Proton11-7"),
                          QStringLiteral("Proton 9.0")}));
    const ProtonBuild &pinned = builds.at(0);
    QCOMPARE(pinned.path, sysPath);
    QVERIFY(QDir::isAbsolutePath(pinned.path));
    QCOMPARE(pinned.displayName, QStringLiteral("GE-Proton11-6"));
    QCOMPARE(pinned.versionText, QStringLiteral("1756415527 GE-Proton11-6"));
    QCOMPARE(pinned.origin, ProtonBuild::Origin::System);
    QVERIFY(pinned.pinnable);
    QVERIFY(!pinned.removable); // Portage owns it
    QVERIFY(builds.at(2).removable);
    QVERIFY(!builds.at(3).removable);
    QVERIFY(builds.at(3).pinnable); // a stable Valve release is pinnable
    QCOMPARE(protonVersionLabel(builds.at(3).versionText),
             QStringLiteral("proton-9.0-2"));
  }

  void missingVersionFileIsListedButNotPinnable() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    makeBuild(tmp.path(), QStringLiteral("GE-Proton9-1"), QByteArray());
    const QVector<ProtonBuild> builds =
        discoverProtonBuilds({{tmp.path(), ProtonBuild::Origin::User}});
    QCOMPARE(builds.size(), 1);
    QCOMPARE(builds.at(0).displayName, QStringLiteral("GE-Proton9-1"));
    QVERIFY(builds.at(0).versionText.isEmpty());
    QVERIFY(!builds.at(0).pinnable);
    QCOMPARE(protonBuildStatusLabel(builds.at(0)),
             QStringLiteral("No version file — not pinnable"));
  }

  // Review 5c: a same-named build in a later root is KEPT with its own
  // origin and path, so an exact-path pin to it still resolves.
  void duplicateNamesAreAllKeptSystemFirst() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString system = tmp.filePath(QStringLiteral("system"));
    const QString user = tmp.filePath(QStringLiteral("user"));
    const QString systemPath = makeBuild(system, QStringLiteral("GE-Proton11-6"));
    const QString userPath = makeBuild(user, QStringLiteral("GE-Proton11-6"));
    const QVector<ProtonBuild> builds = discoverProtonBuilds({
        {user, ProtonBuild::Origin::User},
        {system, ProtonBuild::Origin::System},
    });
    QCOMPARE(builds.size(), 2);
    QCOMPARE(builds.at(0).path, systemPath);
    QCOMPARE(builds.at(0).origin, ProtonBuild::Origin::System);
    QCOMPARE(builds.at(1).path, userPath);
    QCOMPARE(builds.at(1).origin, ProtonBuild::Origin::User);
  }

  void theSameRootReachedTwiceCountsOnce() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString real = tmp.filePath(QStringLiteral("real"));
    makeBuild(real, QStringLiteral("GE-Proton11-6"));
    // ~/.steam/root is usually a link to ~/.local/share/Steam.
    QVERIFY(QFile::link(real, tmp.filePath(QStringLiteral("alias"))));
    const QVector<ProtonBuild> builds = discoverProtonBuilds({
        {real, ProtonBuild::Origin::User},
        {tmp.filePath(QStringLiteral("alias")), ProtonBuild::Origin::User},
    });
    QCOMPARE(builds.size(), 1);
  }

  void nonExecutableOrMissingProtonIsNotABuild() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.filePath(QStringLiteral("NotExecutable/proton")), "#!/bin/sh\n");
    QVERIFY(QDir().mkpath(tmp.filePath(QStringLiteral("Empty"))));
    QVERIFY(QDir().mkpath(tmp.filePath(QStringLiteral("DirProton/proton"))));
    QVERIFY(discoverProtonBuilds({{tmp.path(), ProtonBuild::Origin::User}})
                .isEmpty());
  }

  void symlinksAndAliasNamedDirectoriesAreSkipped() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString root = tmp.filePath(QStringLiteral("root"));
    makeBuild(root, QStringLiteral("GE-Proton11-7"));
    // A retargetable link is exactly the floating build ADR-0275 forbids.
    QVERIFY(QFile::link(root + QStringLiteral("/GE-Proton11-7"),
                        root + QStringLiteral("/Pinned-Looking")));
    // A directory literally named like a floating alias.
    makeBuild(root, QStringLiteral("GE-Proton"));
    // A real directory whose proton entry is a symlink.
    writeFile(tmp.filePath(QStringLiteral("elsewhere/proton")), "#!/bin/sh\n", true);
    QVERIFY(QDir().mkpath(root + QStringLiteral("/LinkedScript")));
    QVERIFY(QFile::link(tmp.filePath(QStringLiteral("elsewhere/proton")),
                        root + QStringLiteral("/LinkedScript/proton")));
    const QVector<ProtonBuild> builds =
        discoverProtonBuilds({{root, ProtonBuild::Origin::User}});
    QCOMPARE(names(builds), QStringList({QStringLiteral("GE-Proton11-7")}));
  }

  // The download jobs stage and retire builds in hidden directories inside
  // a compatibilitytools.d; a half-extracted build must never be listed.
  void hiddenDirectoriesAreSkippedInEveryRoot() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString user = tmp.filePath(QStringLiteral("user"));
    const QString system = tmp.filePath(QStringLiteral("system"));
    const QString steam = tmp.filePath(QStringLiteral("steam"));
    makeBuild(user, QStringLiteral(".qindalutris-staging-1234"));
    makeBuild(user + QStringLiteral("/.qindalutris-trash"),
              QStringLiteral("GE-Proton9-1"));
    makeBuild(user, QStringLiteral(".qindalutris-trash"));
    makeBuild(system, QStringLiteral(".hidden-build"));
    makeBuild(steam, QStringLiteral(".Proton 9.0"));
    makeBuild(user, QStringLiteral("GE-Proton11-6-x86_64"));
    const QVector<ProtonBuild> builds = discoverProtonBuilds({
        {system, ProtonBuild::Origin::System},
        {user, ProtonBuild::Origin::User},
        {steam, ProtonBuild::Origin::Steam},
    });
    QCOMPARE(names(builds), QStringList({QStringLiteral("GE-Proton11-6-x86_64")}));
  }

  // Review R3: games sorting before "Proton*" never crowd Proton out.
  void steamLibraryWithManyGamesStillYieldsProton() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString common = tmp.filePath(QStringLiteral("steamapps/common"));
    for (int i = 0; i < kMaxProtonDirsPerRoot + 2; ++i) {
      QVERIFY(QDir().mkpath(common + QStringLiteral("/A Game %1").arg(i, 3, 10,
                                                                     QLatin1Char('0'))));
    }
    makeBuild(common, QStringLiteral("Proton 9.0"), "1718395834 proton-9.0-2");
    const QVector<ProtonBuild> builds =
        discoverProtonBuilds({{common, ProtonBuild::Origin::Steam}});
    QCOMPARE(names(builds), QStringList({QStringLiteral("Proton 9.0")}));
  }

  // Review R1: Valve's rolling channels are listed, never pinnable.
  void steamRollingChannelsAreNotPinnable() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString common = tmp.filePath(QStringLiteral("steamapps/common"));
    makeBuild(common, QStringLiteral("Proton - Experimental"),
              "1758000000 experimental-bleeding-edge");
    makeBuild(common, QStringLiteral("Proton Hotfix"), "1758000001 proton-hotfix");
    makeBuild(common, QStringLiteral("Proton Next"), "1758000002 proton-next");
    makeBuild(common, QStringLiteral("Proton 9.0"), "1718395834 proton-9.0-2");
    const QVector<ProtonBuild> builds =
        discoverProtonBuilds({{common, ProtonBuild::Origin::Steam}});
    QCOMPARE(builds.size(), 4);
    for (const ProtonBuild &b : builds) {
      const bool rolling = b.name != QLatin1String("Proton 9.0");
      QCOMPARE(b.pinnable, !rolling);
      QCOMPARE(protonBuildStatusLabel(b),
               rolling ? QStringLiteral("Updated by Steam — not pinnable")
                       : QString());
    }
    QVERIFY(isRollingProtonChannel(QStringLiteral("proton-EXPERIMENTAL-x"),
                                   ProtonBuild::Origin::Steam));
    // Only Steam's own channels are rolling; a user build name is a name.
    QVERIFY(!isRollingProtonChannel(QStringLiteral("GE-Proton-Next-1"),
                                    ProtonBuild::Origin::User));
  }

  void absentRootsAreEmptyNotErrors() {
    QVERIFY(discoverProtonBuilds({{QStringLiteral("/nonexistent/qindalutris"),
                                   ProtonBuild::Origin::System},
                                  {QString(), ProtonBuild::Origin::User}})
                .isEmpty());
  }

  // The build cap applies after sorting: the newest-looking survive, and a
  // System build is never dropped for a User one.
  void countsAreBoundedAfterSorting() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString user = tmp.filePath(QStringLiteral("user"));
    const QString system = tmp.filePath(QStringLiteral("system"));
    for (int i = 0; i < kMaxProtonBuilds + 10; ++i) {
      makeBuild(user, QStringLiteral("GE-Proton1-%1").arg(i));
    }
    makeBuild(system, QStringLiteral("GE-Proton0-1"));
    const QVector<ProtonBuild> builds = discoverProtonBuilds({
        {user, ProtonBuild::Origin::User},
        {system, ProtonBuild::Origin::System},
    });
    QCOMPARE(builds.size(), kMaxProtonBuilds);
    QCOMPARE(builds.first().name, QStringLiteral("GE-Proton0-1"));
    QCOMPARE(builds.at(1).name,
             QStringLiteral("GE-Proton1-%1").arg(kMaxProtonBuilds + 9));
    QVERIFY(!names(builds).contains(QStringLiteral("GE-Proton1-0")));
  }

  void namesAndVersionLabels() {
    QVERIFY(isProtonAliasName(QStringLiteral("Umu-Proton")));
    QVERIFY(!isProtonAliasName(QStringLiteral("GE-Proton11-6")));
    QVERIFY(isValidProtonBuildName(QStringLiteral("GE-Proton11-6-x86_64")));
    QVERIFY(isValidProtonBuildName(QStringLiteral("Proton 9.0")));
    QVERIFY(!isValidProtonBuildName(QStringLiteral("GE-Latest")));
    QVERIFY(!isValidProtonBuildName(QStringLiteral("a/b")));
    QVERIFY(!isValidProtonBuildName(QStringLiteral("..")));
    QVERIFY(!isValidProtonBuildName(QStringLiteral("bad\nname")));
    QVERIFY(!isValidProtonBuildName(QString(kMaxProtonBuildNameChars + 1,
                                            QLatin1Char('x'))));
    QCOMPARE(protonVersionLabel(QStringLiteral("1756415527 GE-Proton11-6")),
             QStringLiteral("GE-Proton11-6"));
    QCOMPARE(protonVersionLabel(QStringLiteral("proton-9.0-2")),
             QStringLiteral("proton-9.0-2"));
    QCOMPARE(protonVersionLabel(QString()), QStringLiteral("unknown"));
  }

  // Review R6: the plan-time re-check sees a build removed after discovery.
  void stillPresentRechecksTheScript() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    makeBuild(tmp.path(), QStringLiteral("GE-Proton11-6"));
    const QVector<ProtonBuild> builds =
        discoverProtonBuilds({{tmp.path(), ProtonBuild::Origin::System}});
    QCOMPARE(builds.size(), 1);
    QVERIFY(protonBuildStillPresent(builds.first()));
    QVERIFY(QFile::remove(builds.first().path + QStringLiteral("/proton")));
    QVERIFY(QFile::link(tmp.filePath(QStringLiteral("x")),
                        builds.first().path + QStringLiteral("/proton")));
    QVERIFY(!protonBuildStillPresent(builds.first()));
    QVERIFY(QDir(builds.first().path).removeRecursively());
    QVERIFY(!protonBuildStillPresent(builds.first()));
  }
};

QTEST_GUILESS_MAIN(tst_proton_catalog)
#include "tst_proton_catalog.moc"
