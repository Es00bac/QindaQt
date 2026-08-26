// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notifications/notification_clock.h"
#include "qindaqt/services/notifications/notification_service.h"
#include "qindaqt/services/notifications/notification_types.h"

#include <QDBusConnection>
#include <QStringList>

#include <memory>

namespace QindaQt::Services::Notifications {

struct FreedesktopServerIdentity final {
    QString name = QStringLiteral("QindaQt");
    QString vendor = QStringLiteral("QindaQt");
    QString version = QStringLiteral("0.1.0");
    QString specificationVersion = QStringLiteral("1.3");
    QStringList capabilities = {
        QStringLiteral("body"),
    };
};

// Owns one org.freedesktop.Notifications object and its authenticated adapter.
// The supplied connection, clock, and optional presentation backend must
// remain usable until this object is destroyed. Registration is explicit so a
// session host can report ownership conflicts instead of silently running
// without notifications. Construction, start/stop, service access, and the
// registered QObject are confined to the connection object's thread.
class FreedesktopNotificationServer final {
public:
    FreedesktopNotificationServer(QDBusConnection connection,
                                  NotificationClock &clock,
                                  NotificationPolicy policy = {},
                                  FreedesktopServerIdentity identity = {},
                                  NotificationBackend *presentationBackend = nullptr);
    ~FreedesktopNotificationServer();

    FreedesktopNotificationServer(const FreedesktopNotificationServer &) = delete;
    FreedesktopNotificationServer &operator=(const FreedesktopNotificationServer &) = delete;
    FreedesktopNotificationServer(FreedesktopNotificationServer &&) = delete;
    FreedesktopNotificationServer &operator=(FreedesktopNotificationServer &&) = delete;

    [[nodiscard]] bool start(
        const QString &serviceName = QStringLiteral("org.freedesktop.Notifications"));
    void stop() noexcept;

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] const QString &lastError() const noexcept;
    [[nodiscard]] NotificationService &service() noexcept;
    [[nodiscard]] const NotificationService &service() const noexcept;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::Notifications
