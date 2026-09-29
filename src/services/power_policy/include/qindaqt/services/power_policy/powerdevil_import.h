// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QPair>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

namespace QindaQt::Services::PowerPolicy {

struct ImportPlan final {
  bool sourceSupported = false;
  QVector<QPair<QString, QVariant>> values;
};

// Read-only KConfig-group reader for an injected path. Missing, malformed, empty,
// unreadable, or over-1-MiB sources return false and leave entries unchanged.
[[nodiscard]] bool readPowerDevilPreferences(const QString &path,
                                             QVariantMap *entries);

// Pure conversion of documented PowerDevil 6.6.6 keys. Entries use flattened
// group/key names; explicit Settings1 user values take precedence. The caller
// owns persistence and must commit all returned values in one transaction.
[[nodiscard]] ImportPlan
planPowerDevilImport(const QVariantMap &legacyEntries,
                     const QVariantMap &explicitNativeValues);

} // namespace QindaQt::Services::PowerPolicy
