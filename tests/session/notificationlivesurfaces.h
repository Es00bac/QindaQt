// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "notificationliveworkflow.h"

#include <QJsonObject>
#include <QString>

namespace QindaQt::Test {

class CompositorProbeClient;

[[nodiscard]] bool validateNotificationLiveSurface(
    QLatin1StringView scope, const QJsonObject &shellSnapshot,
    const NotificationLiveExpectations &expectations,
    CompositorProbeClient &compositor, QString *error);

} // namespace QindaQt::Test
