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
  // Issue the fresh operation identity BEFORE dispatch so the mover's
  // result -- and any quiet-kill result after cancel() -- correlates to
  // exactly this request. Never reset m_operation: uniqueness over the
  // controller's whole lifetime is what fences a cancelled job's late
  // result away from a same-listing retry.
  ++m_operation;
  // The listing generation stays the dispatch-time freshness fence (see the
  // header contract); it is deliberately not reused as the request
  // identity.
  Q_UNUSED(listingGeneration);
  Q_EMIT busyChanged();
  m_mover->move(m_operation, source, target);
  return true;
}

void RemoteMoveToController::onMoveFinished(quint64 operation,
                                            const QString &diagnostic) {
  // Fenced by the operation identity, not the listing generation: a result
  // belonging to a cancelled or superseded move (including the quiet kill
  // that cancellation triggers) carries an older identity and is discarded
  // without touching visible state -- even when a retry in the same
  // unchanged listing is already in flight (reviewed P1).
  if (!m_busy || operation != m_operation) {
    return;
  }
  m_busy = false;
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
  m_mover->cancel(m_operation);
  m_busy = false;
  // m_operation is intentionally NOT reset here: the cancelled job's late
  // result keeps its (now stale) identity, and the next accepted move gets
  // a fresh one -- that is the whole token-alias repair.
  Q_EMIT busyChanged();
}

} // namespace QindaQt::Apps::FileManager
