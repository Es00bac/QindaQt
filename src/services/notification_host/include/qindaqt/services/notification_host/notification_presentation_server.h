// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_access_token.h"

#include <QDBusConnection>
#include <QString>
#include <QtTypes>

#include <memory>

namespace QindaQt::Services::Notifications {
class NotificationService;
}

namespace QindaQt::Services::NotificationHost {

// AGENT-CONTRACT: optional server adapter confined to the host's D-Bus
// thread. The borrowed NotificationService must outlive this object. start()
// owns only the private object path; its caller owns service-name rollback.
class NotificationPresentationServer final {
public:
    NotificationPresentationServer(
        QDBusConnection connection,
        Notifications::NotificationService &service,
        NotificationPresentation::PresentationAccessToken accessToken);
    ~NotificationPresentationServer();

    NotificationPresentationServer(const NotificationPresentationServer &) = delete;
    NotificationPresentationServer &operator=(const NotificationPresentationServer &) = delete;

    [[nodiscard]] bool start();
    void stop() noexcept;
    void publishRevision(quint64 revision);

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] const QString &lastError() const noexcept;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::NotificationHost
