// SPDX-License-Identifier: GPL-3.0-or-later
#include "setup_file_install_job.h"

#include "store_recipes.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

namespace QindaQt::QindaLutris {

SetupFileInstallJob::SetupFileInstallJob(InstallerRunner *runner, const SystemProbe *probe,
                                         InstallerPlanner planner, QObject *parent)
    : QObject(parent), m_runner(runner), m_probe(probe), m_planner(std::move(planner)) {
  connect(m_runner, &ProcessRunner::finished, this, &SetupFileInstallJob::onInstallerFinished);
}

SetupFileInstallJob::~SetupFileInstallJob() {
  ++m_generation; // no queued result after destruction
  if (m_stage == Stage::Installing) {
    m_runner->cancel(); // the runner stops the whole tree on its own
  }
}

void SetupFileInstallJob::start(const SetupFileInstallRequest &request) {
  if (isRunning()) {
    return;
  }
  m_stage = Stage::Concluding; // any early fail() concludes from here
  m_request = request;
  m_request.title = request.title.trimmed();
  if (m_request.title.isEmpty()) {
    m_request.title = QFileInfo(request.installerPath).completeBaseName();
  }
  m_log.reset(QStringLiteral("QindaLutris setup-file install: %1").arg(m_request.title));
  m_log.append(QStringLiteral("Installer: %1").arg(request.installerPath));
  Q_EMIT progress(0.0, QStringLiteral("Checking this computer"));

  const QFileInfo installer(request.installerPath);
  const QString suffix = installer.suffix().toLower();
  if (!installer.isFile() || !installer.isReadable() ||
      (suffix != QLatin1String("exe") && suffix != QLatin1String("msi"))) {
    fail(QStringLiteral("That file is not a Windows setup program QindaLutris can run. "
                        "Choose a file ending in .exe or .msi."),
         QStringLiteral("Refused installer file."));
    return;
  }
  static const QRegularExpression umuIdPattern(QStringLiteral("^umu-[A-Za-z0-9._-]{1,64}$"));
  if (!umuIdPattern.match(request.umuId).hasMatch() ||
      !knownUmuStores().contains(request.umuStore)) {
    fail(QStringLiteral("QindaLutris's information about %1 is damaged, so nothing was "
                        "installed.")
             .arg(m_request.title),
         QStringLiteral("Refused umu id '%1' / store '%2'").arg(request.umuId, request.umuStore));
    return;
  }
  const QString prefix = QDir::cleanPath(request.prefixPath);
  if (prefix.isEmpty() || QDir::isRelativePath(prefix) || prefix == QLatin1String("/")) {
    fail(QStringLiteral("QindaLutris does not know where to install %1.").arg(m_request.title),
         QStringLiteral("Refused prefix '%1'").arg(request.prefixPath));
    return;
  }
  m_request.prefixPath = prefix;

  PreflightRequest preflight;
  preflight.displayName = m_request.title;
  preflight.spacePath = prefix;
  preflight.minimumFreeBytes = request.minimumFreeBytes;
  preflight.protonBuildPath = request.protonBuildPath;
  const PreflightOutcome checked = runInstallPreflight(preflight, *m_probe);
  for (const QString &line : checked.details) {
    m_log.append(line);
  }
  if (!checked.ok) {
    fail(checked.message, QStringLiteral("Preflight refused."));
    return;
  }
  if (!QDir().mkpath(prefix)) {
    fail(QStringLiteral("QindaLutris could not create the folder for %1.").arg(m_request.title),
         QStringLiteral("Cannot create prefix %1").arg(prefix));
    return;
  }
  m_before = scanPrefixExecutables(prefix);
  m_log.append(QStringLiteral("Executables before install: %1").arg(m_before.size()));

  const InstallerKind kind = suffix == QLatin1String("msi") ? InstallerKind::Msi
                                                            : InstallerKind::Exe;
  InstallerPlanRequest planRequest;
  planRequest.umuRunBinary = checked.umuRunBinary;
  planRequest.protonBuildName = request.protonBuildName;
  planRequest.protonBuildPath = request.protonBuildPath;
  planRequest.prefixPath = prefix;
  planRequest.umuId = request.umuId;
  planRequest.umuStore = request.umuStore;
  planRequest.installerPath = installer.absoluteFilePath();
  planRequest.installerKind = kind;
  planRequest.windowsCommand = installerCommand(kind, installer.absoluteFilePath(), {});
  const InstallerPlan plan = planInstallerRun(m_planner, planRequest);
  if (!plan.ok) {
    fail(plan.reason, QStringLiteral("Installer planning refused."));
    return;
  }
  m_log.append(QStringLiteral("Running: %1 %2")
                   .arg(plan.spec.program, plan.spec.arguments.join(QLatin1Char(' '))));
  m_stage = Stage::Installing;
  m_runner->start(plan.spec);
  Q_EMIT progress(0.1, QStringLiteral("Installing %1. Follow the setup window to the end.")
                           .arg(m_request.title));
}

void SetupFileInstallJob::onInstallerFinished(const ProcessRunResult &result) {
  if (m_stage == Stage::Stopping) {
    m_log.append(describeProcessResult(result));
    concludeCancelled();
    return;
  }
  if (m_stage != Stage::Installing) {
    return;
  }
  m_log.append(describeProcessResult(result));
  Q_EMIT progress(0.9, QStringLiteral("Looking for the game"));
  const ExecutableSnapshot after = scanPrefixExecutables(m_request.prefixPath);
  const QVector<ExecutableCandidate> candidates =
      rankNewExecutables(m_before, after, m_request.title);
  m_log.append(QStringLiteral("Executables after install: %1, new candidates: %2")
                   .arg(after.size())
                   .arg(candidates.size()));
  for (const ExecutableCandidate &candidate : candidates) {
    m_log.append(QStringLiteral("Candidate (score %1): %2").arg(candidate.score).arg(candidate.unixPath));
  }
  if (candidates.isEmpty()) {
    if (!result.started) {
      fail(QStringLiteral("The setup program could not be started."), result.error);
    } else if (result.timedOut) {
      fail(QStringLiteral("The setup program took too long and was stopped."), result.error);
    } else {
      fail(QStringLiteral("The setup finished, but QindaLutris could not find the game it "
                          "installed. You can still choose the game's program yourself."),
           QStringLiteral("No new executables under the scanned folders."));
    }
    return;
  }
  SetupFileInstallResult done;
  done.ok = true;
  done.message = QStringLiteral("%1 is installed. Confirm which program starts the game.")
                     .arg(m_request.title);
  if (!result.started || result.crashed || result.exitCode != 0 || result.timedOut) {
    done.note = QStringLiteral("The setup program reported a problem (code %1), but it "
                               "installed files.")
                    .arg(result.exitCode);
  }
  done.candidates = candidates;
  m_log.append(QStringLiteral("Result: %1 %2").arg(done.message, done.note));
  Q_EMIT progress(1.0, done.message);
  conclude(done);
}

void SetupFileInstallJob::fail(const QString &plain, const QString &detail) {
  if (!detail.isEmpty()) {
    m_log.append(detail);
  }
  m_log.append(QStringLiteral("Result: %1").arg(plain));
  SetupFileInstallResult result;
  result.message = plain;
  conclude(result);
}

void SetupFileInstallJob::cancel() {
  if (m_stage != Stage::Installing) {
    return;
  }
  m_log.append(QStringLiteral("Cancelled by the user."));
  m_stage = Stage::Stopping;
  m_runner->cancel();
}

void SetupFileInstallJob::concludeCancelled() {
  SetupFileInstallResult result;
  result.cancelled = true;
  result.message = QStringLiteral("Installing %1 was cancelled.").arg(m_request.title);
  conclude(result);
}

void SetupFileInstallJob::conclude(SetupFileInstallResult result) {
  m_stage = Stage::Concluding;
  result.title = m_request.title;
  result.prefixPath = m_request.prefixPath;
  result.protonBuildName = m_request.protonBuildName;
  result.umuId = m_request.umuId;
  result.umuStore = m_request.umuStore;
  const quint64 generation = ++m_generation;
  QTimer::singleShot(0, this, [this, generation, result] {
    if (generation == m_generation) {
      m_stage = Stage::Idle;
      Q_EMIT finished(result);
    }
  });
}

} // namespace QindaQt::QindaLutris
