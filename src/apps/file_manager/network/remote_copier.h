// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: Copies one already-validated source URL to one
// already-validated destination URL through the platform's supported KIO
// copy facility (ADR-0155). NavigationController -- not this interface --
// proves the source is an immediate listed child of the current remote
// folder, validates the destination folder, rejects same-target and
// directory self/descendant copies, fences the result by generation, and
// refreshes the listing only when the confirmed destination is the current
// folder. This collaborator never copies more than the one requested item,
// never persists anything, and an implementation must:
//  - never persist a credential (no wallet/keyring/session write), and never
//    read, log, or accept embedded URL userinfo;
//  - never present handler/UI choices or mutate anything besides the one
//    requested copy (no move, delete, rename, or Trash);
//  - treat cancel() of an unknown or finished generation as a no-op, kill a
//    cancelled job quietly, and emit copyFinished at most once per accepted
//    copy() call, on the GUI thread, never synchronously reentering the
//    caller before copy() returns.
class RemoteCopier : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~RemoteCopier() override = default;

  // Starts copying source to destination. NavigationController passes only
  // canonical smb/sftp URLs whose scheme and authority match the active
  // remote folder; an implementation still re-validates that boundary
  // itself (defense in depth, mirroring KioNetworkDirectoryBackend).
  virtual void copy(quint64 generation, const QUrl &source, const QUrl &destination) = 0;
  // Best-effort cancellation of a still-pending generation; a no-op for an
  // unknown or already-completed generation.
  virtual void cancel(quint64 generation) = 0;

signals:
  // Emitted exactly once per copy() that reached the facility: an empty
  // diagnostic means success; a bounded human-readable message means a
  // typed failure or cancellation.
  void copyFinished(quint64 generation, QString diagnostic);
};

using RemoteCopierPtr = std::unique_ptr<RemoteCopier>;

} // namespace QindaQt::Apps::FileManager
