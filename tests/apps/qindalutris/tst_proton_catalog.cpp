// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "proton_catalog.h"

using namespace QindaQt::QindaLutris;

// The ADR-0275 Proton catalog over synthetic roots only: fake build
// directories holding a shell stub named `proton`. Nothing here is a real
// Proton, and nothing is ever run.
class tst_proton_catalog : public QObject {
  Q_OBJECT

  static void writeFile(const QString &path, const QByteArray &bytes,
                        bool executable = false) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), qint64(bytes.size()));
    file.close();
    if (executable) {
      QVERIFY(QFile::setPermissions(
          path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
  }

  // A complete GE-style build the way ge-proton-bin installs it.
  static QString makeBuild(const QString &root, const QString &name,
                           const QByteArray &version = {},
                           const QByteArray &displayName = {}) {
    const QString dir = root + QLatin1Char('/') + name;
    writeFile(dir + QStringLiteral("/proton"), "#!/bin/sh\nexit 0\n", true);
    if (!version.isEmpty()) {
      writeFile(dir + QStringLiteral("/version"), version);
    }
    if (!displayName.isEmpty()) {
      writeFile(dir + QStringLiteral("/compatibilitytool.vdf"),
                "\"compatibilitytools\"\n{\n  \"compat_tools\"\n  {\n"
                "    \"" + name.toUtf8() + "\"\n    {\n"
                "      \"install_path\" \".\"\n"
                "      \"display_name\" \"" + displayName + "\"\n"
                "    }\n  }\n}\n");
    }
    return QDir(dir).canonicalPath();
  }

  static QStringList names(const QVector<ProtonBuild> &builds) {
    QStringList out;
    for (const ProtonBuild &build : builds) {
      out.append(build.name);
    }
    return out;
  }

  static ProtonBuild build(const QString &name, ProtonBuild::Origin origin) {
    ProtonBuild out;
    out.name = name;
    out.displayName = name;
    out.path = QStringLiteral("/roots/") + name;
    out.origin = origin;
    out.removable = origin == ProtonBuild::Origin::User;
    return out;
  }

private Q_SLOTS:
  void defaultRootsFollowTheAdrOrder() {
    const QVector<ProtonRoot> roots = defaultProtonRoots(
        QStringLiteral("/home/u"), QString(),
        {QStringLiteral("/home/u/.local/share/Steam"),
         QStringLiteral("/mnt/games/SteamLibrary")});
    const QVector<ProtonRoot> expected{
        {QStringLiteral("/usr/share/steam/compatibilitytools.d"),
         ProtonBuild::Origin::System},
        {QStringLiteral("/home/u/.local/share/Steam/compatibilitytools.d"),
         ProtonBuild::Origin::User},
        {QStringLiteral("/home/u/.steam/root/compatibilitytools.d"),
         ProtonBuild::Origin::User},
        {QStringLiteral("/home/u/.local/share/Steam/steamapps/common"),
         ProtonBuild::Origin::Steam},
        {QStringLiteral("/mnt/games/SteamLibrary/steamapps/common"),
         ProtonBuild::Origin::Steam},
    };
    QCOMPARE(roots, expected);
    // An explicit XDG_DATA_HOME replaces ~/.local/share.
    QCOMPARE(defaultProtonRoots(QStringLiteral("/home/u"),
                                QStringLiteral("/data"), {})
                 .at(1).path,
             QStringLiteral("/data/Steam/compatibilitytools.d"));
  }

  void discoversAcrossOriginsSystemFirst() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString system = tmp.filePath(QStringLiteral("system"));
    const QString user = tmp.filePath(QStringLiteral("user"));
    const QString steam = tmp.filePath(QStringLiteral("steam/steamapps/common"));
    const QString sysPath =
        makeBuild(system, QStringLiteral("GE-Proton11-6-x86_64"),
                  "1756415527 GE-Proton11-6\n", "GE-Proton11-6");
    makeBuild(system, QStringLiteral("GE-Proton10-25-x86_64"),
              "1740000000 GE-Proton10-25\n");
    makeBuild(user, QStringLiteral("GE-Proton11-7"));
    makeBuild(steam, QStringLiteral("Proton 9.0"), "1718395834 proton-9.0-2\n");
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
    QVERIFY(!pinned.removable); // Portage owns it
    QVERIFY(builds.at(2).removable);
    QCOMPARE(builds.at(2).origin, ProtonBuild::Origin::User);
    QVERIFY(!builds.at(3).removable);
    QCOMPARE(builds.at(3).origin, ProtonBuild::Origin::Steam);
  }

  void missingVersionAndVdfFallBackToTheName() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    makeBuild(tmp.path(), QStringLiteral("GE-Proton9-1"));
    const QVector<ProtonBuild> builds =
        discoverProtonBuilds({{tmp.path(), ProtonBuild::Origin::User}});
    QCOMPARE(builds.size(), 1);
    QCOMPARE(builds.at(0).displayName, QStringLiteral("GE-Proton9-1"));
    QVERIFY(builds.at(0).versionText.isEmpty());
  }

  void duplicateNamesKeepTheFirstRoot() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString system = tmp.filePath(QStringLiteral("system"));
    const QString user = tmp.filePath(QStringLiteral("user"));
    const QString systemPath = makeBuild(system, QStringLiteral("GE-Proton11-6"));
    makeBuild(user, QStringLiteral("GE-Proton11-6"));
    const QVector<ProtonBuild> builds = discoverProtonBuilds({
        {system, ProtonBuild::Origin::System},
        {user, ProtonBuild::Origin::User},
    });
    QCOMPARE(builds.size(), 1);
    QCOMPARE(builds.at(0).path, systemPath);
    QCOMPARE(builds.at(0).origin, ProtonBuild::Origin::System);
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

  void absentRootsAreEmptyNotErrors() {
    QVERIFY(discoverProtonBuilds({{QStringLiteral("/nonexistent/qindalutris"),
                                   ProtonBuild::Origin::System},
                                  {QString(), ProtonBuild::Origin::User}})
                .isEmpty());
  }

  void countsAreBounded() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    for (int i = 0; i < kMaxProtonBuilds + 10; ++i) {
      makeBuild(tmp.path(), QStringLiteral("GE-Proton1-%1").arg(i));
    }
    const QVector<ProtonBuild> builds =
        discoverProtonBuilds({{tmp.path(), ProtonBuild::Origin::User}});
    QCOMPARE(builds.size(), kMaxProtonBuilds);
  }

  void aliasRefusalTable() {
    const QVector<ProtonBuild> known{
        build(QStringLiteral("GE-Proton11-6"), ProtonBuild::Origin::System)};
    const QStringList floating{
        QString(), QStringLiteral("  "), QStringLiteral("GE-Proton"),
        QStringLiteral("ge-proton"), QStringLiteral("GE-Latest"),
        QStringLiteral("UMU-Latest"), QStringLiteral("umu-latest"),
        QStringLiteral("UMU-Proton"), QStringLiteral("latest"),
        QStringLiteral("LATEST"), QStringLiteral("GE-Proton11-7"),
        QStringLiteral("relative/path")};
    for (const QString &value : floating) {
      QVERIFY2(isFloatingProtonAlias(value, known), qPrintable(value));
    }
    QVERIFY(!isFloatingProtonAlias(QStringLiteral("GE-Proton11-6"), known));
    QVERIFY(!isFloatingProtonAlias(
        QStringLiteral("/usr/share/steam/compatibilitytools.d/GE-Proton11-6"),
        known));
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
  }

  void resolvePinnedBuildNeverSubstitutes() {
    const QVector<ProtonBuild> builds{
        build(QStringLiteral("GE-Proton11-7"), ProtonBuild::Origin::System),
        build(QStringLiteral("GE-Proton11-6-x86_64"), ProtonBuild::Origin::System),
    };
    const PinnedBuildResolution exact =
        resolvePinnedBuild(QStringLiteral("GE-Proton11-6-x86_64"), builds);
    QVERIFY(exact.ok());
    QCOMPARE(exact.build->name, QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(exact.failure, PinnedBuildResolution::Failure::None);

    // The WoW case: pinned 11-6 is gone, 11-7 is installed. Refuse; never
    // pick 11-7, never pick a prefix match.
    const QVector<ProtonBuild> without{builds.at(0)};
    const PinnedBuildResolution missing =
        resolvePinnedBuild(QStringLiteral("GE-Proton11-6"), without);
    QVERIFY(!missing.ok());
    QCOMPARE(missing.failure, PinnedBuildResolution::Failure::NotInstalled);
    QCOMPARE(missing.reason,
             QStringLiteral("GE-Proton11-6 is not installed. Reinstall it or "
                            "choose another Proton build for this game."));
    QVERIFY(!resolvePinnedBuild(QStringLiteral("GE-Proton11"), builds).ok());
    QVERIFY(!resolvePinnedBuild(QStringLiteral("ge-proton11-7"), builds).ok());

    const PinnedBuildResolution alias =
        resolvePinnedBuild(QStringLiteral("GE-Proton"), builds);
    QVERIFY(!alias.ok());
    QCOMPARE(alias.failure, PinnedBuildResolution::Failure::FloatingAlias);
    const PinnedBuildResolution empty = resolvePinnedBuild(QString(), builds);
    QCOMPARE(empty.failure, PinnedBuildResolution::Failure::NotChosen);
    QCOMPARE(empty.reason, QStringLiteral("Choose a Proton build for this game."));

    // Absolute pins match a build directory or (legacy) its proton script.
    QCOMPARE(resolvePinnedBuild(QStringLiteral("/roots/GE-Proton11-7"), builds)
                 .build->name,
             QStringLiteral("GE-Proton11-7"));
    QCOMPARE(resolvePinnedBuild(QStringLiteral("/roots/GE-Proton11-7/proton"),
                                builds).build->name,
             QStringLiteral("GE-Proton11-7"));
    const PinnedBuildResolution gonePath =
        resolvePinnedBuild(QStringLiteral("/old/GE-Proton9-1/proton"), builds);
    QCOMPARE(gonePath.failure, PinnedBuildResolution::Failure::NotInstalled);
    QVERIFY(gonePath.reason.startsWith(QStringLiteral("GE-Proton9-1 is not installed.")));
  }

  void chooseDefaultBuildOrder() {
    const ProtonBuild sys = build(QStringLiteral("GE-Proton11-6"),
                                  ProtonBuild::Origin::System);
    const ProtonBuild user = build(QStringLiteral("GE-Proton11-7"),
                                   ProtonBuild::Origin::User);
    const ProtonBuild valve = build(QStringLiteral("Proton 9.0"),
                                    ProtonBuild::Origin::Steam);
    QCOMPARE(chooseDefaultBuild({user, sys, valve}, QStringLiteral("Proton 9.0")),
             std::optional<ProtonBuild>(valve));
    // Preferred but not installed: first System build, not the first build.
    QCOMPARE(chooseDefaultBuild({user, sys, valve}, QStringLiteral("GE-Proton8-1")),
             std::optional<ProtonBuild>(sys));
    QCOMPARE(chooseDefaultBuild({user, valve}, QString()),
             std::optional<ProtonBuild>(user));
    QVERIFY(!chooseDefaultBuild({}, QStringLiteral("GE-Proton11-6")).has_value());
  }

  void newEntriesPinABuildName() {
    const ProtonBuild sys = build(QStringLiteral("GE-Proton11-6"),
                                  ProtonBuild::Origin::System);
    const ProtonBuild user = build(QStringLiteral("GE-Proton11-7"),
                                   ProtonBuild::Origin::User);
    // Empty request: the default build, by name.
    QCOMPARE(pinForNewEntry(QString(), {user, sys}, QString()),
             std::optional<QString>(QStringLiteral("GE-Proton11-6")));
    QCOMPARE(pinForNewEntry(QString(), {user, sys}, QStringLiteral("GE-Proton11-7")),
             std::optional<QString>(QStringLiteral("GE-Proton11-7")));
    // A path from protonChoices() is recorded as the build's name.
    QCOMPARE(pinForNewEntry(user.path, {user, sys}, QString()),
             std::optional<QString>(QStringLiteral("GE-Proton11-7")));
    // Aliases, unknown builds, and an empty catalog record nothing.
    QVERIFY(!pinForNewEntry(QStringLiteral("GE-Proton"), {user, sys}, QString()).has_value());
    QVERIFY(!pinForNewEntry(QStringLiteral("GE-Proton9-1"), {user, sys}, QString()).has_value());
    QVERIFY(!pinForNewEntry(QString(), {}, QString()).has_value());
  }
};

QTEST_GUILESS_MAIN(tst_proton_catalog)
#include "tst_proton_catalog.moc"
