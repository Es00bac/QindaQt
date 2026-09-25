// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "umu_launch.h"

using namespace QindaQt::QindaLutris;

// ADR-0275 section 1: every Windows title launches through umu-run with its
// exact pinned build. Plans only -- nothing is spawned; the prefix and the
// executable are empty fixture files in a QTemporaryDir.
class tst_umu_launch_plan : public QObject {
  Q_OBJECT

  QTemporaryDir m_dir;
  QString m_prefix;
  QString m_exe;

  static const QString &buildPath() {
    static const QString path = QStringLiteral(
        "/usr/share/steam/compatibilitytools.d/GE-Proton11-6-x86_64");
    return path;
  }

  static LaunchToolSet tools() {
    LaunchToolSet out;
    out.umuRunBinary = QStringLiteral("/usr/bin/umu-run");
    out.gamemodeRunBinary = QStringLiteral("/usr/bin/gamemoderun");
    out.mangohudBinary = QStringLiteral("/usr/bin/mangohud");
    out.wineBinary = QStringLiteral("/usr/bin/wine");
    ProtonBuild pinned;
    pinned.name = QStringLiteral("GE-Proton11-6-x86_64");
    pinned.displayName = QStringLiteral("GE-Proton11-6");
    pinned.path = buildPath();
    pinned.origin = ProtonBuild::Origin::System;
    ProtonBuild newer = pinned;
    newer.name = QStringLiteral("GE-Proton11-7");
    newer.path = QStringLiteral("/home/u/.local/share/Steam/compatibilitytools.d/GE-Proton11-7");
    newer.origin = ProtonBuild::Origin::User;
    newer.removable = true;
    out.protonBuilds = {newer, pinned}; // the newer one listed FIRST
    return out;
  }

  TitleRecord wow() const {
    TitleRecord record;
    record.id = QStringLiteral("title/world-of-warcraft");
    record.title = QStringLiteral("World of Warcraft");
    record.kind = TitleKind::StoreGame;
    record.store = GameStore::BattleNet;
    record.prefixPath = m_prefix;
    record.protonBuild = QStringLiteral("GE-Proton11-6-x86_64");
    record.umuId = QStringLiteral("umu-wow");
    record.umuStore = QStringLiteral("battlenet");
    record.executable = m_exe;
    record.arguments = {QStringLiteral("--exec=launch WoW"),
                        QStringLiteral("-d3d12")};
    record.environment = {QStringLiteral("DXVK_ASYNC=1")};
    record.installedAt = QStringLiteral("2026-09-25");
    return record;
  }

  static const QVector<DisplayTarget> &displays() {
    static const QVector<DisplayTarget> list{
        {QStringLiteral("DP-1"), QStringLiteral("DP-1"), 0},
        {QStringLiteral("HDMI-A-1"), QStringLiteral("HDMI-A-1"), 1},
    };
    return list;
  }

private Q_SLOTS:
  void initTestCase() {
    QVERIFY(m_dir.isValid());
    m_prefix = m_dir.filePath(QStringLiteral("Games/battlenet"));
    const QString exeDir = m_prefix + QStringLiteral("/drive_c/Program Files/WoW");
    QVERIFY(QDir().mkpath(exeDir));
    m_exe = exeDir + QStringLiteral("/Wow.exe");
    QFile file(m_exe);
    QVERIFY(file.open(QIODevice::WriteOnly));
  }

  void titlePlanIsExact() {
    const LaunchPlan plan = planTitleLaunch(wow(), {}, tools(), displays());
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/umu-run"));
    QCOMPARE(plan.arguments,
             QStringList({m_exe, QStringLiteral("--exec=launch WoW"),
                          QStringLiteral("-d3d12")}));
    QCOMPARE(plan.workingDirectory, QFileInfo(m_exe).absolutePath());
    const QHash<QString, QString> expected{
        {QStringLiteral("WINEPREFIX"), m_prefix},
        {QStringLiteral("PROTONPATH"), buildPath()},
        {QStringLiteral("GAMEID"), QStringLiteral("umu-wow")},
        {QStringLiteral("STORE"), QStringLiteral("battlenet")},
        {QStringLiteral("UMU_RUNTIME_UPDATE"), QStringLiteral("0")},
        {QStringLiteral("DXVK_ASYNC"), QStringLiteral("1")},
    };
    QCOMPARE(plan.environment, expected);
    QVERIFY(plan.notes.isEmpty());
    QVERIFY(QDir::isAbsolutePath(plan.environment.value(QStringLiteral("PROTONPATH"))));
  }

  void unknownGameDefaultsToUmuZeroAndNone() {
    TitleRecord record = wow();
    record.umuId.clear();
    record.umuStore.clear();
    const LaunchPlan plan = planTitleLaunch(record, {}, tools(), {});
    QVERIFY(plan.ok);
    QCOMPARE(plan.environment.value(QStringLiteral("GAMEID")), QStringLiteral("umu-0"));
    QCOMPARE(plan.environment.value(QStringLiteral("STORE")), QStringLiteral("none"));
    QCOMPARE(plan.environment.value(QStringLiteral("UMU_RUNTIME_UPDATE")),
             QStringLiteral("0"));
  }

