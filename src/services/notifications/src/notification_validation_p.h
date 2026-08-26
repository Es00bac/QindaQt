// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notifications/notification_types.h"

namespace QindaQt::Services::Notifications::Private {

[[nodiscard]] bool validateRequest(const NotificationRequest &request, QString *error);
[[nodiscard]] bool validateSourceService(const QString &sourceService, QString *error);
[[nodiscard]] bool validateActionInvocation(const QString &actionKey,
                                            const QString &activationToken,
                                            QString *error);
[[nodiscard]] qsizetype retainedPayloadBytes(const NotificationRequest &request);

} // namespace QindaQt::Services::Notifications::Private
