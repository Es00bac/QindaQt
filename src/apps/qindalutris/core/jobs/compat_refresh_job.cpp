// SPDX-License-Identifier: GPL-3.0-or-later
#include "compat_refresh_job.h"

#include "downloader.h"
#include "ge_proton_releases.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace QindaQt::QindaLutris {
namespace {

QString checksumName() {
  return QString::fromLatin1(kCompatDbFileName) + QStringLiteral(".sha512sum");
}

} // namespace

QUrl compatDbRefreshUrl() {
  return QUrl(QString::fromLatin1(kCompatDbReleaseBase) + QString::fromLatin1(kCompatDbFileName));
}

QUrl compatDbChecksumUrl() {
  return QUrl(QString::fromLatin1(kCompatDbReleaseBase) + checksumName());
}

CompatRefreshJob::CompatRefreshJob(Downloader *downloader, QObject *parent)
    : QObject(parent), m_downloader(downloader) {
  connect(m_downloader, &Downloader::finished, this, &CompatRefreshJob::onDownloadFinished);
  connect(m_downloader, &Downloader::progress, this, [this](qint64 received, qint64) {
    if (m_stage != Stage::Idle && received > kMaxDocumentBytes) {
      m_downloader->cancel();
      conclude(false, QStringLiteral("The downloaded information was too large; nothing changed."));
    }
  });
}

CompatRefreshJob::~CompatRefreshJob() {
  if (isRunning()) {
    m_stage = Stage::Idle;
    m_downloader->cancel();
  }
}

void CompatRefreshJob::start(const QString &stagingDir) {
  if (isRunning()) {
    return;
  }
  m_log.reset(QStringLiteral("QindaLutris compatibility information refresh"));
  m_stagingDir = QDir::cleanPath(stagingDir);
  m_stage = Stage::Checksum;
  if (!QDir::isAbsolutePath(m_stagingDir) || !QDir().mkpath(m_stagingDir)) {
    conclude(false, QStringLiteral("QindaLutris could not prepare a folder for the download."));
    return;
  }
  const QString target = QDir(m_stagingDir).filePath(checksumName());
  QFile::remove(target);
  m_log.append(QStringLiteral("checksum: %1").arg(compatDbChecksumUrl().toString()));
  m_downloader->start(compatDbChecksumUrl(), target);
}

void CompatRefreshJob::cancel() {
  if (isRunning()) {
    m_downloader->cancel(); // no finished() follows a cancel (Downloader contract)
    conclude(false, QStringLiteral("Checking for newer information was stopped."));
  }
}

void CompatRefreshJob::onDownloadFinished(bool ok, const QString &reason) {
  if (m_stage == Stage::Idle) {
    return;
  }
  if (!ok) {
    m_log.append(QStringLiteral("download failed: %1").arg(reason));
    conclude(false, QStringLiteral("Newer information could not be downloaded. Check your "
                                   "internet connection and try again."));
    return;
  }
  const QDir dir(m_stagingDir);
  if (m_stage == Stage::Checksum) {
    m_stage = Stage::Document;
    const QString target = dir.filePath(QString::fromLatin1(kCompatDbFileName));
    QFile::remove(target);
    m_log.append(QStringLiteral("document: %1").arg(compatDbRefreshUrl().toString()));
    m_downloader->start(compatDbRefreshUrl(), target);
    return;
  }
  QFile sums(dir.filePath(checksumName()));
  const QString document = dir.filePath(QString::fromLatin1(kCompatDbFileName));
  const std::optional<QByteArray> expected =
      sums.open(QIODevice::ReadOnly) && sums.size() < 4096
          ? parseSha512SumFile(sums.readAll(), QString::fromLatin1(kCompatDbFileName))
          : std::nullopt;
  const std::optional<QByteArray> actual = sha512HexOfFile(document);
  if (!expected || !actual || *expected != *actual ||
      QFileInfo(document).size() > kMaxDocumentBytes) {
    m_log.append(QStringLiteral("checksum mismatch or unreadable"));
    QFile::remove(document);
    conclude(false, QStringLiteral("The downloaded information was damaged; nothing changed."));
    return;
  }
  m_log.append(QStringLiteral("verified sha512 %1").arg(QString::fromLatin1(*actual)));
  m_stage = Stage::Idle;
  CompatRefreshResult result;
  result.ok = true;
  result.stagedPath = document;
  Q_EMIT finished(result);
}

void CompatRefreshJob::conclude(bool ok, const QString &message) {
  m_stage = Stage::Idle;
  m_log.append(message);
  CompatRefreshResult result;
  result.ok = ok;
  result.message = message;
  Q_EMIT finished(result);
}

} // namespace QindaQt::QindaLutris
