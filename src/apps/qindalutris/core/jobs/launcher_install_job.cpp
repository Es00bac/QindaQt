// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_install_job.h"

#include "downloader.h"
#include "prefix_paths.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

namespace QindaQt::QindaLutris {

QString defaultGamesDirectory() { return QDir::homePath() + QStringLiteral("/Games"); }

QString defaultInstallerDownloadDirectory() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
         QStringLiteral("/qindalutris/installers");
}

LauncherInstallJob::LauncherInstallJob(Downloader *downloader, InstallerRunner *runner,
                                       const SystemProbe *probe, InstallerPlanner planner,
                                       QObject *parent)
    : QObject(parent), m_downloader(downloader), m_runner(runner), m_probe(probe),
      m_planner(std::move(planner)) {
  connect(m_downloader, &Downloader::progress, this, &LauncherInstallJob::onDownloadProgress);
  connect(m_downloader, &Downloader::finished, this, &LauncherInstallJob::onDownloadFinished);
  connect(m_runner, &ProcessRunner::finished, this, &LauncherInstallJob::onInstallerFinished);
}

LauncherInstallJob::~LauncherInstallJob() {
  ++m_generation; // no queued result after destruction
  if (m_stage == Stage::Downloading) {
    m_downloader->cancel();
  } else if (m_stage == Stage::Installing) {
    // The runner outlives us and stops the whole tree on its own; the
    // installer file stays (the tree may still use it) for the cache.
    m_runner->cancel();
  }
}

void LauncherInstallJob::start(const LauncherInstallRequest &request) {
  if (isRunning()) {
    return;
  }
  m_request = request;
  m_name = request.recipe.displayName.isEmpty() ? request.recipe.id : request.recipe.displayName;
  if (m_request.title.trimmed().isEmpty()) {
    m_request.title = m_name;
  }
  m_installerFile.clear();
  m_umuRun.clear();
  m_keepInstaller = false;
  m_stage = Stage::Preflight;
  m_log.reset(QStringLiteral("QindaLutris launcher install: %1 (%2)")
                  .arg(m_name, request.recipe.id));
  Q_EMIT progress(0.0, QStringLiteral("Checking this computer"));

  const QStringList problems = validateStoreRecipe(request.recipe);
  if (!problems.isEmpty()) {
    fail(QStringLiteral("QindaLutris's instructions for %1 are damaged, so nothing was "
                        "installed.")
             .arg(m_name),
         QStringLiteral("Recipe problems: %1").arg(problems.join(QStringLiteral("; "))));
    return;
  }
  const QString prefix = QDir::cleanPath(request.prefixPath);
  if (prefix.isEmpty() || QDir::isRelativePath(prefix) || prefix == QLatin1String("/") ||
      request.downloadDirectory.isEmpty() || QDir::isRelativePath(request.downloadDirectory)) {
    fail(QStringLiteral("QindaLutris does not know where to install %1.").arg(m_name),
         QStringLiteral("Refused prefix '%1' / download directory '%2'")
             .arg(request.prefixPath, request.downloadDirectory));
    return;
  }
  m_request.prefixPath = prefix;
  m_log.append(QStringLiteral("Prefix: %1").arg(prefix));
  m_log.append(QStringLiteral("Proton build: %1 (%2)")
                   .arg(request.protonBuildName, request.protonBuildPath));

  PreflightRequest preflight;
  preflight.displayName = m_name;
  preflight.spacePath = prefix;
  preflight.minimumFreeBytes = request.recipe.minimumFreeBytes;
  preflight.protonBuildPath = request.protonBuildPath;
  const PreflightOutcome checked = runInstallPreflight(preflight, *m_probe);
  for (const QString &line : checked.details) {
    m_log.append(line);
  }
  if (!checked.ok) {
    fail(checked.message, QStringLiteral("Preflight refused."));
    return;
  }
  m_umuRun = checked.umuRunBinary;

  const QString existing =
      firstExistingCandidate(prefix, request.recipe.launcherExecutableCandidates);
  if (!existing.isEmpty()) {
    m_log.append(QStringLiteral("Launcher already present: %1").arg(existing));
    succeed(existing, QStringLiteral("%1 was already installed in this folder, so it "
                                     "was added without reinstalling.")
                          .arg(m_name));
    return;
  }

  if (!QDir().mkpath(request.downloadDirectory)) {
    fail(QStringLiteral("QindaLutris could not save the %1 installer.").arg(m_name),
         QStringLiteral("Cannot create %1").arg(request.downloadDirectory));
    return;
  }
  m_installerFile = QDir(request.downloadDirectory).filePath(request.recipe.installerFileName);
  m_stage = Stage::Downloading;
  m_log.append(QStringLiteral("Downloading %1").arg(request.recipe.installerUrl.toString()));
  m_downloader->start(request.recipe.installerUrl, m_installerFile);
  Q_EMIT progress(0.05, QStringLiteral("Downloading the %1 installer").arg(m_name));
}

