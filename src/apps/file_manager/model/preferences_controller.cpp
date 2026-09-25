// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences_controller.h"

#include "../network/connect_request.h"

#include <QVariantList>

#include <cmath>
#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] QVariantList columnsToVariant(const QList<DetailsColumn> &columns) {
  QVariantList list;
  list.reserve(columns.size());
  for (const DetailsColumn &column : columns) {
    list.append(QVariantMap{{QStringLiteral("key"), column.key},
                            {QStringLiteral("width"), column.width}});
  }
  return list;
}

// QML hands numbers over as doubles; a fractional width reads as -1 and is
// refused by Preferences::isValid() like any other out-of-range value.
[[nodiscard]] int integerFrom(const QVariant &value) {
  bool ok = false;
  const double number = value.toDouble(&ok);
  if (!ok || !std::isfinite(number) || std::abs(number) > 1.0e6 ||
      number != std::floor(number)) {
    return -1;
  }
  return static_cast<int>(number);
}

[[nodiscard]] QList<DetailsColumn> columnsFromVariant(const QVariantList &list) {
  QList<DetailsColumn> columns;
  columns.reserve(list.size());
  for (const QVariant &item : list) {
    const QVariantMap map = item.toMap();
    columns.append({map.value(QStringLiteral("key")).toString(),
                    integerFrom(map.value(QStringLiteral("width"), 0))});
  }
  return columns;
}

[[nodiscard]] QVariantMap viewToVariant(const FolderView &view) {
  return {{QStringLiteral("viewMode"), view.viewMode},
          {QStringLiteral("sortColumn"), view.sortColumn},
          {QStringLiteral("sortDirection"), view.sortDirection},
          {QStringLiteral("groupBy"), view.groupBy},
          {QStringLiteral("iconSize"), view.iconSize},
          {QStringLiteral("columns"), columnsToVariant(view.columns)}};
}

[[nodiscard]] FolderView viewFromVariant(const QVariantMap &map) {
  return {.viewMode = map.value(QStringLiteral("viewMode")).toString(),
          .sortColumn = map.value(QStringLiteral("sortColumn")).toString(),
          .sortDirection = map.value(QStringLiteral("sortDirection")).toString(),
          .groupBy = map.value(QStringLiteral("groupBy")).toString(),
          .iconSize = integerFrom(map.value(QStringLiteral("iconSize"))),
          .columns = columnsFromVariant(map.value(QStringLiteral("columns")).toList())};
}

} // namespace

PreferencesController::PreferencesController(std::unique_ptr<PreferencesStore> store,
                                             QObject *parent)
    : QObject(parent), m_store(std::move(store)) {
  const PreferencesLoadResult loaded = m_store->load();
  if (loaded.ok()) {
    m_preferences = loaded.preferences;
    return;
  }
  // Absent is a clean first run: the documented defaults, and nothing to
  // report. Anything else keeps the defaults but says why.
  if (loaded.error != PreferencesError::Absent) {
    m_storeError = loaded.diagnostic;
  }
}

QVariantList PreferencesController::iconSizes() const {
  QVariantList published;
  const QList<int> sizes = Preferences::iconSizes();
  published.reserve(sizes.size());
  for (const int size : sizes) {
    published.append(size);
  }
  return published;
}

QStringList PreferencesController::schemes() const { return connectableSchemes(); }

void PreferencesController::setStoreError(const QString &message) {
  if (m_storeError == message) {
    return;
  }
  m_storeError = message;
  Q_EMIT storeErrorChanged();
}

bool PreferencesController::commit(const Preferences &candidate) {
  if (candidate == m_preferences) {
    return true;
  }
  const PreferencesWriteResult written = m_store->store(candidate);
  if (!written.ok()) {
    // AGENT-GUARD: nothing visible changes when the write is refused, so the
    // window never shows a setting the next launch would not read back.
    setStoreError(written.diagnostic);
    return false;
  }
  m_preferences = candidate;
  setStoreError({});
  Q_EMIT preferencesChanged();
  return true;
}

