// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/notifications/notification_types.h"

#include "qindaqt/services/notifications/notification_limits.h"

namespace QindaQt::Services::Notifications {

bool isValidUrgency(Urgency urgency) noexcept
{
    switch (urgency) {
    case Urgency::Low:
    case Urgency::Normal:
    case Urgency::Critical:
        return true;
    }
    return false;
}

bool NotificationPolicy::validate(QString *error) const
{
    auto reject = [error](const QString &message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (maximumActiveNotifications <= 0
        || maximumActiveNotifications
            > NotificationLimits::MaximumConfiguredActiveNotifications) {
        return reject(QStringLiteral("maximumActiveNotifications is outside the bounded range"));
    }
    if (maximumActiveNotificationsPerSource <= 0
        || maximumActiveNotificationsPerSource
            > NotificationLimits::MaximumConfiguredActiveNotificationsPerSource) {
        return reject(QStringLiteral(
            "maximumActiveNotificationsPerSource is outside the bounded range"));
    }
    if (maximumRetainedPayloadBytes <= 0
        || maximumRetainedPayloadBytes
            > NotificationLimits::MaximumConfiguredRetainedPayloadBytes) {
        return reject(QStringLiteral("maximumRetainedPayloadBytes is outside the bounded range"));
    }
    if (maximumRetainedPayloadBytesPerSource <= 0
        || maximumRetainedPayloadBytesPerSource
            > NotificationLimits::MaximumConfiguredRetainedPayloadBytesPerSource) {
        return reject(QStringLiteral(
            "maximumRetainedPayloadBytesPerSource is outside the bounded range"));
    }
    if (defaultTimeoutMs < 0 || criticalDefaultTimeoutMs < 0
        || maximumRequestedTimeoutMs <= 0) {
        return reject(QStringLiteral("notification timeouts must be non-negative and bounded"));
    }
    if ((defaultTimeoutMs > 0 && defaultTimeoutMs > maximumRequestedTimeoutMs)
        || (criticalDefaultTimeoutMs > 0
            && criticalDefaultTimeoutMs > maximumRequestedTimeoutMs)) {
        return reject(QStringLiteral("default timeouts cannot exceed the requested-timeout cap"));
    }
    return true;
}

QString operationStatusName(OperationStatus status)
{
    switch (status) {
    case OperationStatus::Applied:
        return QStringLiteral("applied");
    case OperationStatus::InvalidRequest:
        return QStringLiteral("invalid-request");
    case OperationStatus::InvalidPolicy:
        return QStringLiteral("invalid-policy");
    case OperationStatus::NotFound:
        return QStringLiteral("not-found");
    case OperationStatus::NotOwner:
        return QStringLiteral("not-owner");
    case OperationStatus::UnknownAction:
        return QStringLiteral("unknown-action");
    case OperationStatus::CapacityReached:
        return QStringLiteral("capacity-reached");
    case OperationStatus::RevisionExhausted:
        return QStringLiteral("revision-exhausted");
    case OperationStatus::ClockFailure:
        return QStringLiteral("clock-failure");
    case OperationStatus::ReentrantOperation:
        return QStringLiteral("reentrant-operation");
    }
    return QStringLiteral("unknown");
}

} // namespace QindaQt::Services::Notifications