  void umuMissingIsRefused() {
    LaunchToolSet noUmu = tools();
    noUmu.umuRunBinary.clear();
    const LaunchPlan plan = planTitleLaunch(wow(), {}, noUmu, {});
    QVERIFY(!plan.ok);
    QCOMPARE(plan.reason,
             QStringLiteral("umu is not installed. Install games-util/umu-launcher."));
    QVERIFY(plan.program.isEmpty());
  }

  void pinnedBuildMissingIsRefusedWithoutFallback() {
    LaunchToolSet onlyNewer = tools();
    onlyNewer.protonBuilds.removeLast(); // 11-6 uninstalled; 11-7 remains
    const LaunchPlan plan = planTitleLaunch(wow(), {}, onlyNewer, {});
    QVERIFY(!plan.ok);
    QCOMPARE(plan.reason,
             QStringLiteral("GE-Proton11-6-x86_64 is not installed. Reinstall it "
                            "or choose another Proton build for this game."));
    QVERIFY(plan.environment.isEmpty());
  }

  void floatingAliasIsRefused() {
    for (const QString &alias :
         {QStringLiteral("GE-Proton"), QStringLiteral("UMU-Latest"),
          QStringLiteral("GE-Latest"), QStringLiteral("latest")}) {
      UmuLaunchRequest request = umuRequestForTitle(wow());
      request.protonBuild = alias;
      const LaunchPlan plan = planUmuLaunch(request, {}, tools(), {});
      QVERIFY2(!plan.ok, qPrintable(alias));
      QVERIFY(plan.reason.contains(QStringLiteral("not a specific Proton build")));
    }
    UmuLaunchRequest request = umuRequestForTitle(wow());
    request.protonBuild.clear();
    QCOMPARE(planUmuLaunch(request, {}, tools(), {}).reason,
             QStringLiteral("Choose a Proton build for this game."));
  }

  void prefixMissingIsRefused() {
    TitleRecord record = wow();
    record.prefixPath = m_dir.filePath(QStringLiteral("Games/gone"));
    const LaunchPlan plan = planTitleLaunch(record, {}, tools(), {});
    QVERIFY(!plan.ok);
    QVERIFY(plan.reason.contains(QStringLiteral("Wine prefix is missing")));
    // A relative prefix is not a usable location at all.
    record.prefixPath = QStringLiteral("Games/battlenet");
    QVERIFY(!planTitleLaunch(record, {}, tools(), {}).ok);
  }

  void executableMissingIsRefused() {
    TitleRecord record = wow();
    record.executable = m_prefix + QStringLiteral("/drive_c/missing.exe");
    const LaunchPlan plan = planTitleLaunch(record, {}, tools(), {});
    QVERIFY(!plan.ok);
    QVERIFY(plan.reason.contains(QStringLiteral("program file is missing")));
  }

  void handAddedProtonEntryRunsThroughUmu() {
    Game game;
    game.id = QStringLiteral("wine/alpha-1");
    game.title = QStringLiteral("Alpha");
    game.source = GameSource::Wine;
    game.installPath = m_exe;
    game.winePrefix = m_dir.filePath(QStringLiteral("fresh-prefix")); // absent
    game.wineRunner = WineRunner::Proton;
    game.protonPath = QStringLiteral("GE-Proton11-6-x86_64");
    const LaunchPlan plan = planGameLaunch(game, {}, tools(), {}, nullptr);
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/umu-run"));
    QCOMPARE(plan.arguments, QStringList({m_exe}));
    QCOMPARE(plan.environment.value(QStringLiteral("PROTONPATH")), buildPath());
    QCOMPARE(plan.environment.value(QStringLiteral("GAMEID")), QStringLiteral("umu-0"));
    QCOMPARE(plan.environment.value(QStringLiteral("STORE")), QStringLiteral("none"));
    QCOMPARE(plan.environment.value(QStringLiteral("UMU_RUNTIME_UPDATE")),
             QStringLiteral("0"));

    // A legacy entry that stored the script path still resolves exactly.
    game.protonPath = buildPath() + QStringLiteral("/proton");
    QCOMPARE(planGameLaunch(game, {}, tools(), {}, nullptr)
                 .environment.value(QStringLiteral("PROTONPATH")),
             buildPath());

    // No pin: refused, never "any Proton".
    game.protonPath.clear();
    const LaunchPlan unpinned = planGameLaunch(game, {}, tools(), {}, nullptr);
    QVERIFY(!unpinned.ok);
    QCOMPARE(unpinned.reason, QStringLiteral("Choose a Proton build for this game."));

    // The Wine runner is untouched by ADR-0275.
    game.wineRunner = WineRunner::Wine;
    const LaunchPlan wine = planGameLaunch(game, {}, tools(), {}, nullptr);
    QVERIFY(wine.ok);
    QCOMPARE(wine.program, QStringLiteral("/usr/bin/wine"));
    QVERIFY(!wine.environment.contains(QStringLiteral("PROTONPATH")));
  }

