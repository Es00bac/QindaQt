// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include "executable_candidates.h"
#include "job_fakes.h"
#include "setup_file_install_job.h"

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

namespace {

constexpr qint64 kMiB = 1024 * 1024;

struct Fixture {
  QTemporaryDir dir{QDir::homePath() + QStringLiteral("/setup-XXXXXX")};
  FakeRunner runner;
  FakeProbe probe;
  RecordingPlanner planner;
  SetupFileInstallJob job{&runner, &probe, planner.planner()};
  std::optional<SetupFileInstallResult> result;
  SetupFileInstallRequest request;

  Fixture() {
    request.installerPath = dir.filePath(QStringLiteral("Downloads/setup_hollow_knight_1.5.exe"));
    writeFile(request.installerPath, 1024);
    request.title = QStringLiteral("Hollow Knight");
    request.prefixPath = dir.filePath(QStringLiteral("Games/hollow-knight"));
    request.protonBuildName = QStringLiteral("GE-Proton11-6-x86_64");
    request.protonBuildPath = makeProtonBuild(dir.filePath(QStringLiteral("compat")), request.protonBuildName);
    QObject::connect(&job, &SetupFileInstallJob::finished,
                     [this](const SetupFileInstallResult &r) { result = r; });
  }

  QString programFiles(const QString &relative) const {
    return request.prefixPath + QStringLiteral("/drive_c/Program Files/") + relative;
  }

  bool run() {
    job.start(request);
    return QTest::qWaitFor([this] { return result.has_value(); }, 5000);
  }
};

QStringList paths(const QVector<ExecutableCandidate> &candidates) {
  QStringList out;
  for (const ExecutableCandidate &candidate : candidates) {
    out.append(candidate.unixPath);
  }
  return out;
}

} // namespace

class tst_setup_file_install_job : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void newGameExecutablesAreOfferedBestFirst() {
    Fixture f;
    // Something already in the prefix before the installer ran is not new.
    writeFile(f.programFiles(QStringLiteral("Old Tool/old.exe")), 20 * kMiB);
    f.runner.sideEffect = [&f](const ProcessRunSpec &) {
      writeFile(f.programFiles(QStringLiteral("Hollow Knight/hollow_knight.exe")), 12 * kMiB);
      writeFile(f.programFiles(QStringLiteral("Hollow Knight/unins000.exe")), 3 * kMiB);
      writeFile(f.programFiles(QStringLiteral("Hollow Knight/UnityCrashHandler64.exe")), 2 * kMiB);
      writeFile(f.programFiles(QStringLiteral("Hollow Knight/redist/vc_redist.x64.exe")), 25 * kMiB);
      writeFile(f.programFiles(QStringLiteral("Hollow Knight/tools/modinstaller.exe")), 1 * kMiB);
      writeFile(f.programFiles(QStringLiteral("Hollow Knight/tools/level_editor.exe")), 40 * kMiB);
      writeFile(f.request.prefixPath +
                    QStringLiteral("/drive_c/users/steamuser/AppData/Local/Programs/HK Mod/hkmod.exe"),
                5 * kMiB);
      writeFile(f.programFiles(QStringLiteral("Hollow Knight/readme.txt")), 10);
    };
    QVERIFY(f.run());
    QVERIFY2(f.result->ok, qPrintable(f.result->message + QLatin1Char('\n') + f.job.detailsText()));
    const QStringList offered = paths(f.result->candidates);
    QCOMPARE(offered.first(), f.programFiles(QStringLiteral("Hollow Knight/hollow_knight.exe")));
    QVERIFY(offered.contains(f.programFiles(QStringLiteral("Hollow Knight/tools/level_editor.exe"))));
    QVERIFY(offered.contains(f.request.prefixPath +
                             QStringLiteral("/drive_c/users/steamuser/AppData/Local/Programs/HK Mod/hkmod.exe")));
    for (const QString &path : offered) {
      QVERIFY2(!path.contains(QStringLiteral("unins")) && !path.contains(QStringLiteral("Crash")) &&
                   !path.contains(QStringLiteral("redist")) && !path.contains(QStringLiteral("installer")) &&
                   !path.contains(QStringLiteral("old.exe")),
               qPrintable(path));
    }
    QCOMPARE(f.result->title, QStringLiteral("Hollow Knight"));
    QCOMPARE(f.result->prefixPath, f.request.prefixPath);
    QCOMPARE(f.result->protonBuildName, QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(f.result->umuId, QStringLiteral("umu-0"));
    QCOMPARE(f.result->umuStore, QStringLiteral("none"));
    // The user's installer is theirs: never deleted.
    QVERIFY(QFile::exists(f.request.installerPath));
    QCOMPARE(f.planner.requests.first().windowsCommand,
             QStringList{QFileInfo(f.request.installerPath).absoluteFilePath()});
  }

