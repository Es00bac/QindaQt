// SPDX-License-Identifier: GPL-3.0-or-later
#include "kio_fuse_remote_file_opener.h"

#include "network_location.h"

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QFileInfo>
#include <KJob>

namespace QindaQt::Apps::FileManager {

namespace {

// AGENT-CONTRACT (ADR-0157): the platform KIOFuse endpoint. KIOGui embeds
// the same client for its own OpenUrlJob kio-fuse routing, so this is the
// supported public surface, not a private protocol. mountUrl returns the
// local FUSE path one remote URL is mounted at.
const auto kKioFuseService = QStringLiteral("org.kde.KIOFuse");
const auto kKioFusePath = QStringLiteral("/org/kde/KIOFuse");
const auto kKioFuseInterface = QStringLiteral("org.kde.KIOFuse.VFS");
const auto kMountMethod = QStringLiteral("mountUrl");

} // namespace

KioFuseRemoteFileOpener::KioFuseRemoteFileOpener(QObject *parent)
    : KioRemoteFileOpener(parent) {}

KioFuseRemoteFileOpener::~KioFuseRemoteFileOpener() = default;

bool KioFuseRemoteFileOpener::canResolve() const {
  return QDBusConnection::sessionBus().isConnected();
}

QDBusPendingCall KioFuseRemoteFileOpener::createMountCall(const QUrl &url) const {
  // QDBusInterface is lazy: constructing it never blocks, and the async call
  // D-Bus-activates the kio-fuse service on demand.
  QDBusInterface vfs(kKioFuseService, kKioFusePath, kKioFuseInterface,
                     QDBusConnection::sessionBus());
  return vfs.asyncCall(kMountMethod, url.toString());
}

void KioFuseRemoteFileOpener::open(const QUrl &url) {
  // AGENT-GUARD: same policy boundary as every remote adapter, checked
  // BEFORE any D-Bus contact: a non-smb/sftp URL or one carrying userinfo
  // never leaves this process, so no credential material can reach the
  // mount call, a job, or a log.
  if (!NetworkLocation::isSupportedScheme(url) || !url.userName().isEmpty() ||
      !url.password().isEmpty()) {
    Q_EMIT openFinished(QStringLiteral("Unsupported network location scheme"));
    return;
  }
  if (!canResolve()) {
    // No session bus: the KIOFuse facility cannot be reached, so open the
    // remote URL directly -- byte-for-byte the pre-ADR-0157 behavior.
    KioRemoteFileOpener::open(url);
    return;
  }
  const quint64 operation = ++m_operation;
  auto *watcher =
      new QDBusPendingCallWatcher(createMountCall(url), this);
  m_pending.insert(operation, PendingMount{watcher, url});
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, operation](QDBusPendingCallWatcher *finished) {
            onMountFinished(operation, finished);
          });
}

void KioFuseRemoteFileOpener::onMountFinished(quint64 operation,
                                              QDBusPendingCallWatcher *watcher) {
  const auto pending = m_pending.find(operation);
  // AGENT-GUARD: a completion for an unknown operation (defensive; watchers
  // are one-shot) must not touch state belonging to a later open().
  if (pending == m_pending.end()) {
    return;
  }
  const QUrl remoteUrl = pending->remoteUrl;
  m_pending.erase(pending);
  // Read the reply before releasing the watcher below.
  const QDBusReply<QString> reply = *watcher;
  watcher->deleteLater();
  if (!reply.isValid()) {
    // The facility answered with an error (service absent, mount refused,
    // ...): fall back to the direct remote open exactly like a missing bus.
    KioRemoteFileOpener::open(remoteUrl);
    return;
  }
  const QString localPath = reply.value();
  // AGENT-GUARD: only an absolute path from the trusted session KIOFuse
  // service is opened. KIOFuse is trusted for this mapping to the same
  // degree as KIOGui's own OpenUrlJob kio-fuse routing (which also hands
  // the handler whatever path the service returns); anything else falls
  // back to the direct open instead of risking a local-path injection.
  if (localPath.isEmpty() || !QFileInfo(localPath).isAbsolute()) {
    KioRemoteFileOpener::open(remoteUrl);
    return;
  }
  openResolvedLocalFile(QUrl::fromLocalFile(localPath));
}

void KioFuseRemoteFileOpener::openResolvedLocalFile(const QUrl &localUrl) {
  KIO::OpenUrlJob *job = createOpenUrlJob(localUrl);
  if (job == nullptr) {
    Q_EMIT openFinished(QStringLiteral("The network location could not be reached"));
    return;
  }
  // AGENT-CONTRACT (ADR-0151/0152/0157): same delegate rule as the base
  // open -- KIO's standard widgets delegate must stay on the job so the
  // handler's ordinary prompts (e.g. its Open With dialog) keep working;
  // the surrounding process is the QWidget-capable application from
  // runtime/file_manager_application.h. QindaQt installs no custom
  // delegate and never sees a credential.
  if (job->uiDelegate() == nullptr) {
    job->setUiDelegate(KIO::createDefaultJobUiDelegate());
  }
  connect(job, &KJob::result, this, [this](KJob *finished) {
    Q_EMIT openFinished(finished->error() == 0
                            ? QString()
                            : NetworkLocation::boundedDiagnostic(finished->errorString()));
  });
  job->start();
}

} // namespace QindaQt::Apps::FileManager