void LauncherInstallJob::onDownloadProgress(qint64 received, qint64 total) {
  if (m_stage == Stage::Downloading && total > 0) {
    const double share = std::min(1.0, double(received) / double(total));
    Q_EMIT progress(0.05 + 0.45 * share,
                    QStringLiteral("Downloading the %1 installer").arg(m_name));
  }
}

void LauncherInstallJob::onDownloadFinished(bool ok, const QString &reason) {
  if (m_stage != Stage::Downloading) {
    return;
  }
  if (!ok) {
    fail(QStringLiteral("The %1 installer could not be downloaded. Check your internet "
                        "connection and try again.")
             .arg(m_name),
         QStringLiteral("Download failed: %1").arg(reason));
    return;
  }
  m_log.append(QStringLiteral("Installer downloaded."));
  if (!QDir().mkpath(m_request.prefixPath)) {
    fail(QStringLiteral("QindaLutris could not create the folder for %1.").arg(m_name),
         QStringLiteral("Cannot create prefix %1").arg(m_request.prefixPath));
    return;
  }
  InstallerPlanRequest planRequest;
  planRequest.umuRunBinary = m_umuRun;
  planRequest.protonBuildName = m_request.protonBuildName;
  planRequest.protonBuildPath = m_request.protonBuildPath;
  planRequest.prefixPath = m_request.prefixPath;
  planRequest.umuId = m_request.recipe.umuId;
  planRequest.umuStore = m_request.recipe.umuStore;
  planRequest.installerPath = m_installerFile;
  planRequest.installerKind = m_request.recipe.installerKind;
  planRequest.windowsCommand = installerCommand(
      m_request.recipe.installerKind, m_installerFile, m_request.recipe.installerArguments);
  const InstallerPlan plan = planInstallerRun(m_planner, planRequest);
  if (!plan.ok) {
    fail(plan.reason, QStringLiteral("Installer planning refused."));
    return;
  }
  m_log.append(QStringLiteral("Running: %1 %2")
                   .arg(plan.spec.program, plan.spec.arguments.join(QLatin1Char(' '))));
  m_stage = Stage::Installing;
  m_runner->start(plan.spec);
  Q_EMIT progress(0.5, QStringLiteral("Installing %1. If a window opens, follow it to "
                                      "the end; when %1 itself opens, close it to finish.")
                           .arg(m_name));
}

