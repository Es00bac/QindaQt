// SPDX-License-Identifier: GPL-3.0-or-later
#include "remote_copy_to_controller.h"
#include "network_location.h"
#include "remote_copier.h"

#include <algorithm>

namespace QindaQt::Apps::FileManager {

RemoteCopyToController::RemoteCopyToController(RemoteCopier &copier, QObject *parent)
    : QObject(parent), m_copier(&copier) {
  connect(&copier, &RemoteCopier::copyFinished, this,
          [this](quint64 generation, const QString &diagnostic) {
            onCopyFinished(generation, diagnostic);
          });
}

bool RemoteCopyToController::requestCopy(const QVector<DirectoryEntry> &listedEntries,
                                         const QUrl &remoteUrl, quint64 listingGeneration,
                                         const QString &sourcePath,
                                         const QString &destinationFolder) {
  const auto refuse = [this](const QString &message) {
    Q_EMIT failure(message);
    return false;
  };
  if (m_busy) {
    return refuse(QStringLiteral("A remote copy is already in progress"));
  }
  // The source must be a currently listed immediate child of the active
  // remote directory: this rejects unlisted/stale identities and
  // cross-folder sources before any dispatch.
  const auto listed =
      std::find_if(listedEntries.begin(), listedEntries.end(),
                   [&sourcePath](const DirectoryEntry &entry) {
                     return entry.absolutePath == sourcePath;
                   });
  if (listed == listedEntries.end()) {
    return refuse(QStringLiteral("The item is no longer listed in this folder"));
  }
  const QUrl source(sourcePath);
  if (!NetworkLocation::isSupportedScheme(source) || !source.userName().isEmpty() ||
      !source.password().isEmpty() || source.scheme() != remoteUrl.scheme() ||
      source.host() != remoteUrl.host() || source.port() != remoteUrl.port() ||
      NetworkLocation::parentOf(source) != remoteUrl) {
    return refuse(QStringLiteral("Copy must start from the current folder"));
  }
  // The destination must canonicalize to a sibling smb/sftp folder without
  // userinfo -- anything else (local paths, other schemes, malformed URLs,
  // embedded credentials) is refused before any KIO contact.
  const auto destination = NetworkLocation::canonicalize(destinationFolder);
  if (!destination || !NetworkLocation::isSupportedScheme(*destination) ||
      !destination->userName().isEmpty() || !destination->password().isEmpty() ||
      destination->scheme() != remoteUrl.scheme() ||
      destination->host() != remoteUrl.host() ||
      destination->port() != remoteUrl.port()) {
    return refuse(QStringLiteral("Choose a valid network destination folder"));
  }
  const QUrl target = NetworkLocation::childUrl(*destination, listed->name);
  if (target == source) {
    return refuse(QStringLiteral("The destination is the item itself"));
  }
  // AGENT-GUARD: copying a directory into itself or one of its own
  // descendants would recurse forever; KIO must never see such a job.
  if (listed->isDirectory &&
      (destination->path() == source.path() ||
       destination->path().startsWith(source.path() + QLatin1Char('/')))) {
    return refuse(QStringLiteral("Cannot copy a folder into itself"));
  }
  m_busy = true;
  m_generation = listingGeneration;
  m_destinationFolder = *destination;
  m_currentFolder = remoteUrl;
  Q_EMIT busyChanged();
  m_copier->copy(m_generation, source, target);
  return true;
}

void RemoteCopyToController::onCopyFinished(quint64 generation,
                                            const QString &diagnostic) {
  // Fenced like the rename handler: a result belonging to a cancelled or
  // superseded copy (including the quiet kill that cancellation triggers)
  // is discarded without touching visible state.
  if (!m_busy || generation != m_generation) {
    return;
  }
  const QUrl destination = m_destinationFolder;
  const QUrl currentFolder = m_currentFolder;
  m_busy = false;
  m_generation = 0;
  m_destinationFolder = QUrl();
  m_currentFolder = QUrl();
  Q_EMIT busyChanged();
  if (diagnostic.isEmpty()) {
    // Success: refresh only when the confirmed destination is the folder
    // the user is looking at -- no optimistic display either way.
    if (destination == currentFolder) {
      Q_EMIT refreshRequested();
    }
    return;
  }
  Q_EMIT failure(diagnostic);
}

void RemoteCopyToController::cancelPending() {
  if (!m_busy) {
    return;
  }
  m_copier->cancel(m_generation);
  m_busy = false;
  m_generation = 0;
  m_destinationFolder = QUrl();
  m_currentFolder = QUrl();
  Q_EMIT busyChanged();
}

} // namespace QindaQt::Apps::FileManager
