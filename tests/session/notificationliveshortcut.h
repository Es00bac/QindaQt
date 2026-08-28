// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::Test {

struct NotificationLiveShortcut final {
    QStringList actionId;
    QList<int> defaultKeys;
    QList<int> activeKeys;
};

[[nodiscard]] std::optional<NotificationLiveShortcut>
discoverNotificationLiveShortcut(QString *error);
[[nodiscard]] bool setNotificationLiveShortcut(
    const NotificationLiveShortcut &shortcut, const QList<int> &keys,
    QString *error);
[[nodiscard]] QList<int> notificationLiveMetaShiftN();

} // namespace QindaQt::Test
