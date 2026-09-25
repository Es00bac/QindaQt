// SPDX-License-Identifier: GPL-3.0-or-later
#include <QProcess>
#include <QSignalSpy>
#include <QTest>

#include "library_controller.h"
#include "running_games.h"
#include "scoped_game_launcher.h"

#include <unistd.h>

using namespace QindaQt::QindaLutris;

// ADR-0275 section 4b: games run in their own systemd user scope and Force
// quit stops the whole scope. The end-to-end row uses the REAL user manager
// (a throwaway `sleep` in a qindalutris-game-* scope) and skips cleanly when
// there is none; everything it starts is gone when it finishes.
class tst_running_games : public QObject {
  Q_OBJECT

  static QString isActive(const UserScopeTools &tools, const QString &unit) {
    QProcess process;
    process.start(tools.systemctl, {QStringLiteral("--user"), QStringLiteral("is-active"),
                                    unit + QStringLiteral(".scope")});
    process.waitForFinished(5000);
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  }

private slots:
  void activeUnitsReadsOneStatePerUnit() {
    const QStringList units{QStringLiteral("a.scope"), QStringLiteral("b.scope"),
                            QStringLiteral("c.scope")};
    QCOMPARE(RunningGames::activeUnits(units, "active\ninactive\nactivating\n"),
             (QStringList{QStringLiteral("a.scope"), QStringLiteral("c.scope")}));
    QCOMPARE(RunningGames::activeUnits(units, "active\n"), QStringList{QStringLiteral("a.scope")});
    QVERIFY(RunningGames::activeUnits(units, "").isEmpty());
  }

  void storeClientsAreNeverScoped() {
    LaunchPlan steam;
    steam.program = QStringLiteral("/usr/bin/steam");
    QVERIFY(!ScopedGameLauncher::shouldScope(steam));
    LaunchPlan lutris;
    lutris.program = QStringLiteral("lutris");
    QVERIFY(!ScopedGameLauncher::shouldScope(lutris));
    LaunchPlan umu;
    umu.program = QStringLiteral("/usr/bin/umu-run");
    QVERIFY(ScopedGameLauncher::shouldScope(umu));
  }

  void withoutAUserManagerLaunchesUnscoped() {
    ScopedGameLauncher launcher(UserScopeTools{});
    LaunchPlan plan;
    plan.ok = true;
    plan.program = QStringLiteral("/bin/true");
    QVERIFY(launcher.launch(plan).ok);
    QVERIFY(launcher.lastScopeUnit().isEmpty());
  }

  void forceQuitStopsTheWholeScope() {
    // ctest isolates XDG_RUNTIME_DIR and the session bus; reach the real
    // user manager for this one row, and restore both afterwards.
    const QByteArray savedRuntime = qgetenv("XDG_RUNTIME_DIR");
    const QByteArray savedBus = qgetenv("DBUS_SESSION_BUS_ADDRESS");
    qputenv("XDG_RUNTIME_DIR", QByteArray("/run/user/") + QByteArray::number(::getuid()));
    qunsetenv("DBUS_SESSION_BUS_ADDRESS");
    const auto restore = qScopeGuard([&] {
      qputenv("XDG_RUNTIME_DIR", savedRuntime);
      qputenv("DBUS_SESSION_BUS_ADDRESS", savedBus);
    });
    const UserScopeTools tools = detectUserScopeTools();
    if (!tools.available()) {
      QSKIP("no systemd user manager reachable; the scope path cannot be exercised here");
    }
    ScopedGameLauncher launcher(tools);
    LaunchPlan plan;
    plan.ok = true;
    plan.program = QStringLiteral("/bin/sh");
    // A game-like tree: a child that detaches into its own session and one
    // that ignores SIGTERM, so only the scope's KILL ends everything.
    plan.arguments = {QStringLiteral("-c"),
                      QStringLiteral("setsid sleep 61.5 & (trap '' TERM; sleep 61.7) & wait")};
    QVERIFY(launcher.launch(plan).ok);
    const QString unit = launcher.lastScopeUnit();
    QVERIFY(unit.startsWith(QStringLiteral("qindalutris-game-")));
    const auto cleanup = qScopeGuard([&] {
      QProcess::execute(tools.systemctl, {QStringLiteral("--user"), QStringLiteral("stop"),
                                          unit + QStringLiteral(".scope")});
    });
    QTRY_COMPARE_WITH_TIMEOUT(isActive(tools, unit), QStringLiteral("active"), 10000);

    LibraryController library;
    RunningGames running(&library, &launcher);
    Q_EMIT library.gameLaunched(QStringLiteral("title/test-game"));
    QVERIFY(running.isRunning(QStringLiteral("title/test-game")));
    running.forceQuit(QStringLiteral("title/test-game"));
    QTRY_VERIFY_WITH_TIMEOUT(!running.isRunning(QStringLiteral("title/test-game")), 20000);
    QCOMPARE(running.message(), QStringLiteral("The game was stopped."));
    QVERIFY(isActive(tools, unit) != QLatin1String("active"));
    QProcess pgrep;
    pgrep.start(QStringLiteral("pgrep"), {QStringLiteral("-f"), QStringLiteral("sleep 61\\.[57]")});
    pgrep.waitForFinished(5000);
    QCOMPARE(pgrep.readAllStandardOutput().trimmed(), QByteArray());
  }
};

QTEST_MAIN(tst_running_games)
#include "tst_running_games.moc"
