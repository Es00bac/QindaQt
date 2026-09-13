// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "kio_remote_file_opener.h"

#include <QDBusPendingCall>
#include <QHash>
#include <QUrl>

class QDBusPendingCallWatcher;

namespace QindaQt::Apps::FileManager {

// Production remote write-in-place opener (ADR-0157). Before the platform's
// KIOFuse facility, opening an smb/sftp file handed the raw remote URL to
// KIO::OpenUrlJob (ADR-0152): a KIO-aware handler wrote back through its own
// KIO I/O and every other handler got a temporary download, so saved bytes
// only reached the server incidentally. This opener makes the write-back
// path deterministic: it asks the session's standard KIOFuse service
// (org.kde.KIOFuse.VFS.mountUrl, D-Bus activated like KIOGui's own OpenUrlJob
// integration) to expose the SAME canonical remote URL as a local FUSE
// path, then hands that local path to the desktop's default handler through
// the same KIO::OpenUrlJob + standard UI delegate as the direct open. The
// handler edits an ordinary local file; KIOFuse owns the upload lifecycle
// and writes the saved bytes back to the mounted URL on close.
//
// QindaQt still owns no downloader, mount framework, sync engine, or
// credential authority: the kio-fuse daemon's lifetime belongs to the
// platform (D-Bus activation / systemd user unit), and ordinary
// authentication flows through KIO's standard delegate exactly as the
// direct open. When the facility cannot answer (no session bus, mount
// error, or a malformed reply) the opener falls back to the ADR-0152
// direct open, so remote open never regresses where KIOFuse is absent.
//
// Unlike the copy/move controllers, an open is fire-and-forget: there is no
// busy state, no listing refresh, and no shared Cancel owner. Each open()
// is an independent operation keyed by a monotonic identity, so concurrent
// opens and late replies can never alias onto each other; destruction
// (e.g. the NavigationController dying) kills pending watchers and jobs
// quietly, matching the base class contract.
//
// Not final: tst_kio_fuse_remote_file_opener.cpp subclasses this to
// override createMountCall()/canResolve()/createOpenUrlJob(), proving the
// scheme/userinfo boundary, the resolve-then-open flow, the fallback, and
// the retained KIO UI delegate without a session bus, KIOFuse daemon,
// network, or desktop handler.
class KioFuseRemoteFileOpener : public KioRemoteFileOpener {
  Q_OBJECT

public:
  explicit KioFuseRemoteFileOpener(QObject *parent = nullptr);
  ~KioFuseRemoteFileOpener() override;

  void open(const QUrl &url) override;

protected:
  // AGENT-NOTE: test seams. A test subclass overrides these to record the
  // exact URLs open() reached and to answer the mount resolution with
  // canned QDBusPendingCall replies (fromCompletedCall/fromError) instead of
  // contacting the session bus. The production bodies instantiate the real
  // D-Bus call and KIO job.
  [[nodiscard]] virtual bool canResolve() const;
  [[nodiscard]] virtual QDBusPendingCall createMountCall(const QUrl &url) const;

private:
  struct PendingMount final {
    QDBusPendingCallWatcher *watcher = nullptr;
    QUrl remoteUrl;
  };

  void onMountFinished(quint64 operation, QDBusPendingCallWatcher *watcher);
  void openResolvedLocalFile(const QUrl &localUrl);

  // Monotonic per-open identity: incremented for every accepted open() and
  // never reused, so a watcher's late completion can never retire another
  // open's pending state.
  quint64 m_operation = 0;
  QHash<quint64, PendingMount> m_pending;
};

} // namespace QindaQt::Apps::FileManager
