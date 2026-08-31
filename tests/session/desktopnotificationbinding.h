// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace QindaQt::Test {

struct DesktopNotificationBinding final {
    QString uniqueName;
    QList<int> defaultKeys;
    QList<int> activeKeys;
};

[[nodiscard]] QString desktopNotificationActionId();
[[nodiscard]] int desktopNotificationMetaN();
[[nodiscard]] bool desktopNotificationBindingReady(
    const DesktopNotificationBinding &binding);
[[nodiscard]] std::optional<DesktopNotificationBinding>
queryDesktopNotificationBinding(QString *error);

} // namespace QindaQt::Test
