// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "proton_fixture.h"
#include "store_launch.h"
#include "umu_launch.h"

using namespace QindaQt::QindaLutris;

// ADR-0275 section 8: Epic, GOG and Amazon games start through their store
// client with `--no-wine --wrapper <the umu plan's front>`. Plans only;
// nothing is spawned.
class tst_store_launch : public QObject {
  Q_OBJECT

  QTemporaryDir m_dir;
  QString m_prefix;
  QString m_gameDir;
  QString m_exe;
  QString m_buildPath;
  QVector<ProtonBuild> m_builds;

  LaunchToolSet tools() const {
    LaunchToolSet out;
    out.umuRunBinary = QStringLiteral("/usr/bin/umu-run");
    out.gamemodeRunBinary = QStringLiteral("/usr/bin/gamemoderun");
    out.protonBuilds = m_builds;
    out.storeClients.legendaryBinary = QStringLiteral("/usr/bin/legendary");
    out.storeClients.gogdlBinary = QStringLiteral("/usr/bin/gogdl");
    out.storeClients.nileBinary = QStringLiteral("/usr/bin/nile");
    out.storeClients.environment = {
        {QStringLiteral("LEGENDARY_CONFIG_PATH"), QStringLiteral("/cfg/stores/legendary")},
        {QStringLiteral("GOGDL_CONFIG_PATH"), QStringLiteral("/cfg/stores")},
        {QStringLiteral("NILE_CONFIG_PATH"), QStringLiteral("/cfg/stores")},
        // A reserved key here must never reach the plan.
        {QStringLiteral("PROTONPATH"), QStringLiteral("/evil")}};
    return out;
  }

  TitleRecord title(GameStore store, const QString &id) const {
    TitleRecord record;
    record.id = QStringLiteral("title/some-game");
    record.title = QStringLiteral("Some Game");
    record.kind = TitleKind::StoreGame;
    record.store = store;
    record.storeGameId = id;
    record.prefixPath = m_prefix;
    record.protonBuild = QStringLiteral("GE-Proton11-6-x86_64");
    record.protonBuildVersion = QStringLiteral("1756415527 GE-Proton11-6");
    record.umuStore = gameStoreId(store);
    record.executable = m_exe;
    record.installedAt = QStringLiteral("2026-09-25");
    return record;
  }

private Q_SLOTS:
  void initTestCase() {
    QVERIFY(m_dir.isValid());
    m_prefix = m_dir.filePath(QStringLiteral("Games/Prefixes/some-game"));
    QVERIFY(QDir().mkpath(m_prefix)); // Installs.installOwnedGame makes it
    m_gameDir = m_dir.filePath(QStringLiteral("Games/GOG/Some Game"));
    QVERIFY(QDir().mkpath(m_gameDir + QStringLiteral("/bin")));
    m_exe = m_gameDir + QStringLiteral("/bin/game.exe");
    QFile exe(m_exe);
    QVERIFY(exe.open(QIODevice::WriteOnly));
    exe.close();
    QFile info(m_gameDir + QStringLiteral("/goggame-1207658924.info"));
    QVERIFY(info.open(QIODevice::WriteOnly));
    info.write("{}");
    info.close();
    const QString system = m_dir.filePath(QStringLiteral("system"));
    m_buildPath = ProtonFixture::makeBuild(system, QStringLiteral("GE-Proton11-6-x86_64"),
                                           "1756415527 GE-Proton11-6");
    m_builds = discoverProtonBuilds({{system, ProtonBuild::Origin::System}});
    QCOMPARE(m_builds.size(), 1);
  }

  void epicWrapsUmuWithThePin() {
    const LaunchPlan plan =
        planTitleLaunch(title(GameStore::Egs, QStringLiteral("Fortnite_x")), {}, tools(), {});
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/legendary"));
    QCOMPARE(plan.arguments,
             QStringList({QStringLiteral("launch"), QStringLiteral("Fortnite_x"),
                          QStringLiteral("--no-wine"), QStringLiteral("--wrapper"),
                          QStringLiteral("/usr/bin/umu-run"),
                          QStringLiteral("--skip-version-check")}));
    QCOMPARE(plan.environment.value(QStringLiteral("PROTONPATH")), m_buildPath);
    QCOMPARE(plan.environment.value(QStringLiteral("WINEPREFIX")), m_prefix);
    QCOMPARE(plan.environment.value(QStringLiteral("STORE")), QStringLiteral("egs"));
    QCOMPARE(plan.environment.value(QStringLiteral("UMU_RUNTIME_UPDATE")), QStringLiteral("0"));
    QCOMPARE(plan.environment.value(QStringLiteral("LEGENDARY_CONFIG_PATH")),
             QStringLiteral("/cfg/stores/legendary"));
    QVERIFY(plan.unsetEnvironment.contains(QStringLiteral("HEROIC_GOGDL_WRAPPER_EXE")));
    QVERIFY(plan.unsetEnvironmentPrefixes.contains(QStringLiteral("PYTHON")));
  }

