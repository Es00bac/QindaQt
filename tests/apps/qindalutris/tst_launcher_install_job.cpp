// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include "job_fakes.h"
#include "launcher_install_job.h"
#include "prefix_paths.h"

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

namespace {

struct Fixture {
  QTemporaryDir dir{QDir::homePath() + QStringLiteral("/launcher-XXXXXX")};
  FakeDownloader downloader;
  FakeRunner runner;
  FakeProbe probe;
  RecordingPlanner planner;
  LauncherInstallJob job{&downloader, &runner, &probe, planner.planner()};
  std::optional<LauncherInstallResult> result;
  LauncherInstallRequest request;

  explicit Fixture(const QString &recipeId = QStringLiteral("battlenet")) {
    request.recipe = *findStoreRecipe(recipeId);
    request.prefixPath = dir.filePath(QStringLiteral("Games/") + request.recipe.defaultPrefixDirName);
    request.protonBuildName = QStringLiteral("GE-Proton11-6-x86_64");
    request.protonBuildPath = makeProtonBuild(dir.filePath(QStringLiteral("compat")), request.protonBuildName);
    request.downloadDirectory = dir.filePath(QStringLiteral("cache/installers"));
    downloader.serve(request.recipe.installerUrl, QByteArray("MZ fake installer"));
    QObject::connect(&job, &LauncherInstallJob::finished,
                     [this](const LauncherInstallResult &r) { result = r; });
  }

  // The fake installer "installs" the first launcher candidate.
  void installerCreatesLauncher() {
    const QString launcher =
        windowsPathToPrefixPath(request.prefixPath, request.recipe.launcherExecutableCandidates.first());
    runner.sideEffect = [launcher](const ProcessRunSpec &) { writeFile(launcher, 4096); };
  }

  bool run() {
    job.start(request);
    return QTest::qWaitFor([this] { return result.has_value(); }, 5000);
  }

  QString installerFile() const {
    return request.downloadDirectory + QLatin1Char('/') + request.recipe.installerFileName;
  }
};

} // namespace

class tst_launcher_install_job : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void installsAndReportsTheLauncher() {
    Fixture f;
    f.installerCreatesLauncher();
    QVERIFY(f.run());
    QVERIFY2(f.result->ok, qPrintable(f.result->message + QLatin1Char('\n') + f.job.detailsText()));
    QCOMPARE(f.result->message, QStringLiteral("Battle.net is installed."));
    QVERIFY(f.result->note.isEmpty());
    const InstalledLauncher &l = f.result->launcher;
    QCOMPARE(l.recipeId, QStringLiteral("battlenet"));
    QCOMPARE(l.title, QStringLiteral("Battle.net"));
    QCOMPARE(l.prefixPath, f.request.prefixPath);
    QCOMPARE(l.executableUnixPath,
             f.request.prefixPath + QStringLiteral("/drive_c/Program Files (x86)/Battle.net/Battle.net Launcher.exe"));
    QCOMPARE(l.protonBuildName, QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(l.umuId, QStringLiteral("umu-0"));
    QCOMPARE(l.umuStore, QStringLiteral("battlenet"));

    // The planner saw every fact it needs, and the runner got its plan.
    QCOMPARE(f.planner.requests.size(), 1);
    const InstallerPlanRequest &plan = f.planner.requests.first();
    QCOMPARE(plan.umuRunBinary, QStringLiteral("/usr/bin/umu-run"));
    QCOMPARE(plan.protonBuildPath, f.request.protonBuildPath);
    QCOMPARE(plan.prefixPath, f.request.prefixPath);
    QCOMPARE(plan.windowsCommand, QStringList{f.installerFile()});
    QCOMPARE(f.runner.specs.size(), 1);
    QCOMPARE(f.runner.specs.first().timeoutMs, kInstallerTimeoutMs);
    QCOMPARE(f.downloader.requested, QList<QUrl>{f.request.recipe.installerUrl});
    // The downloaded installer is not kept.
    QVERIFY(!QFile::exists(f.installerFile()));
    QVERIFY(f.job.detailsText().contains(QStringLiteral("Launcher found")));
  }

  void msiRunsThroughMsiexec() {
    Fixture f(QStringLiteral("egs-launcher"));
    f.installerCreatesLauncher();
    QVERIFY(f.run());
    QVERIFY(f.result->ok);
    QCOMPARE(f.planner.requests.first().windowsCommand,
             QStringList({QStringLiteral("msiexec"), QStringLiteral("/i"), f.installerFile(),
                          QStringLiteral("/q")}));
    QCOMPARE(f.result->launcher.umuStore, QStringLiteral("egs"));
  }

  void oddExitCodeWithLauncherPresentSucceedsWithANote() {
    Fixture f(QStringLiteral("ea"));
    f.installerCreatesLauncher();
    f.runner.result.exitCode = 768;
    QVERIFY(f.run());
    QVERIFY(f.result->ok);
    QVERIFY(f.result->note.contains(QStringLiteral("768")));
    QVERIFY(f.result->note.contains(QStringLiteral("EA app is installed")));
  }

  void nonzeroExitWithoutLauncherIsOnePlainSentence() {
    Fixture f;
    f.runner.result.exitCode = 3;
    f.runner.result.standardError = "wine: could not load kernel32.dll\n";
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QCOMPARE(f.result->message,
             QStringLiteral("The Battle.net installer stopped with an error, and Battle.net was not installed."));
    const QString details = f.job.detailsText();
    QVERIFY(details.contains(QStringLiteral("exit=3")));
    QVERIFY(details.contains(QStringLiteral("kernel32")));
    QVERIFY(!QFile::exists(f.installerFile()));
  }

