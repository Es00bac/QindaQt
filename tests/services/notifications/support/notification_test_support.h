// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/services/notifications/notification_backend.h"
#include "qindaqt/services/notifications/notification_clock.h"
#include "qindaqt/services/notifications/notification_types.h"

#include <QStringList>

#include <functional>

namespace QindaQt::Services::Notifications::TestSupport {

class ManualNotificationClock final : public NotificationClock {
public:
    [[nodiscard]] qint64 nowMs() const noexcept override { return now; }

    qint64 now = 0;
};

class RecordingNotificationBackend final : public NotificationBackend {
public:
    void modelPublished(NotificationSnapshotPtr snapshot) override
    {
        publications.push_back(std::move(snapshot));
        eventOrder.push_back(QStringLiteral("model"));
        if (onPublication) {
            onPublication();
        }
    }

    void notificationClosed(const NotificationCloseEvent &event) override
    {
        closures.push_back(event);
        eventOrder.push_back(QStringLiteral("closed"));
    }

    void actionInvoked(const NotificationActionEvent &event) override
    {
        actions.push_back(event);
        eventOrder.push_back(QStringLiteral("action"));
    }

    QVector<NotificationSnapshotPtr> publications;
    QVector<NotificationCloseEvent> closures;
    QVector<NotificationActionEvent> actions;
    QStringList eventOrder;
    std::function<void()> onPublication;
};

inline NotificationRequest request(QString source,
                                   QString summary = QStringLiteral("Summary"))
{
    NotificationRequest value;
    value.sourceService = std::move(source);
    value.applicationName = QStringLiteral("Example");
    value.summary = std::move(summary);
    value.body = QStringLiteral("Body");
    return value;
}

} // namespace QindaQt::Services::Notifications::TestSupport
