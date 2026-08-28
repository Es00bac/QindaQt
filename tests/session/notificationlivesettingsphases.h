// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "notificationliveworkflow.h"

#include <QJsonObject>

namespace QindaQt::Test {

[[nodiscard]] QJsonObject runNotificationLiveSettingsPhase(
    const NotificationLiveExpectations &expectations, QJsonObject evidence);

} // namespace QindaQt::Test
