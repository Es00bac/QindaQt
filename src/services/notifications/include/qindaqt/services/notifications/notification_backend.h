// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notifications/notification_types.h"

namespace QindaQt::Services::Notifications {

// Output seam shared by presentation and protocol-signal adapters. The service
// mutates its complete model before dispatch and rejects reentrant operations;
// implementations may inspect snapshot() from a callback but must not mutate
// the service there. The backend must outlive the service.
class NotificationBackend {
public:
    virtual ~NotificationBackend() = default;

    virtual void modelPublished(NotificationSnapshotPtr snapshot) = 0;
    virtual void notificationClosed(const NotificationCloseEvent &event) = 0;
    virtual void actionInvoked(const NotificationActionEvent &event) = 0;
};

class NullNotificationBackend final : public NotificationBackend {
public:
    void modelPublished(NotificationSnapshotPtr) override { }
    void notificationClosed(const NotificationCloseEvent &) override { }
    void actionInvoked(const NotificationActionEvent &) override { }
};

} // namespace QindaQt::Services::Notifications
