// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences_controller.h"

#include "../network/connect_request.h"

#include <QVariantList>

#include <utility>

namespace QindaQt::Apps::FileManager {

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

void PreferencesController::restoreDefaults() { commit(Preferences{}); }

void PreferencesController::clearStoreError() { setStoreError({}); }

} // namespace QindaQt::Apps::FileManager
