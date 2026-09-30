// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/lock_preferences/legacy_lock_import.h"
#include "legacy_lock_import_p.h"
#include "qindaqt/services/lock_preferences/lock_preferences.h"
#include "qindaqt/services/settings_service/settings_repository.h"
namespace QindaQt::Services::SettingsService::Private {
void applyLegacyLockImport(SettingsRepository &repository,
                           const QString &readOnlyPath) {
  QVariantMap legacy;
  if (!LockPreferences::readLegacyPreferences(readOnlyPath, &legacy))
    return;
  auto keys = LockPreferences::scopedKeys();
  keys.append(QStringLiteral("lock.migration.kscreenlockerImported"));
  const auto snapshot = repository.snapshot(keys);
  if (!snapshot.ok)
    return;
  QVariantMap explicitValues;
  for (const auto &key : keys)
    if (snapshot.sourceLayers.value(key) ==
        Settings::SettingLayer::UserOverrides)
      explicitValues.insert(key, snapshot.values.value(key));
  const auto plan = LockPreferences::planLegacyImport(legacy, explicitValues);
  if (!plan.sourceSupported || plan.values.isEmpty())
    return;
  QVector<SettingsRepository::Operation> operations;
  for (auto it = plan.values.cbegin(); it != plan.values.cend(); ++it)
    operations.push_back({it.key(), false, it.value()});
  // AGENT-GUARD: marker and imported values share one durable transaction.
  // A save failure cannot advance the marker or suppress a later retry.
  (void)repository.commitUserOverrides(repository.revision(), operations);
}
} // namespace QindaQt::Services::SettingsService::Private
