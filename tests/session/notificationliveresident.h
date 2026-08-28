// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <optional>

namespace QindaQt::Test {

inline constexpr auto NotificationLiveResidentOwnerService =
    "org.qindaqt.NotificationLiveOwner";

// Runs the private, long-lived notification sender used across the shell
// restart. The owner exposes one authenticated close operation because the
// production notification service correctly rejects closure by another D-Bus
// sender.
[[nodiscard]] int runResidentNotificationOwner();
[[nodiscard]] bool closeResidentNotification(quint32 id, qint64 expectedOwnerProcessId,
                                             QString *error);

} // namespace QindaQt::Test
