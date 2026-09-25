// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_install_job.h"

#include "download_allowlist.h"
#include "downloader.h"
#include "fs_ops.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>

namespace QindaQt::QindaLutris {

namespace {

constexpr qint64 kHashSliceBytes = 8 * 1024 * 1024;
constexpr int kListTimeoutMs = 5 * 60 * 1000;
constexpr int kExtractTimeoutMs = 20 * 60 * 1000;
constexpr qsizetype kMaxListingBytes = 64 * 1024 * 1024;

const QString kDownloadFailed = QStringLiteral(
    "The Proton build could not be downloaded. Check your internet connection "
    "and try again.");
const QString kUnsafeArchive = QStringLiteral(
    "The downloaded Proton build was not in the expected shape, so it was not "
    "installed.");
const QString kUnpackFailed = QStringLiteral(
    "The Proton build could not be unpacked. Check that there is enough free "
    "disk space and try again.");

QString archivePath(const QTemporaryDir &staging, const GeProtonRelease &release) {
  return staging.filePath(release.tarballName);
}

QString extractDirectory(const QTemporaryDir &staging) {
  return staging.filePath(QStringLiteral("extract"));
}

QString firstLines(const QByteArray &text, int lines) {
  return QString::fromUtf8(text).split(QLatin1Char('\n')).mid(0, lines).join(
      QLatin1Char('\n'));
}

} // namespace

QString defaultUserCompatToolsRoot() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
         QStringLiteral("/Steam/compatibilitytools.d");
}

ProtonInstallJob::ProtonInstallJob(QString targetRoot, Downloader *downloader,
                                   ProcessRunner *runner, QObject *parent)
    : QObject(parent), m_root(QDir::cleanPath(targetRoot)), m_downloader(downloader),
      m_runner(runner) {
  connect(m_downloader, &Downloader::progress, this, &ProtonInstallJob::onDownloadProgress);
  connect(m_downloader, &Downloader::finished, this, &ProtonInstallJob::onDownloadFinished);
  connect(m_runner, &ProcessRunner::finished, this, &ProtonInstallJob::onProcessFinished);
  m_hashTimer.setInterval(0);
  connect(&m_hashTimer, &QTimer::timeout, this, &ProtonInstallJob::hashSlice);
}

ProtonInstallJob::~ProtonInstallJob() {
  if (isRunning()) {
    cancel();
  }
}

void ProtonInstallJob::start(const GeProtonRelease &release) {
  if (isRunning()) {
    return;
  }
  m_release = release;
  m_log.reset(QStringLiteral("QindaLutris Proton install: %1").arg(release.toolName));
  m_log.append(QStringLiteral("Target folder: %1").arg(m_root));

  if (!isSafeToolName(release.toolName) || !isSafeToolName(release.tarballName) ||
      !isSafeToolName(release.checksumName) || !isAllowedDownloadUrl(release.tarballUrl) ||
      !isAllowedDownloadUrl(release.checksumUrl)) {
    m_stage = Stage::DownloadingArchive; // so fail() concludes
    fail(QStringLiteral("This Proton release cannot be installed safely."),
         QStringLiteral("Release entry refused: unsafe name or non-allowlisted URL."));
    return;
  }
  m_stage = Stage::DownloadingArchive;
  if (QDir::isRelativePath(m_root) || !QDir().mkpath(m_root)) {
    fail(QStringLiteral("QindaLutris could not create its Proton builds folder."),
         QStringLiteral("Cannot create target root %1").arg(m_root));
    return;
  }
  const QString target = m_root + QLatin1Char('/') + release.toolName;
  if (QFileInfo::exists(target) || isSymlink(target)) {
    fail(QStringLiteral("%1 is already installed.").arg(release.toolName),
         QStringLiteral("Refused: %1 already exists.").arg(target));
    return;
  }
  m_resolvedTar = m_tarProgram.isEmpty()
                      ? QStandardPaths::findExecutable(QStringLiteral("tar"))
                      : m_tarProgram;
  if (m_resolvedTar.isEmpty()) {
    fail(QStringLiteral("The tar program is missing, so Proton builds cannot be "
                        "unpacked. Install the app-arch/tar package."),
         QStringLiteral("tar not found on PATH"));
    return;
  }
  m_staging = std::make_unique<QTemporaryDir>(
      m_root + QStringLiteral("/.qindalutris-staging-XXXXXX"));
  if (!m_staging->isValid()) {
    fail(QStringLiteral("QindaLutris could not write to its Proton builds folder."),
         QStringLiteral("Cannot create staging directory: %1").arg(m_staging->errorString()));
    return;
  }
  m_reported = 0.0;
  m_log.append(QStringLiteral("Downloading %1").arg(release.tarballUrl.toString()));
  report(0.0, QStringLiteral("Downloading %1").arg(release.toolName));
  m_downloader->start(release.tarballUrl, archivePath(*m_staging, release));
}

