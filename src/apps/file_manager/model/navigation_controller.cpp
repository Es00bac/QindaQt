// SPDX-License-Identifier: GPL-3.0-or-later
#include "navigation_controller.h"
#include "../network/network_location.h"
#include "entry_presentation.h"
#include "preview/local_preview.h"

#include <QDir>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <iterator>

namespace QindaQt::Apps::FileManager {

namespace {

constexpr std::array iconSizes{16, 24, 32, 48, 64, 96, 128};

} // namespace

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
  if (NetworkLocation::classify(path) == LocationScheme::Local) {
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
  reload(true);
  emit navigationChanged();
}

void NavigationController::goForward() {
  if (!m_history.goForward()) {
    return;
  }
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

int NavigationController::indexOfName(const QString &name) const {
  for (qsizetype i = 0; i < m_entries.size(); ++i) {
    if (m_entries.at(i).name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void NavigationController::setSortColumn(const QString &columnKey) {
  bool ok = false;
  const SortColumn column = sortColumnFromKey(columnKey, &ok);
  if (!ok) {
    return;
  }
  if (m_order.column == column) {
    m_order.direction = m_order.direction == SortDirection::Ascending
                            ? SortDirection::Descending
                            : SortDirection::Ascending;
  } else {
    m_order.column = column;
    m_order.direction = SortDirection::Ascending;
  }
  rebuildVisibleEntries();
  emit presentationChanged();
  emit entriesChanged();
}

void NavigationController::setShowHidden(bool showHidden) {
  if (m_showHidden == showHidden) {
    return;
  }
  m_showHidden = showHidden;
  rebuildVisibleEntries();
  emit presentationChanged();
  emit entriesChanged();
}

void NavigationController::setDirectoriesFirst(bool directoriesFirst) {
  if (m_order.directoriesFirst == directoriesFirst) {
    return;
  }
  m_order.directoriesFirst = directoriesFirst;
  rebuildVisibleEntries();
  emit presentationChanged();
  emit entriesChanged();
}

void NavigationController::setViewMode(const QString &mode) {
  if (mode != QStringLiteral("list") && mode != QStringLiteral("grid")) {
    return;
  }
  if (m_viewMode == mode) {
    return;
  }
  m_viewMode = mode;
  emit presentationChanged();
}

void NavigationController::setNameFilter(const QString &filter) {
  QString bounded = filter.left(maximumNameFilterLength);
  if (bounded.size() < filter.size() && !bounded.isEmpty() &&
      bounded.back().isHighSurrogate() && filter.at(bounded.size()).isLowSurrogate()) {
    bounded.chop(1);
  }
  if (m_nameFilter == bounded) {
    return;
  }
  m_nameFilter = bounded;
  rebuildVisibleEntries();
  emit presentationChanged();
  emit entriesChanged();
}

void NavigationController::zoomBy(int steps) {
  const int next =
      std::clamp(m_iconSizeIndex + std::clamp(steps, -6, 6), 0,
                 static_cast<int>(std::ssize(iconSizes)) - 1);
  if (next == m_iconSizeIndex) {
    return;
  }
  m_iconSizeIndex = next;
  emit presentationChanged();
}

void NavigationController::resetZoom() { zoomBy(4 - m_iconSizeIndex); }

QString NavigationController::currentPath() const {
  return m_history.currentPath();
}

bool NavigationController::canGoBack() const { return m_history.canGoBack(); }

bool NavigationController::canGoForward() const {
  return m_history.canGoForward();
}

bool NavigationController::canGoUp() const {
  if (!m_history.hasCurrent()) {
    return false;
  }
  if (m_remoteActive) {
    return NetworkLocation::parentOf(m_remoteUrl).has_value();
  }
  return NavigationHistory::parentOf(m_history.currentPath()).has_value();
}

QVariantList NavigationController::breadcrumb() const {
  return NavigationPresentation::breadcrumbVariants(m_remoteActive, m_remoteUrl,
                                                      m_history.hasCurrent(),
                                                      m_history.currentPath());
}

QString NavigationController::statusKey() const {
  return NavigationPresentation::statusKeyFor(m_status);
}

QString NavigationController::statusMessage() const { return m_statusMessage; }

QVariantList NavigationController::entries() const {
  // Marshalling lives in EntryPresentation (model/entry_presentation.h) so
  // this controller stays under the project's source-size invariant; the
  // identity-field AGENT-GUARD moved with it.
  return EntryPresentation::entryListToVariants(
      m_entries, m_listingGeneration,
      [this](const DirectoryEntry &entry) { return entryIconName(entry); },
      [this](const DirectoryEntry &entry, quint64 generation) {
        return previewUrl(entry, generation);
      });
}

QString NavigationController::launchError() const { return m_launchError; }

QString NavigationController::sortColumn() const {
  return sortColumnKey(m_order.column);
}

QString NavigationController::sortDirection() const {
  return m_order.direction == SortDirection::Ascending
             ? QStringLiteral("ascending")
             : QStringLiteral("descending");
}

bool NavigationController::directoriesFirst() const {
  return m_order.directoriesFirst;
}

bool NavigationController::showHidden() const { return m_showHidden; }

QString NavigationController::viewMode() const { return m_viewMode; }

QString NavigationController::nameFilter() const { return m_nameFilter; }

int NavigationController::iconSize() const {
  return iconSizes.at(static_cast<std::size_t>(m_iconSizeIndex));
}

bool NavigationController::canZoomIn() const {
  return m_iconSizeIndex < static_cast<int>(std::ssize(iconSizes)) - 1;
}

bool NavigationController::canZoomOut() const { return m_iconSizeIndex > 0; }

int NavigationController::entryCount() const {
  return static_cast<int>(m_entries.size());
}

const DirectoryEntry *NavigationController::entryAt(int index) const {
  if (index < 0 || index >= m_entries.size()) {
    return nullptr;
  }
  return &m_entries.at(index);
}

NavigationStatus NavigationController::status() const { return m_status; }

void NavigationController::showGuestListing(
    const QVector<DirectoryEntry> &entries, const QString &statusText) {
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
  std::sort(m_entries.begin(), m_entries.end(),
            [this](const DirectoryEntry &a, const DirectoryEntry &b) {
              return listingEntryLessThan(a, b, m_order);
            });
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
  QStringList notices;
  if (!m_nameFilter.isEmpty() && m_status == NavigationStatus::Ready) {
    notices.append(m_entries.isEmpty()
                       ? QStringLiteral("No matching items")
                       : m_entries.size() == 1
                             ? QStringLiteral("1 matching item")
                             : QStringLiteral("%1 matching items").arg(m_entries.size()));
  }
  if (m_truncated) {
    notices.append(
        QStringLiteral("Showing the first %1 entries").arg(m_listedEntries.size()));
  }
  if (m_hiddenFilteredCount > 0) {
    notices.append(QStringLiteral("%1 hidden").arg(m_hiddenFilteredCount));
  }
  m_statusMessage = notices.join(QStringLiteral("; "));
}

} // namespace QindaQt::Apps::FileManager
