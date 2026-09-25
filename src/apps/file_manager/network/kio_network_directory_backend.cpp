// SPDX-License-Identifier: GPL-3.0-or-later
#include "kio_network_directory_backend.h"
#include "network_location.h"

#include <KFileItem>
#include <KIO/Job>
#include <KIO/ListJob>
#include <KJob>

namespace QindaQt::Apps::FileManager {

namespace {

[[nodiscard]] NetworkListingError errorFor(int kioErrorCode) {
  switch (kioErrorCode) {
  case 0:
    return NetworkListingError::None;
  case KIO::ERR_DOES_NOT_EXIST:
    return NetworkListingError::NotFound;
  case KIO::ERR_ACCESS_DENIED:
  case KIO::ERR_WRITE_ACCESS_DENIED:
    return NetworkListingError::PermissionDenied;
  case KIO::ERR_CANNOT_LOGIN:
  case KIO::ERR_CANNOT_AUTHENTICATE:
    return NetworkListingError::AuthenticationRequired;
  case KIO::ERR_CANNOT_CONNECT:
  case KIO::ERR_UNKNOWN_HOST:
  case KIO::ERR_SERVER_TIMEOUT:
  case KIO::ERR_CONNECTION_BROKEN:
    return NetworkListingError::Transport;
  case KIO::ERR_UNSUPPORTED_PROTOCOL:
  case KIO::ERR_NO_SOURCE_PROTOCOL:
    return NetworkListingError::Unavailable;
  default:
    return NetworkListingError::Unknown;
  }
}

[[nodiscard]] DirectoryEntry entryFor(const KIO::UDSEntry &uds, const QUrl &directoryUrl) {
  const KFileItem item(uds, directoryUrl, /*delayedMimeTypes=*/true,
                       /*urlIsDirectory=*/true);
  DirectoryEntry entry;
  entry.name = item.name();
  entry.absolutePath = NetworkLocation::childUrl(directoryUrl, entry.name).toString();
  entry.isDirectory = item.isDir();
  entry.isSymlink = item.isLink();
  entry.isHidden = entry.name.startsWith(QLatin1Char('.'));
  entry.isReadable = true;
  entry.size = item.isDir() ? 0 : static_cast<qint64>(item.size());
  entry.lastModified = item.time(KFileItem::ModificationTime);
  // ADR-0270: the server's own times, when it reports them; owner and group
  // stay unknown because KIO names them rather than numbering them.
  entry.created = item.time(KFileItem::CreationTime);
  entry.accessed = item.time(KFileItem::AccessTime);
  // AGENT-GUARD: identity fields (device/inode/size/mtime-ns/mode) stay at
  // their zero default. Remote entries carry no local mutation identity --
  // MutationController's identity check would refuse them anyway, and
  // NavigationController disables every local-only action while a remote
  // folder is active, but zeroing here is the second, independent guard.
  return entry;
}

} // namespace

KioNetworkDirectoryBackend::KioNetworkDirectoryBackend(QObject *parent)
    : NetworkDirectoryBackend(parent) {}

KioNetworkDirectoryBackend::~KioNetworkDirectoryBackend() {
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it->job != nullptr) {
      it->job->kill(KJob::Quietly);
    }
  }
}

KIO::ListJob *KioNetworkDirectoryBackend::createListJob(const QUrl &url) const {
  return KIO::listDir(url, KIO::HideProgressInfo);
}

void KioNetworkDirectoryBackend::requestListing(quint64 generation, const QUrl &url) {
  // AGENT-GUARD: the policy boundary is enforced here, before any KIO job
  // exists. An unsupported/malformed location never reaches KIO, and this
  // check is independent of NavigationController's own canonicalization --
  // see the reuse-check note in tst_kio_network_directory_backend.cpp.
  if (!NetworkLocation::isSupportedScheme(url)) {
    NetworkListingResult rejection;
    rejection.url = url;
    rejection.error = NetworkListingError::Unavailable;
    rejection.diagnostic = QStringLiteral("Unsupported network location scheme");
    Q_EMIT listingReady(generation, url, rejection);
    return;
  }
  KIO::ListJob *job = createListJob(url);
  if (job == nullptr) {
    NetworkListingResult unavailable;
    unavailable.url = url;
    unavailable.error = NetworkListingError::Unavailable;
    unavailable.diagnostic = QStringLiteral("The network location could not be reached");
    Q_EMIT listingReady(generation, url, unavailable);
    return;
  }
  // AGENT-CONTRACT (ADR-0151): a supported job keeps the platform's standard
  // KIO UI delegate and delegate extension. A kioslave that needs
  // credentials or a mount therefore shows KIO's normal prompt instead of
  // failing with ERR_CANNOT_LOGIN; cancelled/failed prompts still surface as
  // the typed AuthenticationRequired result below. QindaQt never installs a
  // custom delegate, so it never reads, stores, or owns a credential -- and
  // destruction/cancel() above still kills the job quietly, taking any open
  // prompt down with it.
  PendingRequest &pending = m_pending[generation];
  pending.job = job;
  pending.url = url;

  connect(job, &KIO::ListJob::entries, this,
          [this, generation](KIO::Job *, const KIO::UDSEntryList &list) {
            const auto it = m_pending.find(generation);
            if (it == m_pending.end()) {
              return;
            }
            PendingRequest &request = it.value();
            if (request.truncated) {
              return;
            }
            for (const KIO::UDSEntry &uds : list) {
              const QString name = uds.stringValue(KIO::UDSEntry::UDS_NAME);
              if (name == QLatin1String(".") || name == QLatin1String("..")) {
                continue;
              }
              if (request.entries.size() >= NetworkLocation::maximumEntries) {
                request.truncated = true;
                request.job->kill(KJob::Quietly);
                request.job = nullptr;
                finish(generation, NetworkListingError::None, {});
                return;
              }
              request.entries.append(entryFor(uds, request.url));
            }
          });
  connect(job, &KJob::result, this, [this, generation](KJob *finished) {
    finish(generation, errorFor(finished->error()),
          NetworkLocation::boundedDiagnostic(finished->errorString()));
  });
}

void KioNetworkDirectoryBackend::cancel(quint64 generation) {
  const auto it = m_pending.find(generation);
  if (it == m_pending.end()) {
    return;
  }
  if (it->job != nullptr) {
    it->job->kill(KJob::Quietly);
  }
  m_pending.erase(it);
}

void KioNetworkDirectoryBackend::finish(quint64 generation, NetworkListingError error,
                                       const QString &diagnostic) {
  const auto it = m_pending.find(generation);
  if (it == m_pending.end()) {
    return;
  }
  NetworkListingResult result;
  result.url = it->url;
  result.entries = it->entries;
  result.truncated = it->truncated;
  result.error = error;
  result.diagnostic = diagnostic;
  m_pending.erase(it);
  Q_EMIT listingReady(generation, result.url, result);
}

} // namespace QindaQt::Apps::FileManager
