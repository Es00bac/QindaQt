// SPDX-License-Identifier: GPL-3.0-or-later
#include "navigation_controller.h"
#include "../network/network_location.h"
#include "entry_presentation.h"
#include "preview/local_preview.h"

#include <QDate>
#include <QDir>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>

namespace QindaQt::Apps::FileManager {

NavigationController::NavigationController(DirectoryListerPtr lister,
                                           FileLauncherPtr launcher,
                                           NetworkDirectoryBackendPtr networkBackend,
                                           RemoteFileOpenerPtr remoteOpener,
                                           RemoteRenamerPtr remoteRenamer,
                                           RemoteFolderCreatorPtr folderCreator,
                                           RemoteCopierPtr copier,
                                           RemoteMoverPtr mover,
                                           QObject *parent)
    : QObject(parent), m_lister(std::move(lister)),
      m_launcher(std::move(launcher)),
      m_networkBackend(std::move(networkBackend)),
      m_remoteOpener(std::move(remoteOpener)),
      m_remoteRenamer(std::move(remoteRenamer)),
      m_folderCreator(std::move(folderCreator)),
      m_copier(std::move(copier)),
      m_mover(std::move(mover)) {
  Q_ASSERT(m_lister);
  Q_ASSERT(m_launcher);
  if (m_networkBackend) {
    connect(m_networkBackend.get(), &NetworkDirectoryBackend::listingReady, this,
            &NavigationController::onNetworkListingReady);
  }
  if (m_remoteOpener) {
    m_remoteOpen = std::make_unique<RemoteOpenController>(*m_remoteOpener, this);
    connect(m_remoteOpen.get(), &RemoteOpenController::cleared, this, [this] {
      if (!m_launchError.isEmpty()) {
        m_launchError.clear();
        emit launchErrorChanged();
      }
    });
    connect(m_remoteOpen.get(), &RemoteOpenController::failure, this,
            [this](const QString &message) {
              m_launchError = message;
              emit launchErrorChanged();
            });
  }
  if (m_remoteRenamer) {
    m_remoteRename = std::make_unique<RemoteRenameController>(*m_remoteRenamer, this);
    connect(m_remoteRename.get(), &RemoteRenameController::busyChanged, this,
            [this] { emit remoteRenameChanged(); });
    connect(m_remoteRename.get(), &RemoteRenameController::refreshRequested, this,
            &NavigationController::onRemoteOperationRefreshRequested);
    connect(m_remoteRename.get(), &RemoteRenameController::failure, this,
            &NavigationController::onRemoteOperationFailed);
  }
  if (m_folderCreator) {
    m_remoteCreate = std::make_unique<RemoteCreateFolderController>(*m_folderCreator, this);
    connect(m_remoteCreate.get(), &RemoteCreateFolderController::busyChanged, this,
            [this] { emit remoteCreateChanged(); });
    connect(m_remoteCreate.get(), &RemoteCreateFolderController::refreshRequested, this,
            &NavigationController::onRemoteOperationRefreshRequested);
    connect(m_remoteCreate.get(), &RemoteCreateFolderController::failure, this,
            &NavigationController::onRemoteOperationFailed);
  }
  if (m_copier) {
    m_remoteCopy = std::make_unique<RemoteCopyToController>(*m_copier, this);
    connect(m_remoteCopy.get(), &RemoteCopyToController::busyChanged, this,
            [this] { emit remoteCopyChanged(); });
    connect(m_remoteCopy.get(), &RemoteCopyToController::refreshRequested, this,
            &NavigationController::onRemoteOperationRefreshRequested);
    connect(m_remoteCopy.get(), &RemoteCopyToController::failure, this,
            &NavigationController::onRemoteOperationFailed);
  }
  if (m_mover) {
    m_remoteMove = std::make_unique<RemoteMoveToController>(*m_mover, this);
    connect(m_remoteMove.get(), &RemoteMoveToController::busyChanged, this,
            [this] { emit remoteMoveChanged(); });
    connect(m_remoteMove.get(), &RemoteMoveToController::refreshRequested, this,
            &NavigationController::onRemoteOperationRefreshRequested);
    connect(m_remoteMove.get(), &RemoteMoveToController::failure, this,
            &NavigationController::onRemoteOperationFailed);
  }
}

void NavigationController::navigateTo(const QString &path) {
  const bool wasRevoked = m_mediaLocationRevoked;
  if (NetworkLocation::classify(path) == LocationScheme::Local) {
    clearMediaRevocation();
    if (m_remoteActive) {
      cancelPendingRemoteRename();
      cancelPendingRemoteCreate();
      cancelRemoteCopy();
      cancelRemoteMove();
      if (m_networkBackend) {
        m_networkBackend->cancel(m_listingGeneration);
      }
      m_remoteActive = false;
      emit remoteRenameChanged();
      emit remoteCreateChanged();
      emit remoteCopyChanged();
      emit remoteMoveChanged();
    }
    const QString normalized = QDir::cleanPath(path);
    if (!m_history.hasCurrent()) {
      m_history.reset(normalized);
      reload(true);
      emit navigationChanged();
      return;
    }
    if (!m_history.navigateTo(normalized)) {
      if (wasRevoked) { reload(true); emit navigationChanged(); emit presentationChanged(); }
      return; // already there: no reload, no history churn
    }
    reload(true);
    emit navigationChanged();
    return;
  }

  const auto canonical = NetworkLocation::canonicalize(path);
  if (!canonical) {
    // Malformed/refused location (bad host, embedded credentials, a ".."
    // escape attempt): no navigation, no history churn.
    return;
  }
  clearMediaRevocation();
  const QString locationText = canonical->toString();
  if (!m_history.hasCurrent()) {
    m_history.reset(locationText);
    enterRemote(*canonical);
    emit navigationChanged();
    return;
  }
  if (!m_history.navigateTo(locationText)) {
    return; // already there
  }
  enterRemote(*canonical);
  emit navigationChanged();
}

void NavigationController::goBack() {
  if (!m_history.goBack()) {
    return;
  }
  clearMediaRevocation();
  reload(true);
  emit navigationChanged();
}

void NavigationController::goForward() {
  if (!m_history.goForward()) {
    return;
  }
  clearMediaRevocation();
  reload(true);
  emit navigationChanged();
}

void NavigationController::goUp() {
  if (!m_history.hasCurrent()) {
    return;
  }
  if (m_remoteActive) {
    const auto parent = NetworkLocation::parentOf(m_remoteUrl);
    if (!parent) {
      return;
    }
    navigateTo(parent->toString());
    return;
  }
  const auto parent = NavigationHistory::parentOf(m_history.currentPath());
  if (!parent) {
    return;
  }
  navigateTo(*parent);
}

void NavigationController::refresh() {
  if (m_mediaLocationRevoked) return;
  if (m_remoteActive) {
    requestRemoteListing();
    return;
  }
  reload();
}

void NavigationController::activate(int index) {
  if (index < 0 || index >= m_entries.size()) {
    return;
  }
  const DirectoryEntry &entry = m_entries.at(index);
  if (entry.isDirectory) {
    navigateTo(entry.absolutePath);
    return;
  }
  if (m_remoteActive) {
    activateRemoteFile(entry);
    return;
  }
  const LaunchResult result = m_launcher->launch(entry.absolutePath);
  if (!result.ok()) {
    m_launchError = result.diagnostic;
    emit launchErrorChanged();
  }
}

// ADR-0152: a remote regular file is handed to the injected RemoteFileOpener
// (production: KIO::OpenUrlJob, i.e. the desktop's default handler after any
// KIO-managed temp download). QindaQt never downloads, executes, or locally
// launches a URL-shaped path itself, and never opens a handler picker.
void NavigationController::activateRemoteFile(const DirectoryEntry &entry) {
  if (m_remoteOpen) {
    m_remoteOpen->open(QUrl(entry.absolutePath));
    return;
  }
  // Truthful disabled state when no opener is injected (the stock app always
  // injects one; tests and foreign compositions may not).
  m_launchError =
      QStringLiteral("Opening files from a network location is not supported yet");
  emit launchErrorChanged();
}

bool NavigationController::renameRemoteEntry(const QString &sourcePath,
                                             const QString &newName) {
  if (!m_remoteActive || !m_remoteRename) {
    m_launchError = QStringLiteral("Remote rename is not available here");
    emit launchErrorChanged();
    return false;
  }
  // Validation, dispatch, result fencing, and job retirement live in
  // RemoteRenameController (see its header); this controller supplies the
  // current folder snapshot and surfaces refresh/failure.
  return m_remoteRename->requestRename(m_listedEntries, m_remoteUrl, m_listingGeneration,
                                       sourcePath, newName);
}

void NavigationController::onRemoteOperationRefreshRequested() {
  // Confirmed success for the visible folder: re-read the authoritative
  // remote listing -- nothing changed optimistically before this point.
  if (m_remoteActive) {
    requestRemoteListing();
  }
}

void NavigationController::onRemoteOperationFailed(const QString &message) {
  m_launchError = message;
  emit launchErrorChanged();
}

void NavigationController::cancelPendingRemoteRename() {
  if (m_remoteRename) {
    m_remoteRename->cancelPending();
  }
}

bool NavigationController::createRemoteFolder(const QString &name) {
  if (!m_remoteActive || !m_remoteCreate) {
    m_launchError = QStringLiteral("Creating folders is not available here");
    emit launchErrorChanged();
    return false;
  }
  // Validation, dispatch, result fencing, and job retirement live in
  // RemoteCreateFolderController (see its header); this controller supplies
  // the active folder and surfaces refresh/failure.
  return m_remoteCreate->requestCreate(m_remoteUrl, m_listingGeneration, name);
}

bool NavigationController::copyRemoteChild(const QString &sourcePath,
                                           const QString &destinationFolder) {
  if (!m_remoteActive || !m_remoteCopy) {
    m_launchError = QStringLiteral("Remote copy is not available here");
    emit launchErrorChanged();
    return false;
  }
  // Validation, dispatch, result fencing, destination-scoped refresh, and
  // job retirement live in RemoteCopyToController (see its header); this
  // controller supplies the current folder snapshot.
  return m_remoteCopy->requestCopy(m_listedEntries, m_remoteUrl, m_listingGeneration,
                                   sourcePath, destinationFolder);
}

void NavigationController::cancelPendingRemoteCreate() {
  if (m_remoteCreate) {
    m_remoteCreate->cancelPending();
  }
}

void NavigationController::cancelRemoteCopy() {
  if (m_remoteCopy) {
    m_remoteCopy->cancelPending();
  }
}

bool NavigationController::moveRemoteChild(const QString &sourcePath,
                                           const QString &destinationFolder) {
  if (!m_remoteActive || !m_remoteMove) {
    m_launchError = QStringLiteral("Remote move is not available here");
    emit launchErrorChanged();
    return false;
  }
  // Validation, dispatch, fencing, and retirement live in RemoteMoveToController.
  return m_remoteMove->requestMove(m_listedEntries, m_remoteUrl, m_listingGeneration,
                                   sourcePath, destinationFolder);
}

void NavigationController::cancelRemoteMove() {
  if (m_remoteMove) {
    m_remoteMove->cancelPending();
  }
}

void NavigationController::clearLaunchError() {
  if (m_launchError.isEmpty()) {
    return;
  }
  m_launchError.clear();
  emit launchErrorChanged();
}

void NavigationController::showGuestListing(
    const QVector<DirectoryEntry> &entries, const QString &statusText) {
  if (m_mediaLocationRevoked) return;
  ++m_listingGeneration;
  m_guestActive = true;
  m_guestStatusText = statusText;
  m_status = NavigationStatus::Ready;
  m_truncated = false;
  m_listedEntries = entries;
  rebuildVisibleEntries();
  emit entriesChanged();
}

void NavigationController::clearGuestListing() {
  if (!m_guestActive) {
    return;
  }
  m_guestActive = false;
  m_guestStatusText.clear();
  if (m_remoteActive) {
    requestRemoteListing();
    return;
  }
  reload();
}

void NavigationController::enterRemote(const QUrl &url) {
  if (m_remoteActive) {
    // Remote-to-remote replacement: retire any in-flight remote operation
    // for the folder being left, like the listing cancellation.
    cancelPendingRemoteRename();
    cancelPendingRemoteCreate();
    cancelRemoteCopy();
    cancelRemoteMove();
  }
  m_guestActive = false;
  m_guestStatusText.clear();
  m_nameFilter.clear();
  m_remoteActive = true;
  m_remoteUrl = url;
  requestRemoteListing();
  emit remoteRenameChanged();
  emit remoteCreateChanged();
  emit remoteCopyChanged();
  emit remoteMoveChanged();
}

void NavigationController::requestRemoteListing() {
  // ADR-0151 / review P1: a superseded remote job can still own KIO's
  // credential prompt, so retire the current pending generation before any
  // replacement request (refresh, remote-to-remote navigation, guest
  // clearance). cancel() of an unknown generation is a documented no-op, so
  // a first remote entry is unaffected.
  if (m_remoteActive && m_networkBackend) {
    m_networkBackend->cancel(m_listingGeneration);
  }
  ++m_listingGeneration;
  m_truncated = false;
  m_listedEntries.clear();
  m_entries.clear();
  m_hiddenFilteredCount = 0;
  if (!m_networkBackend) {
    m_status = NavigationStatus::Unavailable;
    m_statusMessage = QStringLiteral("Network browsing is unavailable");
    emit entriesChanged();
    return;
  }
  m_status = NavigationStatus::Loading;
  m_statusMessage.clear();
  const quint64 generation = m_listingGeneration;
  const QUrl url = m_remoteUrl;
  emit entriesChanged();
  m_networkBackend->requestListing(generation, url);
}

void NavigationController::onNetworkListingReady(quint64 generation, const QUrl &url,
                                                 const NetworkListingResult &result) {
  // Fencing: a stale generation, a URL that no longer matches the current
  // remote location, or a callback arriving after leaving remote browsing
  // altogether is silently discarded. This also protects a callback that
  // races this object's own destruction: the backend is destroyed (and any
  // queued connection severed) before this object finishes tearing down its
  // own members, per Qt's parent/child and unique_ptr destruction order.
  if (!m_remoteActive || generation != m_listingGeneration || url != m_remoteUrl) {
    return;
  }
  m_status = result.ok() ? (result.entries.isEmpty() ? NavigationStatus::Empty
                                                     : NavigationStatus::Ready)
                        : NavigationPresentation::statusForNetworkError(result.error);
  m_truncated = result.ok() && result.truncated;
  m_listedEntries = result.ok() ? result.entries : QVector<DirectoryEntry>{};
  rebuildVisibleEntries();
  if (!result.ok()) {
    m_statusMessage = NetworkLocation::boundedDiagnostic(result.diagnostic);
  }
  emit entriesChanged();
}

void NavigationController::reload(bool resetFilter) {
  if (m_mediaLocationRevoked) return;
  const bool filterChanged = resetFilter && !m_nameFilter.isEmpty();
  if (filterChanged) {
    m_nameFilter.clear();
  }
  m_guestActive = false;
  m_guestStatusText.clear();
  ++m_listingGeneration;
  const ListingResult result = m_lister->list(m_history.currentPath());
  m_status = NavigationPresentation::statusFor(result);
  m_truncated = result.ok() && result.truncated;
  m_listedEntries = result.ok() ? result.entries : QVector<DirectoryEntry>{};
  rebuildVisibleEntries();
  if (!result.ok()) {
    m_statusMessage = result.diagnostic;
  }
  if (filterChanged) {
    emit presentationChanged();
  }
  emit entriesChanged();
}

void NavigationController::rebuildVisibleEntries() {
  m_entries.clear();
  m_entries.reserve(m_listedEntries.size());
  m_hiddenFilteredCount = 0;
  for (const auto &entry : m_listedEntries) {
    if (!m_showHidden && entry.isHidden) {
      ++m_hiddenFilteredCount;
      continue;
    }
    if (!m_nameFilter.isEmpty() &&
        !entry.name.contains(m_nameFilter, Qt::CaseInsensitive)) {
      continue;
    }
    m_entries.append(entry);
  }
  sortListing(m_entries, m_order, QDate::currentDate());
  // AGENT-GUARD: Presentation-only changes must retain a failed listing's
  // diagnostic. No matches is a Ready projection, never an empty directory.
  if (m_status != NavigationStatus::Ready && m_status != NavigationStatus::Empty) {
    return;
  }
  if (m_guestActive) {
    // Guest (search-result) listings carry their producer's status text; the
    // hidden/name-filter/sort projection above still applies unchanged.
    m_statusMessage = m_guestStatusText;
    return;
  }
  m_statusMessage = EntryPresentation::listingNotices(
      !m_nameFilter.isEmpty() && m_status == NavigationStatus::Ready, m_entries.size(),
      m_truncated, m_listedEntries.size(), m_hiddenFilteredCount);
}

} // namespace QindaQt::Apps::FileManager
