// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_install_job.h"

#include "archive_listing.h"
#include "downloader.h"
#include "fs_ops.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>
#include <cerrno>

namespace QindaQt::QindaLutris {

namespace {

constexpr qint64 kHashSliceBytes = 8 * 1024 * 1024;
constexpr int kListTimeoutMs = 5 * 60 * 1000;
constexpr int kExtractTimeoutMs = 20 * 60 * 1000;
constexpr qsizetype kMaxListingBytes = 64 * 1024 * 1024;
constexpr qint64 kUnknownSizeNeed = qint64(3) * 1024 * 1024 * 1024;

const QString kDownloadFailed = QStringLiteral(
    "The Proton build could not be downloaded. Check your internet connection "
    "and try again.");
const QString kUnsafeArchive = QStringLiteral(
    "The downloaded Proton build was not in the expected shape, so it was not "
    "installed.");
const QString kUnpackFailed = QStringLiteral(
    "The Proton build could not be unpacked, so it was not installed. Try again, "
    "or copy the details for someone helping you.");
const QString kNotAtomic = QStringLiteral(
    "The folder for Proton builds is on a disk that cannot install them safely. "
    "Keep your Steam folder on a Linux disk such as ext4 or btrfs.");

QString archivePath(const QTemporaryDir &staging, const GeProtonRelease &release) {
  return staging.filePath(release.tarballName);
}

QString extractDirectory(const QTemporaryDir &staging) {
  return staging.filePath(QStringLiteral("extract"));
}

QString firstLines(const QByteArray &text, int lines) {
  return QString::fromUtf8(text).split(QLatin1Char('\n')).mid(0, lines).join(QLatin1Char('\n'));
}

} // namespace

QString defaultUserCompatToolsRoot() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
         QStringLiteral("/Steam/compatibilitytools.d");
}

qint64 protonInstallSpaceNeeded(qint64 tarballBytes) {
  return tarballBytes > 0 ? std::max(tarballBytes * 4, qint64(256) * 1024 * 1024)
                          : kUnknownSizeNeed;
}

ProtonInstallJob::ProtonInstallJob(QString targetRoot, Downloader *downloader,
                                   ProcessRunner *runner, const SystemProbe *probe,
                                   QObject *parent)
    : QObject(parent), m_root(QDir::cleanPath(targetRoot)), m_downloader(downloader),
      m_runner(runner), m_probe(probe) {
  connect(m_downloader, &Downloader::progress, this, &ProtonInstallJob::onDownloadProgress);
  connect(m_downloader, &Downloader::finished, this, &ProtonInstallJob::onDownloadFinished);
  connect(m_runner, &ProcessRunner::finished, this, &ProtonInstallJob::onProcessFinished);
  m_hashTimer.setInterval(0);
  connect(&m_hashTimer, &QTimer::timeout, this, &ProtonInstallJob::hashSlice);
}

ProtonInstallJob::~ProtonInstallJob() {
  ++m_generation; // no queued result after destruction
  m_hashTimer.stop();
  if (m_stage == Stage::DownloadingArchive || m_stage == Stage::DownloadingChecksum) {
    m_downloader->cancel();
  }
  if (m_stage == Stage::Listing || m_stage == Stage::Extracting) {
    // The runner stops tar's tree on its own; staging stays (dot-named)
    // because tar may still be writing into it.
    m_runner->cancel();
  } else if (m_staging && m_stage != Stage::Stopping) {
    (void)removeTreeForcibly(m_staging->path());
  }
}

void ProtonInstallJob::start(const GeProtonRelease &release) {
  if (isRunning()) {
    return;
  }
  m_release = release;
  m_reported = 0.0;
  m_keepStaging = false;
  m_stage = Stage::DownloadingArchive; // any early fail() concludes from here
  m_log.reset(QStringLiteral("QindaLutris Proton install: %1").arg(release.toolName));
  m_log.append(QStringLiteral("Target folder: %1").arg(m_root));

  if (!isUpstreamGeProtonRelease(release)) {
    fail(QStringLiteral("This Proton release cannot be installed safely."),
         QStringLiteral("Release entry refused: not an upstream GloriousEggroll release "
                        "(tag, file names and download URLs must match)."));
    return;
  }
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
  const qint64 needed = protonInstallSpaceNeeded(release.tarballBytes);
  const std::optional<qint64> free = m_probe != nullptr ? m_probe->availableBytes(m_root)
                                                        : std::nullopt;
  m_log.append(QStringLiteral("Free space: %1, needed: %2")
                   .arg(free ? QString::number(*free) : QStringLiteral("unknown"))
                   .arg(needed));
  if (free && *free < needed) {
    fail(QStringLiteral("There is not enough free disk space to install %1: it needs at "
                        "least %2, and only %3 is free. Free up some space, then try again.")
             .arg(release.toolName, plainSize(needed), plainSize(*free)),
         QStringLiteral("Preflight refused: not enough free space."));
    return;
  }
  m_resolvedTar = m_tarProgram.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("tar"))
                                         : m_tarProgram;
  if (m_resolvedTar.isEmpty()) {
    fail(QStringLiteral("The tar program is missing, so Proton builds cannot be "
                        "unpacked. Install the app-arch/tar package."),
         QStringLiteral("tar not found on PATH"));
    return;
  }
  m_staging = std::make_unique<QTemporaryDir>(m_root + QStringLiteral("/.qindalutris-staging-XXXXXX"));
  // AGENT-GUARD: never let QTemporaryDir delete staging: it cannot remove
  // read-only folders, and it must not run while a tar tree may still write.
  m_staging->setAutoRemove(false);
  if (!m_staging->isValid()) {
    fail(QStringLiteral("QindaLutris could not write to its Proton builds folder."),
         QStringLiteral("Cannot create staging directory: %1").arg(m_staging->errorString()));
    return;
  }
  m_log.append(QStringLiteral("Downloading %1").arg(release.tarballUrl.toString()));
  m_downloader->start(release.tarballUrl, archivePath(*m_staging, release));
  report(0.0, QStringLiteral("Downloading %1").arg(release.toolName));
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
    m_downloader->start(m_release.checksumUrl, m_staging->filePath(m_release.checksumName));
    report(0.75, QStringLiteral("Downloading the safety checksum"));
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
  m_hashTimer.start();
  report(0.77, QStringLiteral("Checking the download"));
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
  ProcessRunSpec spec;
  spec.program = m_resolvedTar;
  spec.arguments = tarListArguments(archivePath(*m_staging, m_release));
  spec.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  spec.timeoutMs = kListTimeoutMs;
  spec.maxOutputBytes = kMaxListingBytes;
  m_runner->start(spec);
  report(0.85, QStringLiteral("Checking the archive"));
}

