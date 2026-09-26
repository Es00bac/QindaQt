// SPDX-License-Identifier: GPL-3.0-or-later
#include "compat_refresher.h"

#include "compat_refresh_job.h"
#include "network_downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace QindaQt::QindaLutris {

CompatRefresher::CompatRefresher(CompatDatabase *database, QString refreshedPath,
                                 std::unique_ptr<Downloader> downloader, QObject *parent)
    : QObject(parent), m_database(database), m_refreshedPath(std::move(refreshedPath)),
      m_downloader(downloader ? std::move(downloader) : std::make_unique<NetworkDownloader>()) {
  m_job = new CompatRefreshJob(m_downloader.get(), this);
  connect(m_job, &CompatRefreshJob::finished, this, &CompatRefresher::onFinished);
}

// AGENT-NOTE: the job is a child that borrows the downloader; delete it
// first so a running download is cancelled through a live downloader.
CompatRefresher::~CompatRefresher() { delete m_job; }

QString CompatRefresher::dateText() const {
  return m_database->isLoaded() ? m_database->generated().date().toString(Qt::ISODate)
                                : QString();
}

int CompatRefresher::gameCount() const {
  return static_cast<int>(m_database->games().size());
}

bool CompatRefresher::busy() const { return m_job->isRunning(); }

void CompatRefresher::checkForNewer() {
  if (m_job->isRunning()) {
    return;
  }
  m_message.clear();
  m_job->start(QFileInfo(m_refreshedPath).absolutePath() + QStringLiteral("/refresh-staging"));
  Q_EMIT changed();
}

void CompatRefresher::onFinished(const CompatRefreshResult &result) {
  if (!result.ok) {
    m_message = result.message;
    Q_EMIT changed();
    return;
  }
  CompatLoadError error = CompatLoadError::None;
  const CompatDatabase downloaded = loadCompatDatabase(result.stagedPath, &error);
  const CompatDatabase newer = chooseNewer(*m_database, downloaded);
  if (!downloaded.isLoaded()) {
    m_message = QStringLiteral("The downloaded information could not be read; nothing changed.");
  } else if (newer.generated() != downloaded.generated() ||
             downloaded.generated() == m_database->generated()) {
    m_message = QStringLiteral("You already have the newest information.");
  } else {
    QFile staged(result.stagedPath);
    QSaveFile out(m_refreshedPath);
    QDir().mkpath(QFileInfo(m_refreshedPath).absolutePath());
    if (!staged.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly) ||
        out.write(staged.readAll()) < 0 || !out.commit()) {
      m_message = QStringLiteral("The newer information could not be saved; nothing changed.");
    } else {
      *m_database = downloaded;
      m_message = QStringLiteral("Updated to information from %1.").arg(dateText());
    }
  }
  QFile::remove(result.stagedPath);
  Q_EMIT changed();
}

} // namespace QindaQt::QindaLutris
