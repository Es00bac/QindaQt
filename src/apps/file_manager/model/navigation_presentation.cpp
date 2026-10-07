// SPDX-License-Identifier: GPL-3.0-or-later
#include "navigation_controller.h"
#include "../network/network_location.h"
#include "entry_presentation.h"
#include "preview/local_preview.h"
#include <QDate>
#include <algorithm>
#include <array>
#include <iterator>
namespace QindaQt::Apps::FileManager {
namespace { constexpr std::array iconSizes{16, 24, 32, 48, 64, 96, 128}; }

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
  static const QStringList modes{QStringLiteral("list"), QStringLiteral("grid"),
                                 QStringLiteral("columns"), QStringLiteral("gallery")};
  if (!modes.contains(mode) || m_viewMode == mode) {
    return;
  }
  m_viewMode = mode;
  emit presentationChanged();
}

void NavigationController::setGroupBy(const QString &groupKey) {
  bool ok = false;
  const EntryGroup group = entryGroupFromKey(groupKey, &ok);
  if (!ok || m_order.group == group) {
    return;
  }
  m_order.group = group;
  rebuildVisibleEntries();
  emit presentationChanged();
  emit entriesChanged();
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
  const QDate today = QDate::currentDate();
  return EntryPresentation::entryListToVariants(
      m_entries, m_listingGeneration,
      [this](const DirectoryEntry &entry) { return entryIconName(entry); },
      [this](const DirectoryEntry &entry, quint64 generation) {
        return previewUrl(entry, generation);
      },
      [this, &today](const DirectoryEntry &entry) {
        return entryGroupFor(entry, m_order.group, today).label;
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

} // namespace QindaQt::Apps::FileManager
