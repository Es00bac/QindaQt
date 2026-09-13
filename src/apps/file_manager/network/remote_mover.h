// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: Moves one already-validated source URL to one
// already-validated destination URL through the platform's supported KIO
// move facility (ADR-0156). NavigationController -- not this interface --
// proves the source is an immediate listed child of the current remote
// folder, validates the destination folder, rejects same-target and
// directory self/descendant moves, fences the result by generation, and
// refreshes the listing after every confirmed success (a move always
// removes its source from the folder being viewed). A move is destructive:
// unlike a copy, a completed or half-completed KIO move may leave the
// source deleted at the server, so the pre-dispatch checks in
// RemoteMoveToController are the last line of defense and must never be
// weakened. This collaborator never moves more than the one requested item,
// never persists anything, and an implementation must:
//  - never persist a credential (no wallet/keyring/session write), and never
//    read, log, or accept embedded URL userinfo;
//  - never present handler/UI choices or mutate anything besides the one
//    requested move (no copy, delete, rename, or Trash);
//  - treat cancel() of an unknown or finished generation as a no-op, kill a
//    cancelled job quietly, and emit moveFinished at most once per accepted
//    move() call, on the GUI thread, never synchronously reentering the
//    caller before move() returns.
class RemoteMover : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~RemoteMover() override = default;

  // Starts moving source to destination. NavigationController passes only
  // canonical smb/sftp URLs whose scheme and authority match the active
  // remote folder; an implementation still re-validates that boundary
  // itself (defense in depth, mirroring KioNetworkDirectoryBackend).
  virtual void move(quint64 generation, const QUrl &source, const QUrl &destination) = 0;
  // Best-effort cancellation of a still-pending generation; a no-op for an
  // unknown or already-completed generation.
  virtual void cancel(quint64 generation) = 0;

signals:
  // Emitted exactly once per move() that reached the facility: an empty
  // diagnostic means success; a bounded human-readable message means a
  // typed failure or cancellation.
  void moveFinished(quint64 generation, QString diagnostic);
};

using RemoteMoverPtr = std::unique_ptr<RemoteMover>;

} // namespace QindaQt::Apps::FileManager
