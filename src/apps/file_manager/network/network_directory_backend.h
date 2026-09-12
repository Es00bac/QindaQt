// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../model/file_manager_types.h"

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

#include <memory>

namespace QindaQt::Apps::FileManager {

enum class NetworkListingError {
  None,
  // The scheme/location was refused before any remote contact, or the
  // adapter itself is unusable (e.g. the production KIO slave is missing).
  Unavailable,
  AuthenticationRequired,
  PermissionDenied,
  NotFound,
  // DNS/connect/timeout/broken-connection failures.
  Transport,
  Unknown,
};

struct NetworkListingResult final {
  QUrl url;
  QVector<DirectoryEntry> entries;
  // True when NetworkLocation::maximumEntries was hit before the remote
  // listing finished.
  bool truncated = false;
  NetworkListingError error = NetworkListingError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == NetworkListingError::None; }
};

// AGENT-CONTRACT: Desktop-owned/local-only authorities (mutation, Trash,
// preview, recursive search, bounded local launch) never see a
// NetworkDirectoryBackend or a remote DirectoryEntry; NavigationController
// keeps those authorities local-only and disables their UI entry points
// while a remote location is active. An implementation must:
//  - never persist a credential (no wallet/keyring/session write);
//  - never present interactive UI (a dialog, a mount prompt, a portal) --
//    an authentication requirement becomes a typed AuthenticationRequired
//    result instead;
//  - emit listingReady at most once per accepted requestListing() call, on
//    the GUI thread. NavigationController -- not this interface -- fences a
//    stale generation/URL, so an implementation may still emit after
//    cancel() or after a newer requestListing() call; it must simply never
//    crash, block, or emit synchronously in a way that reenters the caller
//    before requestListing() returns (queued or post-return direct emission
//    only).
class NetworkDirectoryBackend : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~NetworkDirectoryBackend() override = default;

  // Starts (or restarts) an asynchronous listing of url, tagged with
  // generation. url must already be NetworkLocation::canonicalize()d; an
  // implementation still re-validates the scheme itself (defense in depth --
  // see KioNetworkDirectoryBackend) rather than trusting the caller.
  virtual void requestListing(quint64 generation, const QUrl &url) = 0;
  // Best-effort cancellation of a still-pending generation; a no-op for an
  // unknown or already-completed generation.
  virtual void cancel(quint64 generation) = 0;

signals:
  void listingReady(quint64 generation, QUrl url,
                    QindaQt::Apps::FileManager::NetworkListingResult result);
};

using NetworkDirectoryBackendPtr = std::unique_ptr<NetworkDirectoryBackend>;

} // namespace QindaQt::Apps::FileManager

Q_DECLARE_METATYPE(QindaQt::Apps::FileManager::NetworkListingResult)