  void msiSetupRunsThroughMsiexec() {
    Fixture f;
    f.request.installerPath = f.dir.filePath(QStringLiteral("Downloads/Game.msi"));
    writeFile(f.request.installerPath, 64);
    f.runner.sideEffect = [&f](const ProcessRunSpec &) {
      writeFile(f.programFiles(QStringLiteral("Game/game.exe")), kMiB);
    };
    QVERIFY(f.run());
    QVERIFY(f.result->ok);
    QCOMPARE(f.planner.requests.first().windowsCommand.mid(0, 2),
             QStringList({QStringLiteral("msiexec"), QStringLiteral("/i")}));
    QCOMPARE(f.planner.requests.first().installerKind, InstallerKind::Msi);
  }

  void oddExitWithFilesStillOffersCandidates() {
    Fixture f;
    f.runner.result.exitCode = 1;
    f.runner.sideEffect = [&f](const ProcessRunSpec &) {
      writeFile(f.programFiles(QStringLiteral("Hollow Knight/hollow_knight.exe")), kMiB);
    };
    QVERIFY(f.run());
    QVERIFY(f.result->ok);
    QVERIFY(f.result->note.contains(QStringLiteral("code 1")));
  }

  void nothingInstalledIsAPlainFailure() {
    Fixture f;
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.contains(QStringLiteral("could not find the game")));
    QVERIFY(f.result->candidates.isEmpty());
    QCOMPARE(f.result->prefixPath, f.request.prefixPath);
  }

  void refusesFilesThatAreNotSetupPrograms() {
    Fixture f;
    f.request.installerPath = f.dir.filePath(QStringLiteral("Downloads/game.zip"));
    writeFile(f.request.installerPath, 64);
    f.job.start(f.request);
    QVERIFY(f.result.has_value());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.contains(QStringLiteral(".exe or .msi")));
    QVERIFY(f.runner.specs.isEmpty());
  }

  void preflightRunsBeforeThePrefixIsCreated() {
    Fixture f;
    f.probe.umu.clear();
    f.job.start(f.request);
    QVERIFY(f.result.has_value());
    QVERIFY(f.result->message.contains(QStringLiteral("umu")));
    QVERIFY(!QFileInfo::exists(f.request.prefixPath));
  }

  void cancelStopsTheInstaller() {
    Fixture f;
    f.runner.hang = true;
    f.job.start(f.request);
    QVERIFY(f.job.isRunning());
    f.job.cancel();
    QVERIFY(f.result->cancelled);
    QCOMPARE(f.runner.cancels, 1);
    QVERIFY(!f.job.isRunning());
  }

  void rankingPrefersTitleThenSize() {
    const ExecutableSnapshot before{{QStringLiteral("/p/drive_c/Program Files/X/keep.exe"), 10}};
    const ExecutableSnapshot after{
        {QStringLiteral("/p/drive_c/Program Files/X/keep.exe"), 10},
        {QStringLiteral("/p/drive_c/Program Files/Stardew Valley/Stardew Valley.exe"), 2 * kMiB},
        {QStringLiteral("/p/drive_c/Program Files/Stardew Valley/StardewModdingAPI.exe"), 30 * kMiB},
        {QStringLiteral("/p/drive_c/Program Files/Stardew Valley/big_helper.exe"), 90 * kMiB},
        {QStringLiteral("/p/drive_c/Program Files/Stardew Valley/Setup.exe"), 90 * kMiB},
    };
    const auto ranked = rankNewExecutables(before, after, QStringLiteral("Stardew Valley"));
    QCOMPARE(ranked.size(), 3);
    QCOMPARE(ranked.at(0).unixPath,
             QStringLiteral("/p/drive_c/Program Files/Stardew Valley/Stardew Valley.exe"));
    QVERIFY(ranked.at(0).score > ranked.at(1).score);
    QCOMPARE(rankNewExecutables(before, after, QStringLiteral("Stardew Valley"), 1).size(), 1);

    // A changed size counts as new (an installer that updated a file).
    ExecutableSnapshot updated = before;
    updated[QStringLiteral("/p/drive_c/Program Files/X/keep.exe")] = 11;
    QCOMPARE(rankNewExecutables(before, updated, QStringLiteral("X")).size(), 1);
  }

  void helperNamesAreRecognised_data() {
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("helper");
    QTest::newRow("inno uninstaller") << QStringLiteral("unins000.exe") << true;
    QTest::newRow("setup") << QStringLiteral("GameSetup.exe") << true;
    QTest::newRow("uninstall") << QStringLiteral("Uninstall Game.exe") << true;
    QTest::newRow("unity crash") << QStringLiteral("UnityCrashHandler64.exe") << true;
    QTest::newRow("crashpad") << QStringLiteral("crashpad_handler.exe") << true;
    QTest::newRow("vc redist") << QStringLiteral("VC_redist.x64.exe") << true;
    QTest::newRow("ue prereq") << QStringLiteral("UE4PrereqSetup_x64.exe") << true;
    QTest::newRow("game") << QStringLiteral("hollow_knight.exe") << false;
    QTest::newRow("launcher") << QStringLiteral("Launcher.exe") << false;
  }
  void helperNamesAreRecognised() {
    QFETCH(QString, name);
    QFETCH(bool, helper);
    QCOMPARE(isHelperExecutableName(name), helper);
  }

  void scanIsBoundedAndIgnoresSymlinks() {
    QTemporaryDir dir(QDir::homePath() + QStringLiteral("/scan-XXXXXX"));
    const QString prefix = dir.path();
    const QString pf = prefix + QStringLiteral("/drive_c/Program Files/");
    for (int i = 0; i < 30; ++i) {
      writeFile(pf + QStringLiteral("Many/f%1.exe").arg(i, 2, 10, QLatin1Char('0')), 1);
    }
    writeFile(dir.filePath(QStringLiteral("outside/secret.exe")), 1);
    QVERIFY(QFile::link(dir.filePath(QStringLiteral("outside")), pf + QStringLiteral("Linked")));
    writeFile(prefix + QStringLiteral("/drive_c/windows/system32/notepad.exe"), 1);

    const ExecutableSnapshot all = scanPrefixExecutables(prefix);
    QCOMPARE(all.size(), 30);
    for (auto it = all.cbegin(); it != all.cend(); ++it) {
      QVERIFY(!it.key().contains(QStringLiteral("secret")));
      QVERIFY(!it.key().contains(QStringLiteral("windows")));
    }
    ExecutableScanLimits limits;
    limits.maxEntries = 10;
    QVERIFY(scanPrefixExecutables(prefix, limits).size() < 10);
    QVERIFY(scanPrefixExecutables(QStringLiteral("relative/prefix")).isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_setup_file_install_job)
#include "tst_setup_file_install_job.moc"
