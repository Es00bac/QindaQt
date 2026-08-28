// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

namespace QindaQt::Test {

enum class NotificationLivePhase {
    Primary,
    SettingsRejected,
    SettingsUncertain,
    SettingsOutage,
    SettingsRestart,
    ShellRestart,
};

struct NotificationLiveExpectations final {
    NotificationLivePhase phase = NotificationLivePhase::Primary;
    qint64 compositorProcessId = 0;
    qint64 notificationHostProcessId = 0;
    qint64 settingsProcessId = 0;
    qint64 shellProcessId = 0;
    qint64 residentOwnerProcessId = 0;
    quint32 residentNotificationId = 0;
    int logicalWidth = 0;
    int logicalHeight = 0;
    double scale = 0.0;
};

[[nodiscard]] QJsonObject runNotificationLiveWorkflow(
    const NotificationLiveExpectations &expectations);
[[nodiscard]] QString notificationLivePhaseName(NotificationLivePhase phase);

} // namespace QindaQt::Test
