// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "launch_planner.h"

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
  tools.protons = {{QStringLiteral("Proton 9.0"),
                    QStringLiteral("/steam/common/Proton 9.0/proton")}};
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

  void protonPlanUsesCompatPath() {
    LaunchOptions options;
    options.runnerOverride = WineRunner::Proton;
    const LaunchPlan plan =
        planGameLaunch(wineGame(), options, fullTools(), {}, nullptr);
    QVERIFY(plan.ok);
    QCOMPARE(plan.program, QStringLiteral("/steam/common/Proton 9.0/proton"));
    QCOMPARE(plan.arguments,
             QStringList({QStringLiteral("run"),
                          QStringLiteral("/games/alpha/game.exe")}));
    QCOMPARE(plan.environment.value(QStringLiteral("STEAM_COMPAT_DATA_PATH")),
             QStringLiteral("/games/alpha/prefix"));
  }

  void protonWithoutPrefixFails() {
    Game game = wineGame();
    game.winePrefix.clear();
    game.wineRunner = WineRunner::Proton;
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
};

QTEST_GUILESS_MAIN(tst_launch_planner)
#include "tst_launch_planner.moc"
