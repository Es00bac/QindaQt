// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "notificationliveworkflow.h"

#include <QJsonObject>
#include <QString>

#include <functional>
#include <optional>

namespace QindaQt::Test {

inline constexpr auto NotificationLiveCompositorService = "org.qindaqt.Compositor";
inline constexpr auto NotificationLiveSettingsService = "org.qindaqt.Settings1";
inline constexpr auto NotificationLiveNotificationService = "org.freedesktop.Notifications";

[[nodiscard]] bool awaitNotificationLiveCondition(const std::function<bool()> &condition,
                                                  int timeoutMilliseconds = 7'500);
[[nodiscard]] std::optional<QString> notificationLiveServiceOwner(const QString &service,
                                                                  QString *error);
[[nodiscard]] std::optional<qint64> notificationLiveServicePid(const QString &service,
                                                               QString *error);
[[nodiscard]] bool validateNotificationLiveSignalTarget(qint64 processId, QString *error);
[[nodiscard]] bool awaitNotificationLiveService(const QString &service);
[[nodiscard]] std::optional<QJsonObject>
validateNotificationLiveRuntime(const NotificationLiveExpectations &expectations, QString *error);

} // namespace QindaQt::Test