  void gogFindsItsInstallFolder() {
    const LaunchPlan plan =
        planTitleLaunch(title(GameStore::Gog, QStringLiteral("1207658924")), {}, tools(), {});
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/gogdl"));
    QCOMPARE(plan.arguments,
             QStringList({QStringLiteral("launch"), m_gameDir, QStringLiteral("1207658924"),
                          QStringLiteral("--platform"), QStringLiteral("windows"),
                          QStringLiteral("--no-wine"), QStringLiteral("--wrapper"),
                          QStringLiteral("/usr/bin/umu-run")}));
    QCOMPARE(plan.workingDirectory, m_gameDir);
  }

  void gogWithoutInfoFileIsRefused() {
    const LaunchPlan plan =
        planTitleLaunch(title(GameStore::Gog, QStringLiteral("42")), {}, tools(), {});
    QVERIFY(!plan.ok);
    QVERIFY(plan.reason.contains(QStringLiteral("GOG")));
  }

  void amazonAndGamemodeWrapper() {
    LaunchOptions options;
    options.gamemode = true;
    const LaunchPlan plan = planTitleLaunch(
        title(GameStore::Amazon, QStringLiteral("amzn1.adg.product.5a1b-c2")), options, tools(), {});
    QVERIFY2(plan.ok, qPrintable(plan.reason));
    QCOMPARE(plan.program, QStringLiteral("/usr/bin/nile"));
    QCOMPARE(plan.arguments,
             QStringList({QStringLiteral("launch"), QStringLiteral("amzn1.adg.product.5a1b-c2"),
                          QStringLiteral("--no-wine"), QStringLiteral("--wrapper"),
                          QStringLiteral("/usr/bin/gamemoderun /usr/bin/umu-run")}));
  }

  void refusals() {
    LaunchToolSet noClient = tools();
    noClient.storeClients.legendaryBinary.clear();
    LaunchPlan plan = planTitleLaunch(title(GameStore::Egs, QStringLiteral("App")), {}, noClient, {});
    QVERIFY(!plan.ok);
    QVERIFY(plan.reason.contains(QStringLiteral("games-util/legendary")));

    plan = planTitleLaunch(title(GameStore::Egs, QStringLiteral("--wrapper")), {}, tools(), {});
    QVERIFY(!plan.ok);

    TitleRecord unpinned = title(GameStore::Egs, QStringLiteral("App"));
    unpinned.protonBuildVersion = QStringLiteral("1 something else");
    plan = planTitleLaunch(unpinned, {}, tools(), {});
    QVERIFY(!plan.ok); // the umu plan's own pin refusal, unchanged
  }

  void deletedPrefixIsRefusedNotRemade() {
    TitleRecord record = title(GameStore::Egs, QStringLiteral("App"));
    record.prefixPath = m_dir.filePath(QStringLiteral("Games/Prefixes/deleted"));
    const LaunchPlan plan = planTitleLaunch(record, {}, tools(), {});
    QVERIFY(!plan.ok);
    QVERIFY(plan.reason.contains(QStringLiteral("prefix is missing")));
  }

  void titleArgumentsAreNotPassedToTheClient() {
    TitleRecord record = title(GameStore::Egs, QStringLiteral("App"));
    record.arguments = {QStringLiteral("--wrapper"), QStringLiteral("/tmp/x")};
    const LaunchPlan plan = planTitleLaunch(record, {}, tools(), {});
    QVERIFY(plan.ok);
    QVERIFY(!plan.arguments.contains(QStringLiteral("/tmp/x")));
    QVERIFY(!plan.notes.isEmpty());
  }

  void shlexQuoting() {
    QCOMPARE(joinForShlex({QStringLiteral("/usr/bin/umu-run")}), QStringLiteral("/usr/bin/umu-run"));
    QCOMPARE(joinForShlex({QStringLiteral("/opt/my games/run"), QStringLiteral("it's")}),
             QStringLiteral("'/opt/my games/run' 'it'\"'\"'s'"));
  }

  void storeGameIds() {
    QVERIFY(isSafeStoreGameId(QStringLiteral("Fortnite")));
    QVERIFY(isSafeStoreGameId(QStringLiteral("1207658924")));
    QVERIFY(isSafeStoreGameId(QStringLiteral("amzn1.adg.product.5a1b-c2")));
    QVERIFY(!isSafeStoreGameId(QStringLiteral("-x")));
    QVERIFY(!isSafeStoreGameId(QStringLiteral("a b")));
    QVERIFY(!isSafeStoreGameId(QString()));
    QVERIFY(!isSafeStoreGameId(QString(129, QLatin1Char('a'))));
  }
};

QTEST_GUILESS_MAIN(tst_store_launch)
#include "tst_store_launch.moc"