  void cleanExitWithoutLauncherFails() {
    Fixture f;
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.contains(QStringLiteral("was not found afterwards")));
  }

  void installerThatCannotStartFails() {
    Fixture f;
    f.runner.result = {};
    f.runner.result.error = QStringLiteral("execve failed");
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QCOMPARE(f.result->message, QStringLiteral("The Battle.net installer could not be started."));
  }

  void cancelDuringDownload() {
    Fixture f;
    f.downloader.script(f.request.recipe.installerUrl, FakeDownloader::Mode::Hang);
    f.job.start(f.request);
    QCOMPARE(f.job.stage(), LauncherInstallJob::Stage::Downloading);
    f.job.cancel();
    QVERIFY(f.result.has_value());
    QVERIFY(f.result->cancelled);
    QVERIFY(!f.result->ok);
    QCOMPARE(f.downloader.cancels, 1);
    QVERIFY(f.runner.specs.isEmpty());
    QVERIFY(!f.job.isRunning());
    QVERIFY(!QFile::exists(f.installerFile()));
  }

  void cancelDuringInstallStopsTheInstaller() {
    Fixture f;
    f.runner.hang = true;
    f.job.start(f.request);
    QVERIFY(QTest::qWaitFor([&f] { return f.job.stage() == LauncherInstallJob::Stage::Installing; }, 5000));
    f.job.cancel();
    QVERIFY(f.result->cancelled);
    QCOMPARE(f.runner.cancels, 1);
  }

  void downloadFailureIsPlain() {
    Fixture f;
    f.downloader.script(f.request.recipe.installerUrl, FakeDownloader::Mode::BadRedirect);
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QVERIFY(f.result->message.startsWith(QStringLiteral("The Battle.net installer could not be downloaded.")));
    QVERIFY(f.job.detailsText().contains(QStringLiteral("downloads.evil.example")));
    QVERIFY(f.runner.specs.isEmpty());
  }

  void preflightFailures_data() {
    QTest::addColumn<QString>("broken");
    QTest::addColumn<QString>("expected");
    QTest::newRow("disk full") << QStringLiteral("disk") << QStringLiteral("not enough free disk space");
    QTest::newRow("umu missing") << QStringLiteral("umu") << QStringLiteral("games-util/umu-launcher");
    QTest::newRow("no vulkan") << QStringLiteral("vulkan") << QStringLiteral("Vulkan");
    QTest::newRow("no proton build") << QStringLiteral("proton") << QStringLiteral("No Proton build is chosen");
    QTest::newRow("damaged proton build") << QStringLiteral("damaged") << QStringLiteral("missing or damaged");
  }
  void preflightFailures() {
    QFETCH(QString, broken);
    QFETCH(QString, expected);
    Fixture f;
    if (broken == QLatin1String("disk")) {
      f.probe.freeBytes = qint64(100) * 1024 * 1024;
    } else if (broken == QLatin1String("umu")) {
      f.probe.umu.clear();
    } else if (broken == QLatin1String("vulkan")) {
      f.probe.vulkan = false;
    } else if (broken == QLatin1String("proton")) {
      f.request.protonBuildPath.clear();
    } else {
      QFile::remove(f.request.protonBuildPath + QStringLiteral("/proton"));
    }
    f.job.start(f.request);
    QVERIFY(f.result.has_value()); // preflight answers before any download
    QVERIFY(!f.result->ok);
    QVERIFY2(f.result->message.contains(expected), qPrintable(f.result->message));
    QVERIFY(f.downloader.requested.isEmpty());
    QVERIFY(f.runner.specs.isEmpty());
    QVERIFY(!f.result->message.contains(QLatin1Char('\n')));
  }

  void existingLauncherIsAdoptedWithoutDownloading() {
    Fixture f;
    writeFile(windowsPathToPrefixPath(f.request.prefixPath,
                                      f.request.recipe.launcherExecutableCandidates.last()),
              16);
    f.job.start(f.request);
    QVERIFY(f.result.has_value());
    QVERIFY(f.result->ok);
    QVERIFY(f.result->note.contains(QStringLiteral("already installed")));
    QVERIFY(f.downloader.requested.isEmpty());
    QVERIFY(f.result->launcher.executableUnixPath.endsWith(QStringLiteral("Battle.net.exe")));
  }

  void plannerRefusalIsSurfaced() {
    Fixture f;
    f.planner.refuse = true;
    QVERIFY(f.run());
    QVERIFY(!f.result->ok);
    QCOMPARE(f.result->message, QStringLiteral("The pinned Proton build is a floating alias."));
    QVERIFY(f.runner.specs.isEmpty());
  }

  void damagedRecipeIsRefused() {
    Fixture f;
    f.request.recipe.installerUrl = QUrl(QStringLiteral("https://evil.example/Battle.net-Setup.exe"));
    f.job.start(f.request);
    QVERIFY(f.result.has_value());
    QVERIFY(!f.result->ok);
    QVERIFY(f.job.detailsText().contains(QStringLiteral("allowlist")));
    QVERIFY(f.downloader.requested.isEmpty());
  }
};

QTEST_GUILESS_MAIN(tst_launcher_install_job)
#include "tst_launcher_install_job.moc"