void PreferencesController::setDefaultViewMode(const QString &mode) {
  Preferences candidate = m_preferences;
  candidate.defaultViewMode = mode;
  commit(candidate);
}

void PreferencesController::setShowHidden(const bool showHidden) {
  Preferences candidate = m_preferences;
  candidate.showHidden = showHidden;
  commit(candidate);
}

void PreferencesController::setDirectoriesFirst(const bool directoriesFirst) {
  Preferences candidate = m_preferences;
  candidate.directoriesFirst = directoriesFirst;
  commit(candidate);
}

void PreferencesController::setSortColumn(const QString &column) {
  Preferences candidate = m_preferences;
  candidate.sortColumn = column;
  commit(candidate);
}

void PreferencesController::setSortDirection(const QString &direction) {
  Preferences candidate = m_preferences;
  candidate.sortDirection = direction;
  commit(candidate);
}

void PreferencesController::setIconSize(const int iconSize) {
  Preferences candidate = m_preferences;
  candidate.iconSize = iconSize;
  commit(candidate);
}

void PreferencesController::setDiscoverNearbyServers(const bool discover) {
  Preferences candidate = m_preferences;
  candidate.discoverNearbyServers = discover;
  commit(candidate);
}

void PreferencesController::setDefaultConnectScheme(const QString &scheme) {
  Preferences candidate = m_preferences;
  candidate.defaultConnectScheme = scheme;
  commit(candidate);
}

void PreferencesController::setConfirmTrash(const bool confirmTrash) {
  Preferences candidate = m_preferences;
  candidate.confirmTrash = confirmTrash;
  commit(candidate);
}

void PreferencesController::setGroupBy(const QString &groupBy) {
  Preferences candidate = m_preferences;
  candidate.groupBy = groupBy;
  commit(candidate);
}

QVariantList PreferencesController::detailsColumns() const {
  return columnsToVariant(m_preferences.detailsColumns);
}

void PreferencesController::setDetailsColumns(const QVariantList &columns) {
  Preferences candidate = m_preferences;
  candidate.detailsColumns = columnsFromVariant(columns);
  commit(candidate);
}

void PreferencesController::setRelativeDates(const bool relativeDates) {
  Preferences candidate = m_preferences;
  candidate.relativeDates = relativeDates;
  commit(candidate);
}

void PreferencesController::setRowDensity(const QString &rowDensity) {
  Preferences candidate = m_preferences;
  candidate.rowDensity = rowDensity;
  commit(candidate);
}

void PreferencesController::setShowExtensions(const bool showExtensions) {
  Preferences candidate = m_preferences;
  candidate.showExtensions = showExtensions;
  commit(candidate);
}

QVariantMap PreferencesController::folderView(const QString &location) const {
  QVariantMap view = viewToVariant(m_preferences.folderViewFor(location));
  view.insert(QStringLiteral("remembered"), m_preferences.remembers(location));
  return view;
}

QVariantMap PreferencesController::defaultFolderView() const {
  return viewToVariant(m_preferences.defaultFolderView());
}

bool PreferencesController::rememberFolderView(const QString &location,
                                               const QVariantMap &view) {
  const FolderView wanted = viewFromVariant(view);
  // AGENT-GUARD: an unchanged view writes nothing and keeps its place in the
  // most-recent order; the window asks after every presentation change.
  if (m_preferences.folderViewFor(location) == wanted) {
    return true;
  }
  Preferences candidate = m_preferences;
  candidate.rememberFolderView(location, wanted);
  return commit(candidate);
}

bool PreferencesController::useAsDefaults(const QVariantMap &view) {
  Preferences candidate = m_preferences;
  candidate.setDefaultFolderView(viewFromVariant(view));
  const FolderView defaults = candidate.defaultFolderView();
  candidate.folderViews.removeIf([&defaults](const RememberedFolderView &remembered) {
    return remembered.view == defaults;
  });
  return commit(candidate);
}

void PreferencesController::restoreDefaults() { commit(Preferences{}); }

void PreferencesController::clearStoreError() { setStoreError({}); }

} // namespace QindaQt::Apps::FileManager
