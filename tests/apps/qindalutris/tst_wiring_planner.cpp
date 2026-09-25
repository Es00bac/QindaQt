// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include "proton_fixture.h"
#include "title_record.h"
#include "umu_installer_planner.h"

using namespace QindaQt::QindaLutris;

// ADR-0275 sections 1 and 4c at the composition root: installers run under
// exactly the same umu rules as games, and "Run in its own screen" wraps a
// game -- never a store client -- in gamescope sized to the display.
class tst_wiring_planner : public QObject {
  Q_OBJECT

  QTemporaryDir m_dir;
  QString m_system;
  QString m_user;
  QString m_installer;
  QVector<ProtonBuild> m_builds;

  LaunchToolSet tools() const {
    LaunchToolSet out;
    out.umuRunBinary = QStringLiteral("/usr/bin/umu-run");
    out.protonBuilds = m_builds;
    return out;
  }

  InstallerPlanRequest request() const {
    InstallerPlanRequest out;
    out.protonBuildName = QStringLiteral("GE-Proton11-6-x86_64");
    out.protonBuildPath = m_system + QStringLiteral("/GE-Proton11-6-x86_64");
    out.prefixPath = m_dir.filePath(QStringLiteral("Games/battlenet"));
    out.umuId = QStringLiteral("umu-0");
    out.umuStore = QStringLiteral("battlenet");
    out.installerPath = m_installer;
    out.windowsCommand = {m_installer, QStringLiteral("--lang=enUS")};
    return out;
  }

private slots:
  void initTestCase() {
    QVERIFY(m_dir.isValid());
    m_system = m_dir.filePath(QStringLiteral("system"));
    m_user = m_dir.filePath(QStringLiteral("user"));
    ProtonFixture::makeBuild(m_system, QStringLiteral("GE-Proton11-6-x86_64"));
    ProtonFixture::makeBuild(m_user, QStringLiteral("GE-Proton11-6-x86_64"));
    m_installer = m_dir.filePath(QStringLiteral("Battle.net-Setup.exe"));
    ProtonFixture::writeFile(m_installer, "MZ");
    m_builds = discoverProtonBuilds({{m_system, ProtonBuild::Origin::System},
                                     {m_user, ProtonBuild::Origin::User}});
    QCOMPARE(m_builds.size(), 2);
  }

