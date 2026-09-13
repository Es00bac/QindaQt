// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: Performs one same-folder rename of an already-canonical
// smb/sftp child URL to a validated sibling URL through the platform's
// supported KIO rename facility (ADR-0153). NavigationController -- not this
// interface -- validates the name, proves the source is a currently listed
// child of the active remote directory, fences the result by generation,
// and refreshes the authoritative listing on success; this collaborator
// never renames across directories, never persists anything, and an
// implementation must:
//  - never persist a credential (no wallet/keyring/session write), and never
//    read, log, or accept embedded URL userinfo;
//  - never present handler/UI choices or mutate anything besides the one
//    requested rename (no copy, create, write, Trash, or permissions);
//  - treat cancel() of an unknown or finished generation as a no-op, kill a
//    cancelled job quietly, and emit renameFinished at most once per
//    accepted rename() call, on the GUI thread, never synchronously
//    reentering the caller before rename() returns.
class RemoteRenamer : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~RemoteRenamer() override = default;

  // Starts renaming source to destination. NavigationController passes only
  // canonical smb/sftp URLs whose scheme, authority, and parent directory
  // match the currently active remote folder; an implementation still
  // re-validates that boundary itself (defense in depth).
  virtual void rename(quint64 generation, const QUrl &source, const QUrl &destination) = 0;
  // Best-effort cancellation of a still-pending generation; a no-op for an
  // unknown or already-completed generation.
  virtual void cancel(quint64 generation) = 0;

signals:
  // Emitted exactly once per rename() that reached the facility: an empty
  // diagnostic means success; a bounded human-readable message means a typed
  // failure or cancellation.
  void renameFinished(quint64 generation, QString diagnostic);
};

using RemoteRenamerPtr = std::unique_ptr<RemoteRenamer>;

} // namespace QindaQt::Apps::FileManager