void LauncherInstallJob::onInstallerFinished(const ProcessRunResult &result) {
  if (m_stage == Stage::Stopping) {
    m_log.append(describeProcessResult(result));
    m_keepInstaller = !result.treeStopped;
    concludeCancelled();
    return;
  }
  if (m_stage != Stage::Installing) {
    return;
  }
  m_log.append(describeProcessResult(result));
  m_keepInstaller = !result.treeStopped;
  Q_EMIT progress(0.95, QStringLiteral("Looking for %1").arg(m_name));
  const QString found = firstExistingCandidate(m_request.prefixPath,
                                               m_request.recipe.launcherExecutableCandidates);
  // AGENT-NOTE: Proton's waitforexitandrun keeps umu-run alive until the
  // prefix's wineserver exits, so an installer that starts its launcher at
  // the end (Battle.net, Amazon) only "finishes" once the user closes that
  // launcher -- or at the time limit. Presence of the launcher decides.
  if (!found.isEmpty()) {
    m_log.append(QStringLiteral("Launcher found: %1").arg(found));
    const bool clean = result.started && !result.crashed && result.exitCode == 0;
    succeed(found, clean ? QString()
                         : QStringLiteral("The %1 installer reported a problem (code %2), "
                                          "but %1 is installed.")
                               .arg(m_name)
                               .arg(result.exitCode));
    return;
  }
  m_log.append(QStringLiteral("No launcher candidate exists in the prefix."));
  if (!result.started) {
    fail(QStringLiteral("The %1 installer could not be started.").arg(m_name), result.error);
  } else if (result.timedOut) {
    fail(QStringLiteral("The %1 installer took too long and was stopped.").arg(m_name),
         result.error);
  } else if (result.crashed || result.exitCode != 0) {
    fail(QStringLiteral("The %1 installer stopped with an error, and %1 was not installed.")
             .arg(m_name),
         QStringLiteral("Exit code %1").arg(result.exitCode));
  } else {
    fail(QStringLiteral("The %1 installer finished, but %1 was not found afterwards. Try "
                        "again, and follow the installer's window to the end.")
             .arg(m_name),
         QStringLiteral("Installer exited cleanly without installing a launcher."));
  }
}

void LauncherInstallJob::succeed(const QString &executable, const QString &note) {
  LauncherInstallResult result;
  result.ok = true;
  result.message = QStringLiteral("%1 is installed.").arg(m_name);
  result.note = note;
  result.launcher.recipeId = m_request.recipe.id;
  result.launcher.title = m_request.title;
  result.launcher.prefixPath = m_request.prefixPath;
  result.launcher.executableUnixPath = executable;
  result.launcher.protonBuildName = m_request.protonBuildName;
  result.launcher.umuId = m_request.recipe.umuId;
  result.launcher.umuStore = m_request.recipe.umuStore;
  m_log.append(QStringLiteral("Result: %1 %2").arg(result.message, note));
  Q_EMIT progress(1.0, result.message);
  conclude(result);
}

void LauncherInstallJob::fail(const QString &plain, const QString &detail) {
  if (!detail.isEmpty()) {
    m_log.append(detail);
  }
  m_log.append(QStringLiteral("Result: %1").arg(plain));
  LauncherInstallResult result;
  result.message = plain;
  conclude(result);
}

void LauncherInstallJob::cancel() {
  if (m_stage == Stage::Idle || m_stage == Stage::Stopping || m_stage == Stage::Concluding) {
    return;
  }
  m_log.append(QStringLiteral("Cancelled by the user."));
  if (m_stage == Stage::Installing) {
    // AGENT-GUARD: the installer file stays until the runner confirms the
    // whole tree (umu, pressure-vessel, Wine, the installer) is gone.
    m_stage = Stage::Stopping;
    m_runner->cancel();
    return;
  }
  concludeCancelled();
}

void LauncherInstallJob::concludeCancelled() {
  LauncherInstallResult result;
  result.cancelled = true;
  result.message = QStringLiteral("Installing %1 was cancelled.").arg(m_name);
  conclude(result);
}

void LauncherInstallJob::conclude(LauncherInstallResult result) {
  const Stage was = m_stage;
  m_stage = Stage::Concluding;
  if (was == Stage::Downloading) {
    m_downloader->cancel();
  }
  if (!m_installerFile.isEmpty()) {
    if (m_keepInstaller) {
      m_log.append(QStringLiteral("Installer kept at %1: its process tree was not confirmed "
                                  "stopped.")
                       .arg(m_installerFile));
    } else {
      QFile::remove(m_installerFile);
      QFile::remove(m_installerFile + QStringLiteral(".part"));
    }
  }
  const quint64 generation = ++m_generation;
  QTimer::singleShot(0, this, [this, generation, result] {
    if (generation == m_generation) {
      m_stage = Stage::Idle;
      Q_EMIT finished(result);
    }
  });
}

} // namespace QindaQt::QindaLutris
