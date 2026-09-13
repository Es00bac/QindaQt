// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: Creates exactly one directory at an already-canonical
// smb/sftp child URL through the platform's supported KIO mkdir facility
// (ADR-0154). NavigationController -- not this interface -- validates the
// name, proves the target parent is the currently active remote directory,
// fences the result by generation, and refreshes the authoritative listing
// on success; this collaborator never creates outside the active folder,
// never persists anything, and an implementation must:
//  - never persist a credential (no wallet/keyring/session write), and never
//    read, log, or accept embedded URL userinfo;
//  - never present handler/UI choices or mutate anything besides the one
//    requested directory (no copy, write, rename, or Trash of other items);
//  - treat cancel() of an unknown or finished generation as a no-op, kill a
//    cancelled job quietly, and emit createFinished at most once per
//    accepted createFolder() call, on the GUI thread, never synchronously
//    reentering the caller before createFolder() returns.
class RemoteFolderCreator : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~RemoteFolderCreator() override = default;

  // Starts creating the directory at url. NavigationController passes only
  // canonical smb/sftp child URLs of the currently active remote folder; an
  // implementation still re-validates the scheme itself (defense in depth,
  // mirroring KioNetworkDirectoryBackend).
  virtual void createFolder(quint64 generation, const QUrl &url) = 0;
  // Best-effort cancellation of a still-pending generation; a no-op for an
  // unknown or already-completed generation.
  virtual void cancel(quint64 generation) = 0;

signals:
  // Emitted exactly once per createFolder() that reached the facility: an
  // empty diagnostic means success; a bounded human-readable message means
  // a typed failure or cancellation.
  void createFinished(quint64 generation, QString diagnostic);
};

using RemoteFolderCreatorPtr = std::unique_ptr<RemoteFolderCreator>;

} // namespace QindaQt::Apps::FileManager
