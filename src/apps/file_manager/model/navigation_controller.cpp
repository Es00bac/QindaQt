// SPDX-License-Identifier: GPL-3.0-or-later
#include "navigation_controller.h"
#include "preview/local_preview.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <array>

namespace QindaQt::Apps::FileManager {

namespace {

constexpr std::array iconSizes{32, 48, 64, 96, 128};

[[nodiscard]] NavigationStatus statusFor(const ListingResult &result) {
  if (result.ok()) {
    return result.entries.isEmpty() ? NavigationStatus::Empty
                                    : NavigationStatus::Ready;
  }
  switch (result.error) {
  case ListingError::NotFound:
    return NavigationStatus::Missing;
  case ListingError::PermissionDenied:
    return NavigationStatus::PermissionDenied;
  case ListingError::NotADirectory:
    return NavigationStatus::NotADirectory;
  case ListingError::Unknown:
  case ListingError::None:
    break;
  }
  return NavigationStatus::Error;
}

// Presentation text is produced C++-side so QML delegates stay dumb and the
// formatting rules are unit-testable through the public entries() snapshot.
[[nodiscard]] QString sizeTextFor(const DirectoryEntry &entry) {
  if (entry.isDirectory) {
    return QStringLiteral("—");
  }
  return QLocale().formattedDataSize(entry.size);
}

[[nodiscard]] QString modifiedTextFor(const DirectoryEntry &entry) {
  if (!entry.lastModified.isValid()) {
    return QString();
  }
  return QLocale().toString(entry.lastModified, QLocale::ShortFormat);
}

[[nodiscard]] QString kindTextFor(const DirectoryEntry &entry) {
  if (entry.isDirectory) {
    return QStringLiteral("Folder");
  }
  if (entry.isSymlink) {
    return QStringLiteral("Link");
  }
  const QString suffix = QFileInfo(entry.name).suffix();
  if (suffix.isEmpty()) {
    return QStringLiteral("File");
  }
  return QStringLiteral("%1 File").arg(suffix.toUpper());
}

} // namespace

NavigationController::NavigationController(DirectoryListerPtr lister,
                                           FileLauncherPtr launcher,
                                           QObject *parent)
    : QObject(parent), m_lister(std::move(lister)),
      m_launcher(std::move(launcher)) {
  Q_ASSERT(m_lister);
  Q_ASSERT(m_launcher);
}

void NavigationController::navigateTo(const QString &path) {
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
  const auto parent = NavigationHistory::parentOf(m_history.currentPath());
  if (!parent) {
    return;
  }
  navigateTo(*parent);
}

void NavigationController::refresh() { reload(); }

void NavigationController::activate(int index) {
  if (index < 0 || index >= m_entries.size()) {
    return;
  }
  const DirectoryEntry &entry = m_entries.at(index);
  if (entry.isDirectory) {
    navigateTo(entry.absolutePath);
    return;
  }
  const LaunchResult result = m_launcher->launch(entry.absolutePath);
  if (!result.ok()) {
    m_launchError = result.diagnostic;
    emit launchErrorChanged();
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
  const int next = std::clamp(m_iconSizeIndex + std::clamp(steps, -4, 4), 0, 4);
  if (next == m_iconSizeIndex) {
    return;
  }
  m_iconSizeIndex = next;
  emit presentationChanged();
}

void NavigationController::resetZoom() { zoomBy(2 - m_iconSizeIndex); }

QString NavigationController::currentPath() const {
  return m_history.currentPath();
}

bool NavigationController::canGoBack() const { return m_history.canGoBack(); }

bool NavigationController::canGoForward() const {
  return m_history.canGoForward();
}

bool NavigationController::canGoUp() const {
  return m_history.hasCurrent() &&
         NavigationHistory::parentOf(m_history.currentPath()).has_value();
}

QVariantList NavigationController::breadcrumb() const {
  QVariantList list;
  if (!m_history.hasCurrent()) {
    return list;
  }
  const auto segments = NavigationHistory::breadcrumbFor(m_history.currentPath());
  list.reserve(segments.size());
  for (const auto &segment : segments) {
    list.append(QVariantMap{{QStringLiteral("name"), segment.name},
                            {QStringLiteral("path"), segment.path}});
  }
  return list;
}

QString NavigationController::statusKey() const { return statusKeyFor(m_status); }

QString NavigationController::statusMessage() const { return m_statusMessage; }

QVariantList NavigationController::entries() const {
  QVariantList list;
  list.reserve(m_entries.size());
  for (const auto &entry : m_entries) {
    list.append(QVariantMap{
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("path"), entry.absolutePath},
        {QStringLiteral("isDirectory"), entry.isDirectory},
        {QStringLiteral("isSymlink"), entry.isSymlink},
        {QStringLiteral("isHidden"), entry.isHidden},
        {QStringLiteral("isReadable"), entry.isReadable},
        {QStringLiteral("size"), entry.size},
        {QStringLiteral("modified"), entry.lastModified},
        {QStringLiteral("sizeText"), sizeTextFor(entry)},
        {QStringLiteral("modifiedText"), modifiedTextFor(entry)},
        {QStringLiteral("kindText"), kindTextFor(entry)},
        {QStringLiteral("iconName"), entryIconName(entry)},
        {QStringLiteral("previewUrl"), previewUrl(entry, m_listingGeneration)},
        // AGENT-GUARD: These identity fields cross QVariant -> JavaScript ->
        // QVariant before mutation dispatch. Decimal strings preserve all 64
        // bits; JS Number would round current-epoch nanoseconds and make every
        // UI mutation fail its optimistic identity check (review P1-1).
        {QStringLiteral("device"), QString::number(entry.device)},
        {QStringLiteral("inode"), QString::number(entry.inode)},
        {QStringLiteral("identitySize"), QString::number(entry.identitySize)},
        {QStringLiteral("modifiedNanoseconds"),
         QString::number(entry.modifiedNanoseconds)},
        {QStringLiteral("mode"), QString::number(entry.mode)},
    });
  }
  return list;
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

bool NavigationController::canZoomIn() const { return m_iconSizeIndex < 4; }

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
  reload();
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
  m_status = statusFor(result);
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

QString NavigationController::statusKeyFor(NavigationStatus status) {
  switch (status) {
  case NavigationStatus::Ready:
    return QStringLiteral("ready");
  case NavigationStatus::Empty:
    return QStringLiteral("empty");
  case NavigationStatus::PermissionDenied:
    return QStringLiteral("permission-denied");
  case NavigationStatus::Missing:
    return QStringLiteral("missing");
  case NavigationStatus::NotADirectory:
    return QStringLiteral("not-a-directory");
  case NavigationStatus::Error:
    return QStringLiteral("error");
  }
  return QStringLiteral("error");
}

} // namespace QindaQt::Apps::FileManager
