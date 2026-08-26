// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Services::NotificationHost::Private {

// Validates the well-known-name grammar before any bus query. This keeps a
// caller input error distinct from an unavailable or unhealthy session bus.
[[nodiscard]] bool validateWellKnownServiceName(const QString &serviceName,
                                                QString *error);

} // namespace QindaQt::Services::NotificationHost::Private
