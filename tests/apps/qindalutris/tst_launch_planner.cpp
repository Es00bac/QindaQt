// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "launch_planner.h"
#include "proton_fixture.h"

using namespace QindaQt::QindaLutris;

namespace {

Game steamGame() {
  Game game;
  game.id = QStringLiteral("steam/42");
  game.title = QStringLiteral("Steam Game");
  game.source = GameSource::Steam;
  game.appId = 42;
  return game;
}

Game lutrisGame() {
  Game game;
  game.id = QStringLiteral("lutris/7");
  game.title = QStringLiteral("Lutris Game");
  game.source = GameSource::Lutris;
  game.sourceRef = QStringLiteral("7");
  return game;
}

Game wineGame() {
  Game game;
  game.id = QStringLiteral("wine/alpha-1");
  game.title = QStringLiteral("Wine Game");
  game.source = GameSource::Wine;
  game.installPath = QStringLiteral("/games/alpha/game.exe");
  game.winePrefix = QStringLiteral("/games/alpha/prefix");
  game.wineRunner = WineRunner::Wine;
  return game;
}

LaunchToolSet fullTools() {
  LaunchToolSet tools;
  tools.steamBinary = QStringLiteral("/usr/bin/steam");
  tools.lutrisBinary = QStringLiteral("/usr/bin/lutris");
  tools.wineBinary = QStringLiteral("/usr/bin/wine");
  tools.gamemodeRunBinary = QStringLiteral("/usr/bin/gamemoderun");
  tools.mangohudBinary = QStringLiteral("/usr/bin/mangohud");
  tools.umuRunBinary = QStringLiteral("/usr/bin/umu-run");
  ProtonBuild build;
  build.name = QStringLiteral("GE-Proton11-6-x86_64");
  build.displayName = QStringLiteral("GE-Proton11-6");
  build.path = QStringLiteral(
      "/usr/share/steam/compatibilitytools.d/GE-Proton11-6-x86_64");
  build.origin = ProtonBuild::Origin::System;
  build.versionText = QStringLiteral("1756415527 GE-Proton11-6");
  tools.protonBuilds = {build}; // no files behind it: refusal rows only
  return tools;
}

const QVector<DisplayTarget> twoDisplays{
    {QStringLiteral("DP-1"), QStringLiteral("DP-1"), 0},
    {QStringLiteral("HDMI-A-1"), QStringLiteral("HDMI-A-1"), 1},
};

} // namespace