  void sharedOptionsStillApply() {
    LaunchOptions options;
    options.gamemode = true;
    options.mangohud = true;
    options.targetDisplay = QStringLiteral("HDMI-A-1");
    options.extraEnvironment = {QStringLiteral("DXVK_HUD=fps"),
                                QStringLiteral("DXVK_ASYNC=0")};
    const LaunchPlan plan = planTitleLaunch(wow(), options, tools(), displays());
    QVERIFY(plan.ok);
    // gamemode wraps umu-run; the mangohud wrapper does not (MANGOHUD=1
    // reaches DXVK inside umu's container instead).
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/gamemoderun"));
    QCOMPARE(plan.arguments.first(), QStringLiteral("/usr/bin/umu-run"));
    QCOMPARE(plan.arguments.at(1), m_exe);
    QVERIFY(!plan.arguments.contains(QStringLiteral("/usr/bin/mangohud")));
    QCOMPARE(plan.environment.value(QStringLiteral("MANGOHUD")), QStringLiteral("1"));
    QCOMPARE(plan.environment.value(QStringLiteral("SDL_VIDEO_FULLSCREEN_DISPLAY")),
             QStringLiteral("1"));
    QCOMPARE(plan.environment.value(QStringLiteral("DXVK_HUD")), QStringLiteral("fps"));
    // The user's options refine the record's environment.
    QCOMPARE(plan.environment.value(QStringLiteral("DXVK_ASYNC")), QStringLiteral("0"));
    QCOMPARE(plan.environment.value(QStringLiteral("UMU_RUNTIME_UPDATE")),
             QStringLiteral("0"));
  }

  void userEnvironmentCannotFloatTheBuild() {
    LaunchOptions options;
    options.extraEnvironment = {QStringLiteral("PROTONPATH=GE-Proton"),
                                QStringLiteral("UMU_RUNTIME_UPDATE=1"),
                                QStringLiteral("WINEPREFIX=/tmp/other")};
    const LaunchPlan plan = planTitleLaunch(wow(), options, tools(), {});
    QVERIFY(plan.ok);
    QCOMPARE(plan.environment.value(QStringLiteral("PROTONPATH")), buildPath());
    QCOMPARE(plan.environment.value(QStringLiteral("UMU_RUNTIME_UPDATE")),
             QStringLiteral("0"));
    QCOMPARE(plan.environment.value(QStringLiteral("WINEPREFIX")), m_prefix);
    QCOMPARE(plan.notes.size(), 3);
    // A record smuggling a reserved key (the store refuses it; the planner
    // does not trust the store) is dropped the same way.
    UmuLaunchRequest request = umuRequestForTitle(wow());
    request.environment = {QStringLiteral("PROTONPATH=/elsewhere")};
    const LaunchPlan smuggled = planUmuLaunch(request, {}, tools(), {});
    QCOMPARE(smuggled.environment.value(QStringLiteral("PROTONPATH")), buildPath());
    QVERIFY(!smuggled.notes.isEmpty());
  }

  void umuDiscoveryPrefersTheSystemPackage() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const auto put = [](const QString &path, bool executable) {
      QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
      QFile file(path);
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write("#!/bin/sh\nexit 0\n");
      file.close();
      if (executable) {
        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner
                                                | QFile::ExeOwner));
      }
    };
    const QString system = tmp.filePath(QStringLiteral("usr/bin"));
    const QString path = tmp.filePath(QStringLiteral("path"));
    const QString user = tmp.filePath(QStringLiteral("home/.local/bin"));
    put(user + QStringLiteral("/umu-run"), true);
    QCOMPARE(discoverUmuRun({system, path, user}), user + QStringLiteral("/umu-run"));
    put(path + QStringLiteral("/umu-run"), false); // not executable: skipped
    QCOMPARE(discoverUmuRun({system, path, user}), user + QStringLiteral("/umu-run"));
    put(system + QStringLiteral("/umu-run"), true);
    QCOMPARE(discoverUmuRun({system, path, user}), system + QStringLiteral("/umu-run"));
    QVERIFY(discoverUmuRun({tmp.filePath(QStringLiteral("nowhere"))}).isEmpty());

    QCOMPARE(defaultUmuSearchPath(QStringLiteral("/home/u"),
                                  {QStringLiteral("/usr/local/bin"),
                                   QStringLiteral("/usr/bin"), QString()}),
             QStringList({QStringLiteral("/usr/bin"),
                          QStringLiteral("/usr/local/bin"),
                          QStringLiteral("/home/u/.local/bin")}));
  }
};

QTEST_GUILESS_MAIN(tst_umu_launch_plan)
#include "tst_umu_launch_plan.moc"