void ProtonInstallJob::onDownloadProgress(qint64 received, qint64 total) {
  if (m_stage != Stage::DownloadingArchive) {
    return;
  }
  if (total <= 0) {
    total = m_release.tarballBytes;
  }
  if (total > 0) {
    const double share = std::min(1.0, double(received) / double(total));
    report(0.75 * share, QStringLiteral("Downloading %1").arg(m_release.toolName));
  }
}

void ProtonInstallJob::onDownloadFinished(bool ok, const QString &reason) {
  if (m_stage == Stage::DownloadingArchive) {
    if (!ok) {
      fail(kDownloadFailed, QStringLiteral("Archive download failed: %1").arg(reason));
      return;
    }
    m_log.append(QStringLiteral("Archive downloaded; fetching %1").arg(m_release.checksumName));
    m_stage = Stage::DownloadingChecksum;
    report(0.75, QStringLiteral("Downloading the safety checksum"));
    m_downloader->start(m_release.checksumUrl, m_staging->filePath(m_release.checksumName));
  } else if (m_stage == Stage::DownloadingChecksum) {
    if (!ok) {
      fail(kDownloadFailed, QStringLiteral("Checksum download failed: %1").arg(reason));
      return;
    }
    beginHashing();
  }
}

void ProtonInstallJob::beginHashing() {
  QFile sums(m_staging->filePath(m_release.checksumName));
  QByteArray content;
  if (sums.open(QIODevice::ReadOnly)) {
    content = sums.read(64 * 1024);
  }
  const auto expected = parseSha512SumFile(content, m_release.tarballName);
  if (!expected) {
    fail(QStringLiteral("The Proton build's safety checksum could not be read, so it "
                        "was not installed."),
         QStringLiteral("No SHA-512 line for %1 in %2")
             .arg(m_release.tarballName, m_release.checksumName));
    return;
  }
  m_expectedHash = *expected;
  m_hash.reset();
  m_hashFile.setFileName(archivePath(*m_staging, m_release));
  if (!m_hashFile.open(QIODevice::ReadOnly)) {
    fail(kUnpackFailed, QStringLiteral("Cannot reopen archive: %1").arg(m_hashFile.errorString()));
    return;
  }
  m_stage = Stage::Hashing;
  report(0.77, QStringLiteral("Checking the download"));
  m_hashTimer.start();
}

void ProtonInstallJob::hashSlice() {
  if (m_stage != Stage::Hashing) {
    m_hashTimer.stop();
    return;
  }
  const QByteArray chunk = m_hashFile.read(kHashSliceBytes);
  if (!chunk.isEmpty()) {
    m_hash.addData(chunk);
    const qint64 size = m_hashFile.size();
    if (size > 0) {
      report(0.77 + 0.08 * double(m_hashFile.pos()) / double(size),
                      QStringLiteral("Checking the download"));
    }
    return;
  }
  m_hashTimer.stop();
  m_hashFile.close();
  const QByteArray actual = m_hash.result().toHex();
  if (actual != m_expectedHash) {
    fail(QStringLiteral("The downloaded Proton build did not pass its safety check, "
                        "so it was not installed. Try again later."),
         QStringLiteral("SHA-512 mismatch: expected %1, got %2")
             .arg(QString::fromLatin1(m_expectedHash), QString::fromLatin1(actual)));
    return;
  }
  m_log.append(QStringLiteral("SHA-512 verified."));
  beginListing();
}

void ProtonInstallJob::beginListing() {
  m_stage = Stage::Listing;
  report(0.85, QStringLiteral("Checking the archive"));
  ProcessRunSpec spec;
  spec.program = m_resolvedTar;
  spec.arguments = {QStringLiteral("--list"), QStringLiteral("--gzip"),
                    QStringLiteral("--file=") + archivePath(*m_staging, m_release)};
  spec.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  spec.timeoutMs = kListTimeoutMs;
  spec.maxOutputBytes = kMaxListingBytes;
  m_runner->start(spec);
}

