// SPDX-License-Identifier: GPL-3.0-or-later
#include "remote_move_to_controller.h"
#include "network_location.h"
#include "remote_mover.h"

#include <algorithm>

namespace QindaQt::Apps::FileManager {

RemoteMoveToController::RemoteMoveToController(RemoteMover &mover, QObject *parent)
    : QObject(parent), m_mover(&mover) {
  connect(&mover, &RemoteMover::moveFinished, this,
          [this](quint64 generation, const QString &diagnostic) {
            onMoveFinished(generation, diagnostic);
          });
}

bool RemoteMoveToController::requestMove(const QVector<DirectoryEntry> &listedEntries,
                                         const QUrl &remoteUrl, quint64 listingGeneration,
                                         const QString &sourcePath,
                                         const QString &destinationFolder) {
  const auto refuse = [this](const QString &message) {
    Q_EMIT failure(message);
    return false;
  };
  if (m_busy) {
    return refuse(QStringLiteral("A remote move is already in progress"));
  }
  // The source must be a currently listed immediate child of the active
  // remote directory: this rejects unlisted/stale identities and
  // cross-folder sources before any dispatch. Unlike a copy, a dispatched
  // move can delete its source, so this check is the last line of defense.
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
    return refuse(QStringLiteral("Move must start from the current folder"));
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
  // AGENT-GUARD: moving a directory into itself or one of its own
  // descendants would recurse forever; KIO must never see such a job.
  if (listed->isDirectory &&
      (destination->path() == source.path() ||
       destination->path().startsWith(source.path() + QLatin1Char('/')))) {
    return refuse(QStringLiteral("Cannot move a folder into itself"));
  }
  m_busy = true;
  m_generation = listingGeneration;
  Q_EMIT busyChanged();
  m_mover->move(m_generation, source, target);
  return true;
}

void RemoteMoveToController::onMoveFinished(quint64 generation,
                                            const QString &diagnostic) {
  // Fenced like the copy handler: a result belonging to a cancelled or
  // superseded move (including the quiet kill that cancellation triggers)
  // is discarded without touching visible state.
  if (!m_busy || generation != m_generation) {
    return;
  }
  m_busy = false;
  m_generation = 0;
  Q_EMIT busyChanged();
  if (diagnostic.isEmpty()) {
    // Success: the source left the folder being viewed, so the caller
    // re-reads the authoritative listing -- no optimistic display either
    // way. (On a mid-move server failure the source may in truth be gone;
    // the refreshed listing is the authoritative answer there too.)
    Q_EMIT refreshRequested();
    return;
  }
  Q_EMIT failure(diagnostic);
}

void RemoteMoveToController::cancelPending() {
  if (!m_busy) {
    return;
  }
  m_mover->cancel(m_generation);
  m_busy = false;
  m_generation = 0;
  Q_EMIT busyChanged();
}

} // namespace QindaQt::Apps::FileManager
