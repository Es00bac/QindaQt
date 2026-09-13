// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: Opens one already-canonical smb/sftp regular-file URL
// through the platform's supported KIO/desktop open facility (ADR-0152).
// QindaQt owns no downloader, handler picker, or credential authority: KIO
// performs any temporary download and dispatch, and ordinary authentication
// flows through the same KIO UI delegate as network listing (ADR-0151).
// NavigationController, not QML, owns this collaborator and surfaces a
// failure as its typed launchError text; an implementation must:
//  - never persist a credential (no wallet/keyring/session write), and never
//    read, log, or accept embedded URL userinfo;
//  - never choose a custom handler, prompt for one, execute a command line,
//    or mutate remote content -- opening means "hand to the desktop's
//    default handler";
//  - emit openFinished at most once per open() call, on the GUI thread,
//    and never synchronously reenter the caller before open() returns.
class RemoteFileOpener : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~RemoteFileOpener() override = default;

  // Starts opening url. NavigationController passes only
  // NetworkLocation::canonicalize()d smb/sftp child URLs of the current
  // remote folder; an implementation still re-validates the scheme itself
  // (defense in depth, mirroring KioNetworkDirectoryBackend).
  virtual void open(const QUrl &url) = 0;

signals:
  // Emitted exactly once per open() that reached the facility: an empty
  // diagnostic means success (remote content handed to the desktop handler);
  // a bounded human-readable message means a typed failure or cancellation.
  void openFinished(QString diagnostic);
};

using RemoteFileOpenerPtr = std::unique_ptr<RemoteFileOpener>;

} // namespace QindaQt::Apps::FileManager