class tst_launch_planner : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void steamUrlThroughSteamBinary() {
    const LaunchPlan plan =
        planGameLaunch(steamGame(), {}, fullTools(), {}, nullptr);
    QVERIFY(plan.ok);
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/steam"));
    QCOMPARE(plan.arguments,
             QStringList({QStringLiteral("steam://rungameid/42")}));
  }

  void steamMissingIsAReasonNotACrash() {
    LaunchToolSet tools = fullTools();
    tools.steamBinary.clear();
    const LaunchPlan plan = planGameLaunch(steamGame(), {}, tools, {}, nullptr);
    QVERIFY(!plan.ok);
    QVERIFY(plan.reason.contains(QStringLiteral("Steam")));
  }

  void lutrisUrlThroughLutrisBinary() {
    const LaunchPlan plan =
        planGameLaunch(lutrisGame(), {}, fullTools(), {}, nullptr);
    QVERIFY(plan.ok);
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/lutris"));
    QCOMPARE(plan.arguments,
             QStringList({QStringLiteral("lutris:rungameid/7")}));
  }

  void winePlanCarriesPrefix() {
    const LaunchPlan plan =
        planGameLaunch(wineGame(), {}, fullTools(), {}, nullptr);
    QVERIFY(plan.ok);
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/wine"));
    QCOMPARE(plan.arguments,
             QStringList({QStringLiteral("/games/alpha/game.exe")}));
    QCOMPARE(plan.environment.value(QStringLiteral("WINEPREFIX")),
             QStringLiteral("/games/alpha/prefix"));
  }

  // ADR-0275 changed this row: a Proton entry used to run `<proton> run`
  // with STEAM_COMPAT_DATA_PATH and "any discovered Proton". It now runs
  // through umu-run with its own pinned build as PROTONPATH.
  void protonEntryLaunchesThroughUmuWithItsPin() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString exe = dir.filePath(QStringLiteral("game.exe"));
    QFile file(exe);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    const QString root = dir.filePath(QStringLiteral("compat"));
    const QString buildPath = ProtonFixture::makeBuild(
        root, QStringLiteral("GE-Proton11-6-x86_64"), "1756415527 GE-Proton11-6");
    LaunchToolSet tools = fullTools();
    tools.protonBuilds =
        discoverProtonBuilds({{root, ProtonBuild::Origin::System}});
    Game game = wineGame();
    game.installPath = exe;
    game.winePrefix = dir.filePath(QStringLiteral("prefix")); // not created
    game.protonPath = QStringLiteral("GE-Proton11-6-x86_64");
    game.protonVersion = QStringLiteral("1756415527 GE-Proton11-6");
    LaunchOptions options;
    options.runnerOverride = WineRunner::Proton;
    const LaunchPlan plan = planGameLaunch(game, options, tools, {}, nullptr);
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/umu-run"));
    QCOMPARE(plan.arguments, QStringList({exe}));
    QCOMPARE(plan.environment.value(QStringLiteral("PROTONPATH")), buildPath);
    QCOMPARE(plan.environment.value(QStringLiteral("WINEPREFIX")),
             game.winePrefix);
    QVERIFY(!plan.environment.contains(QStringLiteral("STEAM_COMPAT_DATA_PATH")));
  }

  void protonEntryWithoutAPinIsRefused() {
    Game game = wineGame();
    game.wineRunner = WineRunner::Proton;
    game.protonPath.clear();
    const LaunchPlan plan = planGameLaunch(game, {}, fullTools(), {}, nullptr);
    QVERIFY(!plan.ok);
    QCOMPARE(plan.reason, QStringLiteral("Choose a Proton build for this game."));
  }

  void installedTitleNeedsItsRecord() {
    Game game;
    game.id = QStringLiteral("title/alpha");
    game.source = GameSource::Installed;
    const LaunchPlan plan = planGameLaunch(game, {}, fullTools(), {}, nullptr);
    QVERIFY(!plan.ok);
    QVERIFY(!plan.reason.isEmpty());
  }

  void protonWithoutPrefixFails() {
    Game game = wineGame();
    game.winePrefix.clear();
    game.wineRunner = WineRunner::Proton;
    game.protonPath = QStringLiteral("GE-Proton11-6-x86_64");
    const LaunchPlan plan =
        planGameLaunch(game, {}, fullTools(), {}, nullptr);
    QVERIFY(!plan.ok);
  }

  void gamemodeWrapsAndMangohudSetsEnv() {
    LaunchOptions options;
    options.gamemode = true;
    options.mangohud = true;
    const LaunchPlan plan =
        planGameLaunch(wineGame(), options, fullTools(), {}, nullptr);
    QVERIFY(plan.ok);
    // gamemode wraps outermost, then the mangohud wrapper, then wine.
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/gamemoderun"));
    QCOMPARE(plan.arguments,
             QStringList({QStringLiteral("/usr/bin/mangohud"),
                          QStringLiteral("/usr/bin/wine"),
                          QStringLiteral("/games/alpha/game.exe")}));
    QCOMPARE(plan.environment.value(QStringLiteral("MANGOHUD")),
             QStringLiteral("1"));
  }

  void missingGamemodeDegradesToANote() {
    LaunchToolSet tools = fullTools();
    tools.gamemodeRunBinary.clear();
    LaunchOptions options;
    options.gamemode = true;
    const LaunchPlan plan =
        planGameLaunch(wineGame(), options, tools, {}, nullptr);
    QVERIFY(plan.ok); // still launches
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/wine"));
    QVERIFY(!plan.notes.isEmpty());
  }

  void displayPinBecomesSdlIndex() {
    LaunchOptions options;
    options.targetDisplay = QStringLiteral("HDMI-A-1");
    const LaunchPlan plan =
        planGameLaunch(wineGame(), options, fullTools(), twoDisplays, nullptr);
    QVERIFY(plan.ok);
    QCOMPARE(plan.environment.value(QStringLiteral("SDL_VIDEO_FULLSCREEN_DISPLAY")),
             QStringLiteral("1"));
  }

  void unpluggedDisplayDegradesToANote() {
    LaunchOptions options;
    options.targetDisplay = QStringLiteral("DP-9");
    const LaunchPlan plan =
        planGameLaunch(wineGame(), options, fullTools(), twoDisplays, nullptr);
    QVERIFY(plan.ok);
    QVERIFY(!plan.environment.contains(
        QStringLiteral("SDL_VIDEO_FULLSCREEN_DISPLAY")));
    QVERIFY(!plan.notes.isEmpty());
  }

  void extraEnvironmentIsValidatedAgain() {
    LaunchOptions options;
    options.extraEnvironment = {QStringLiteral("DXVK_HUD=compiler"),
                                QStringLiteral("NOT VALID")};
    const LaunchPlan plan =
        planGameLaunch(wineGame(), options, fullTools(), {}, nullptr);
    QVERIFY(plan.ok);
    QCOMPARE(plan.environment.value(QStringLiteral("DXVK_HUD")),
             QStringLiteral("compiler"));
    QVERIFY(!plan.environment.contains(QStringLiteral("NOT VALID")));
    QVERIFY(!plan.notes.isEmpty());
  }

  // The planner must never produce a shell: program is argv[0], and no
  // argument may carry metacharacter interpretation by construction.
  void noShellAnywhere() {
    LaunchOptions options;
    options.gamemode = true;
    options.mangohud = true;
    options.targetDisplay = QStringLiteral("DP-1");
    options.extraEnvironment = {QStringLiteral("A=B")};
    const LaunchPlan plan =
        planGameLaunch(wineGame(), options, fullTools(), twoDisplays, nullptr);
    QVERIFY(plan.ok);
    QVERIFY(!plan.program.contains(QLatin1Char(' ')));
    for (const QString &arg : plan.arguments) {
      QVERIFY(!arg.contains(QLatin1Char('\n')));
    }
  }

  void wineLoaderDiscoveryPrefersAPlainLoaderThenTheNewestVersioned() {
    // Searching only for "wine" reported "Wine is not installed" on a machine
    // with a complete wine-proton stack, because Gentoo installs only
    // versioned loaders. These rows pin the order that fixes it.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto put = [&dir](const QString &name) {
      const QString path = dir.filePath(name);
      QFile file(path);
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write("#!/bin/sh\nexit 0\n");
      file.close();
      QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner
                                              | QFile::ExeOwner));
    };

    // Nothing installed: the refusal downstream then means what it says.
    QVERIFY(discoverWineLoader({dir.path()}).isEmpty());

    // Versioned loaders only - the newest wins, compared numerically so that
    // 11.0.2 beats 9.0 rather than losing a string sort.
    put(QStringLiteral("wine64-proton-9.0"));
    put(QStringLiteral("wine64-proton-11.0.2"));
    put(QStringLiteral("wine64-vanilla-10.0"));
    QCOMPARE(QFileInfo(discoverWineLoader({dir.path()})).fileName(),
             QStringLiteral("wine64-proton-11.0.2"));

    // A plain loader is the user's own selection, so it outranks every
    // versioned one.
    put(QStringLiteral("wine"));
    QCOMPARE(QFileInfo(discoverWineLoader({dir.path()})).fileName(),
             QStringLiteral("wine"));

    // A non-executable file never qualifies.
    QTemporaryDir other;
    QVERIFY(other.isValid());
    QFile plain(other.filePath(QStringLiteral("wine")));
    QVERIFY(plain.open(QIODevice::WriteOnly));
    plain.write("not executable");
    plain.close();
    QVERIFY(discoverWineLoader({other.path()}).isEmpty());

    // Earlier directories win, the way PATH behaves.
    QCOMPARE(QFileInfo(discoverWineLoader({other.path(), dir.path()})).fileName(),
             QStringLiteral("wine"));
    QCOMPARE(discoverWineLoader({other.path(), dir.path()}),
             QFileInfo(dir.filePath(QStringLiteral("wine"))).absoluteFilePath());
  }
};

QTEST_GUILESS_MAIN(tst_launch_planner)
#include "tst_launch_planner.moc"
