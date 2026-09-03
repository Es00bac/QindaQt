// SPDX-License-Identifier: GPL-3.0-or-later
//
// Private identity derivation shared by every production adapter. Deliberately
// free of D-Bus includes so the sysfs adapter can use it too.

#pragma once

#include <QtCore/QString>

namespace QindaQt::Power::Upstream {

// Derives a bounded public opaque ID from untrusted upstream identity
// material without ever exposing the raw value. AGENT-GUARD: a raw object
// path, sysfs path, serial number, UID, or PID must never become a public
// opaque ID; route every identity through this one-way derivation.
[[nodiscard]] QString deriveOpaqueId(const QString &domain, const QString &key);

} // namespace QindaQt::Power::Upstream