void ProtonInstallJob::beginExtracting() {
  const QString into = extractDirectory(*m_staging);
  if (!QDir().mkpath(into)) {
    fail(kUnpackFailed, QStringLiteral("Cannot create %1").arg(into));
    return;
  }
  m_stage = Stage::Extracting;
  ProcessRunSpec spec;
  spec.program = m_resolvedTar;
  // AGENT-GUARD: never add --absolute-names/-P or --same-permissions; the
  // listing check has already refused escapes, links out, devices and
  // setuid entries, and GNU tar's own defences remain as a second line.
  spec.arguments = {QStringLiteral("--extract"), QStringLiteral("--gzip"),
                    QStringLiteral("--no-same-owner"), QStringLiteral("--no-same-permissions"),
                    QStringLiteral("--file=") + archivePath(*m_staging, m_release),
                    QStringLiteral("--directory=") + into};
  spec.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  spec.timeoutMs = kExtractTimeoutMs;
  m_runner->start(spec);
  report(0.87, QStringLiteral("Unpacking %1").arg(m_release.toolName));
}

void ProtonInstallJob::onProcessFinished(const ProcessRunResult &result) {
  if (m_stage == Stage::Stopping) {
    finishCancel(&result);
    return;
  }
  if (m_stage != Stage::Listing && m_stage != Stage::Extracting) {
    return;
  }
  const bool clean = result.started && !result.timedOut && !result.crashed && result.exitCode == 0;
  if (!clean) {
    const QString detail = QStringLiteral("tar failed (exit %1): %2 %3 %4")
                               .arg(result.exitCode)
                               .arg(result.error, firstLines(result.standardError, 20),
                                    result.stopDetail);
    m_keepStaging = (result.timedOut || result.cancelled) && !result.treeStopped;
    fail(kUnpackFailed, detail);
    return;
  }
  if (m_stage == Stage::Listing) {
    if (result.outputTruncated) {
      fail(kUnsafeArchive, QStringLiteral("Archive listing exceeded the size limit."));
      return;
    }
    const ArchiveListingVerdict verdict =
        validateArchiveListing(result.standardOutput, result.standardError);
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
  int code = 0;
  if (!renameNoReplace(extracted, target, &error, &code)) {
    const QString detail = QStringLiteral("Final rename to %1 failed: %2").arg(target, error);
    if (code == EEXIST || code == ENOTEMPTY) {
      fail(QStringLiteral("%1 is already installed.").arg(m_release.toolName), detail);
    } else if (code == EINVAL || code == ENOSYS || code == EXDEV || code == EOPNOTSUPP) {
      fail(kNotAtomic, detail);
    } else {
      fail(kUnpackFailed, detail);
    }
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
  if (m_stage == Stage::Idle || m_stage == Stage::Stopping || m_stage == Stage::Concluding) {
    return;
  }
  m_log.append(QStringLiteral("Cancelled by the user."));
  if (m_stage == Stage::Listing || m_stage == Stage::Extracting) {
    m_stage = Stage::Stopping; // finishCancel() runs once tar's tree is gone
    m_runner->cancel();
    return;
  }
  finishCancel(nullptr);
}

void ProtonInstallJob::finishCancel(const ProcessRunResult *stoppedRun) {
  if (stoppedRun != nullptr) {
    m_log.append(stoppedRun->stopDetail);
    if (!stoppedRun->treeStopped) {
      m_log.append(QStringLiteral("tar could not be confirmed stopped; staging kept."));
    }
  }
  ProtonJobResult result;
  result.cancelled = true;
  result.toolName = m_release.toolName;
  result.message = QStringLiteral("Installing %1 was cancelled.").arg(m_release.toolName);
  m_keepStaging = stoppedRun != nullptr && !stoppedRun->treeStopped;
  conclude(result);
}

void ProtonInstallJob::conclude(ProtonJobResult result) {
  const Stage was = m_stage;
  m_stage = Stage::Concluding;
  m_hashTimer.stop();
  if (m_hashFile.isOpen()) {
    m_hashFile.close();
  }
  if (was == Stage::DownloadingArchive || was == Stage::DownloadingChecksum) {
    m_downloader->cancel();
  }
  if (m_staging) {
    QString leftover;
    if (m_keepStaging) {
      // AGENT-GUARD: a tar that may still run could write into a folder we
      // delete; leave it (dot-named, ignored by catalogs) rather than race.
      m_log.append(QStringLiteral("Staging left at %1").arg(m_staging->path()));
    } else if (!removeTreeForcibly(m_staging->path(), &leftover)) {
      m_log.append(QStringLiteral("Temporary files remain: %1").arg(leftover));
    }
    m_staging.reset();
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
