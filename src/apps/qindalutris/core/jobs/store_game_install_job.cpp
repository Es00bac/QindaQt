// SPDX-License-Identifier: GPL-3.0-or-later
#include "store_game_install_job.h"

#include "installer_planning.h"
#include "process_runner.h"

#include <QFileInfo>

namespace QindaQt::QindaLutris {

StoreGameInstallJob::StoreGameInstallJob(ProcessRunner *runner, QObject *parent)
    : QObject(parent), m_runner(runner) {
  connect(m_runner, &ProcessRunner::outputReceived, this, &StoreGameInstallJob::onOutput);
  connect(m_runner, &ProcessRunner::finished, this, &StoreGameInstallJob::onRunFinished);
}

StoreGameInstallJob::~StoreGameInstallJob() {
  if (isRunning()) {
    m_stage = Stage::Idle; // no result after destruction
    m_runner->cancel();    // the runner stops the client's whole tree
  }
}

void StoreGameInstallJob::start(const StoreGameInstallRequest &request) {
  if (isRunning()) {
    return;
  }
  m_request = request;
  m_partialLine.clear();
  m_tail.clear();
  m_log.reset(QStringLiteral("QindaLutris store install: %1 (%2 %3)")
                  .arg(request.title, storeClientId(request.client), request.gameId));
  if (request.binaries.forClient(request.client).isEmpty()) {
    m_stage = Stage::Download;
    fail(QStringLiteral("%1 support is not installed.")
             .arg(storeClientDisplayName(request.client)));
    return;
  }
  const std::optional<StoreClientRun> run =
      installRun(request.client, request.binaries, request.storesRoot, request.gameId,
                 request.baseDir);
  m_stage = Stage::Download;
  if (!run) {
    fail(QStringLiteral("This game's store entry could not be read. Refresh the "
                        "library and try again."));
    return;
  }
  m_log.append(QStringLiteral("download: %1 %2")
                   .arg(run->spec.program, run->spec.arguments.join(QLatin1Char(' '))));
  Q_EMIT progressChanged(-1, QStringLiteral("Downloading %1…").arg(request.title));
  m_runner->start(run->spec);
}

void StoreGameInstallJob::cancel() {
  if (isRunning()) {
    m_runner->cancel();
  }
}

void StoreGameInstallJob::onOutput(const QByteArray &chunk) {
  if (m_stage != Stage::Download) {
    return;
  }
  m_partialLine += chunk;
  // Clients redraw progress with '\r' as well as '\n'.
  m_partialLine.replace('\r', '\n');
  const qsizetype cut = m_partialLine.lastIndexOf('\n');
  if (cut < 0) {
    m_partialLine = m_partialLine.right(4096);
    return;
  }
  const QByteArray complete = m_partialLine.left(cut);
  m_partialLine = m_partialLine.mid(cut + 1);
  keepTail(complete);
  std::optional<double> latest;
  for (const QByteArray &line : complete.split('\n')) {
    if (const auto fraction = parseDownloadProgress(QString::fromUtf8(line))) {
      latest = fraction;
    }
  }
  if (latest) {
    Q_EMIT progressChanged(*latest, QStringLiteral("Downloading %1… %2%")
                                        .arg(m_request.title)
                                        .arg(qRound(*latest * 100)));
  }
}

void StoreGameInstallJob::keepTail(const QByteArray &output) {
  for (const QByteArray &line : output.split('\n')) {
    const QString text = QString::fromUtf8(line).trimmed();
    if (!text.isEmpty()) {
      m_tail.append(text.left(JobLog::kMaxLineChars));
    }
  }
  if (m_tail.size() > kMaxTailLines) {
    m_tail = m_tail.mid(m_tail.size() - kMaxTailLines);
  }
}

void StoreGameInstallJob::onRunFinished(const ProcessRunResult &result) {
  if (m_stage == Stage::Idle) {
    return;
  }
  const bool ok = result.started && !result.timedOut && !result.cancelled &&
                  !result.crashed && result.exitCode == 0;
  if (m_stage == Stage::Download) {
    keepTail(m_partialLine);
    m_partialLine.clear();
    m_log.append(QStringLiteral("download finished: %1").arg(describeProcessResult(result)));
    for (const QString &line : m_tail) {
      m_log.append(QStringLiteral("  %1").arg(line));
    }
  }
  if (result.cancelled) {
    m_log.append(QStringLiteral("stopped"));
    m_stage = Stage::Idle;
    StoreGameInstallResult out;
    out.cancelled = true;
    out.message = QStringLiteral("The download was stopped.");
    Q_EMIT finished(out);
    return;
  }
  if (m_stage == Stage::Download) {
    if (!ok) {
      fail(QStringLiteral("%1 could not be downloaded. Check your internet connection "
                          "and free disk space, then try again.")
               .arg(m_request.title));
      return;
    }
    m_stage = Stage::Locate;
    Q_EMIT progressChanged(1.0, QStringLiteral("Finishing %1…").arg(m_request.title));
    const std::optional<StoreClientRun> info = installedInfoRun(
        m_request.client, m_request.binaries, m_request.storesRoot, m_request.gameId);
    if (!info) { // GOG: no client run needed
      locate({});
      return;
    }
    m_log.append(QStringLiteral("locate: %1 %2")
                     .arg(info->spec.program, info->spec.arguments.join(QLatin1Char(' '))));
    m_runner->start(info->spec);
    return;
  }
  m_log.append(QStringLiteral("locate finished: %1").arg(describeProcessResult(result)));
  if (!ok) {
    fail(QStringLiteral("%1 was downloaded, but its program could not be found.")
             .arg(m_request.title));
    return;
  }
  locate(result.standardOutput);
}

void StoreGameInstallJob::locate(const QByteArray &clientOutput) {
  std::optional<StoreGameInstall> install;
  switch (m_request.client) {
  case StoreClient::Epic:
    install = parseEpicInstalled(clientOutput, m_request.gameId);
    break;
  case StoreClient::Gog:
    install = readGogInstall(m_request.baseDir, m_request.gameId);
    break;
  case StoreClient::Amazon:
    install = parseAmazonLaunchInfo(clientOutput);
    break;
  }
  if (!install || !QFileInfo(install->executable).isFile()) {
    if (install) {
      m_log.append(QStringLiteral("program not found: %1").arg(install->executable));
    }
    fail(QStringLiteral("%1 was downloaded, but its program could not be found.")
             .arg(m_request.title));
    return;
  }
  m_log.append(QStringLiteral("installed: %1").arg(install->executable));
  m_stage = Stage::Idle;
  StoreGameInstallResult out;
  out.ok = true;
  out.message = QStringLiteral("%1 is installed.").arg(m_request.title);
  out.install = *install;
  Q_EMIT finished(out);
}

void StoreGameInstallJob::fail(const QString &message) {
  m_log.append(QStringLiteral("failed: %1").arg(message));
  m_stage = Stage::Idle;
  StoreGameInstallResult out;
  out.message = message;
  Q_EMIT finished(out);
}

} // namespace QindaQt::QindaLutris