void ProtonInstallJob::beginExtracting() {
  const QString into = extractDirectory(*m_staging);
  if (!QDir().mkpath(into)) {
    fail(kUnpackFailed, QStringLiteral("Cannot create %1").arg(into));
    return;
  }
  m_stage = Stage::Extracting;
  report(0.87, QStringLiteral("Unpacking %1").arg(m_release.toolName));
  ProcessRunSpec spec;
  spec.program = m_resolvedTar;
  // AGENT-GUARD: never add --absolute-names or -P; GNU tar's default strips
  // leading '/' and defers dangerous symlinks, and the listing check above
  // has already refused '..' and a second top-level entry.
  spec.arguments = {QStringLiteral("--extract"), QStringLiteral("--gzip"),
                    QStringLiteral("--no-same-owner"),
                    QStringLiteral("--file=") + archivePath(*m_staging, m_release),
                    QStringLiteral("--directory=") + into};
  spec.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  spec.timeoutMs = kExtractTimeoutMs;
  m_runner->start(spec);
}

void ProtonInstallJob::onProcessFinished(const ProcessRunResult &result) {
  if (m_stage != Stage::Listing && m_stage != Stage::Extracting) {
    return;
  }
  const bool clean = result.started && !result.timedOut && !result.crashed &&
                     result.exitCode == 0;
  if (!clean) {
    fail(kUnpackFailed, QStringLiteral("tar failed (exit %1): %2 %3")
                            .arg(result.exitCode)
                            .arg(result.error, firstLines(result.standardError, 20)));
    return;
  }
  if (m_stage == Stage::Listing) {
    if (result.outputTruncated) {
      fail(kUnsafeArchive, QStringLiteral("Archive listing exceeded the size limit."));
      return;
    }
    const ArchiveListingVerdict verdict = validateSingleTopLevelListing(result.standardOutput);
    if (!verdict.ok) {
      fail(kUnsafeArchive, verdict.reason);
      return;
    }
    if (verdict.topLevel != m_release.toolName) {
      fail(kUnsafeArchive, QStringLiteral("Archive folder %1 does not match %2")
                               .arg(verdict.topLevel, m_release.toolName));
      return;
    }
    m_log.append(QStringLiteral("Archive listing accepted."));
    beginExtracting();
    return;
  }
  commit();
}

void ProtonInstallJob::commit() {
  const QString into = extractDirectory(*m_staging);
  const QStringList entries =
      QDir(into).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
  const QString extracted = into + QLatin1Char('/') + m_release.toolName;
  if (entries != QStringList{m_release.toolName} || isSymlink(extracted) ||
      !QFileInfo(extracted).isDir()) {
    fail(kUnsafeArchive, QStringLiteral("Unexpected extraction result: %1")
                             .arg(entries.join(QStringLiteral(", "))));
    return;
  }
  const QString target = m_root + QLatin1Char('/') + m_release.toolName;
  QString error;
  if (!renameNoReplace(extracted, target, &error)) {
    const bool exists = QFileInfo::exists(target);
    fail(exists ? QStringLiteral("%1 is already installed.").arg(m_release.toolName)
                : kUnpackFailed,
         QStringLiteral("Final rename to %1 failed: %2").arg(target, error));
    return;
  }
  m_log.append(QStringLiteral("Installed at %1").arg(target));
  ProtonJobResult result;
  result.ok = true;
  result.toolName = m_release.toolName;
  result.installedPath = target;
  result.message = QStringLiteral("%1 is installed.").arg(m_release.toolName);
  report(1.0, result.message);
  conclude(result);
}

void ProtonInstallJob::report(double fraction, const QString &stageText) {
  // Progress only moves forward, whatever rounding the stage maths produced.
  m_reported = std::clamp(std::max(m_reported, fraction), 0.0, 1.0);
  Q_EMIT progress(m_reported, stageText);
}

void ProtonInstallJob::fail(const QString &plain, const QString &detail) {
  m_log.append(detail);
  m_log.append(QStringLiteral("Result: %1").arg(plain));
  ProtonJobResult result;
  result.toolName = m_release.toolName;
  result.message = plain;
  conclude(result);
}

void ProtonInstallJob::cancel() {
  if (!isRunning()) {
    return;
  }
  m_log.append(QStringLiteral("Cancelled by the user."));
  ProtonJobResult result;
  result.cancelled = true;
  result.toolName = m_release.toolName;
  result.message = QStringLiteral("Installing %1 was cancelled.").arg(m_release.toolName);
  conclude(result);
}

void ProtonInstallJob::conclude(ProtonJobResult result) {
  const Stage was = m_stage;
  m_stage = Stage::Idle;
  m_hashTimer.stop();
  if (m_hashFile.isOpen()) {
    m_hashFile.close();
  }
  if (was == Stage::DownloadingArchive || was == Stage::DownloadingChecksum) {
    m_downloader->cancel();
  } else if (was == Stage::Listing || was == Stage::Extracting) {
    m_runner->cancel();
  }
  m_staging.reset(); // removes staging recursively
  Q_EMIT finished(result);
}

} // namespace QindaQt::QindaLutris
