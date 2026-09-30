// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QVariantMap>
namespace QindaQt::Services::LockPreferences {
struct ImportPlan final {
  bool sourceSupported = false;
  QVariantMap values;
};
// Read-only injected path, <=1MiB. Missing/malformed/empty sources leave output
// unchanged. Only documented Daemon keys enter the planner; no QML path or
// never-password bypass.
bool readLegacyPreferences(const QString &path, QVariantMap *output);
// Explicit native user overrides win. Import marker and all returned values
// MUST share one SettingsRepository transaction; failed persistence remains
// retryable.
ImportPlan planLegacyImport(const QVariantMap &legacy,
                            const QVariantMap &explicitNative);
} // namespace QindaQt::Services::LockPreferences