  void installerRunsThroughUmuWithTheExactBuild() {
    const InstallerPlan plan = planUmuInstallerRun(request(), tools());
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.spec.program, QStringLiteral("/usr/bin/umu-run"));
    QCOMPARE(plan.spec.arguments, (QStringList{m_installer, QStringLiteral("--lang=enUS")}));
    const auto &env = plan.spec.environment;
    QCOMPARE(env.value(QStringLiteral("PROTONPATH")),
             QDir(m_system + QStringLiteral("/GE-Proton11-6-x86_64")).canonicalPath());
    QCOMPARE(env.value(QStringLiteral("WINEPREFIX")), request().prefixPath);
    QCOMPARE(env.value(QStringLiteral("GAMEID")), QStringLiteral("umu-0"));
    QCOMPARE(env.value(QStringLiteral("STORE")), QStringLiteral("battlenet"));
    QCOMPARE(env.value(QStringLiteral("UMU_RUNTIME_UPDATE")), QStringLiteral("0"));
    QCOMPARE(plan.spec.unsetEnvironment, umuUnsetEnvironmentKeys());
    QCOMPARE(plan.spec.unsetEnvironmentPrefixes, umuUnsetEnvironmentPrefixes());
    QVERIFY(plan.spec.unsetEnvironmentPrefixes.contains(QStringLiteral("PYTHON")));
  }

  void theUserCopyIsUsedOnlyWhenItsPathIsNamed() {
    InstallerPlanRequest user = request();
    user.protonBuildPath = m_user + QStringLiteral("/GE-Proton11-6-x86_64");
    const InstallerPlan plan = planUmuInstallerRun(user, tools());
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.spec.environment.value(QStringLiteral("PROTONPATH")),
             QDir(user.protonBuildPath).canonicalPath());
  }

  void refusals_data() {
    QTest::addColumn<QString>("what");
    QTest::newRow("no umu") << QStringLiteral("umu");
    QTest::newRow("unknown build") << QStringLiteral("build");
    QTest::newRow("path of another build") << QStringLiteral("path");
    QTest::newRow("relative prefix") << QStringLiteral("prefix");
    QTest::newRow("nothing to run") << QStringLiteral("command");
    QTest::newRow("installer missing") << QStringLiteral("installer");
  }
  void refusals() {
    QFETCH(QString, what);
    InstallerPlanRequest req = request();
    LaunchToolSet set = tools();
    if (what == QLatin1String("umu")) {
      set.umuRunBinary.clear();
    } else if (what == QLatin1String("build")) {
      req.protonBuildName = QStringLiteral("GE-Proton11-7-x86_64");
    } else if (what == QLatin1String("path")) {
      req.protonBuildPath = m_dir.filePath(QStringLiteral("elsewhere/GE-Proton11-6-x86_64"));
    } else if (what == QLatin1String("prefix")) {
      req.prefixPath = QStringLiteral("Games/battlenet");
    } else if (what == QLatin1String("command")) {
      req.windowsCommand.clear();
    } else {
      req.installerPath = m_dir.filePath(QStringLiteral("gone.exe"));
    }
    const InstallerPlan plan = planUmuInstallerRun(req, set);
    QVERIFY(!plan.ok);
    QVERIFY(!plan.reason.isEmpty());
    QVERIFY(plan.spec.program.isEmpty());
  }

  void ownScreenWrapsTheGameInGamescope() {
    LaunchPlan plan;
    plan.ok = true;
    plan.program = QStringLiteral("/usr/bin/umu-run");
    plan.arguments = {QStringLiteral("/games/Wow.exe")};
    LaunchOptions options;
    options.ownScreen = true;
    options.gamemode = true;
    LaunchToolSet set = tools();
    set.gamescopeBinary = QStringLiteral("/usr/bin/gamescope");
    set.gamemodeRunBinary = QStringLiteral("/usr/bin/gamemoderun");
    DisplayTarget screen;
    screen.key = QStringLiteral("eDP-1");
    screen.widthPx = 1920;
    screen.heightPx = 1080;
    applyLaunchOptions(options, set, {screen}, false, &plan);
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/gamemoderun"));
    QCOMPARE(plan.arguments,
             (QStringList{QStringLiteral("/usr/bin/gamescope"), QStringLiteral("-f"),
                          QStringLiteral("-W"), QStringLiteral("1920"), QStringLiteral("-H"),
                          QStringLiteral("1080"), QStringLiteral("--"),
                          QStringLiteral("/usr/bin/umu-run"), QStringLiteral("/games/Wow.exe")}));
  }

  void ownScreenNeverWrapsTheSteamClient() {
    LaunchPlan plan;
    plan.ok = true;
    plan.program = QStringLiteral("/usr/bin/steam");
    plan.arguments = {QStringLiteral("steam://rungameid/10")};
    LaunchOptions options;
    options.ownScreen = true;
    LaunchToolSet set = tools();
    set.gamescopeBinary = QStringLiteral("/usr/bin/gamescope");
    applyLaunchOptions(options, set, {}, false, &plan);
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/steam"));
  }

  void ownScreenWithoutGamescopeStartsNormallyWithANote() {
    LaunchPlan plan;
    plan.ok = true;
    plan.program = QStringLiteral("/usr/bin/umu-run");
    LaunchOptions options;
    options.ownScreen = true;
    applyLaunchOptions(options, tools(), {}, false, &plan);
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/umu-run"));
    QVERIFY(plan.notes.join(QLatin1Char(' ')).contains(QStringLiteral("gamescope")));
  }
};

QTEST_GUILESS_MAIN(tst_wiring_planner)
#include "tst_wiring_planner.moc"
