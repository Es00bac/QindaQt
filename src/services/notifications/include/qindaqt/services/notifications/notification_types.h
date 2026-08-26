// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtTypes>

#include <memory>
#include <optional>

namespace QindaQt::Services::Notifications {

enum class Urgency : quint8 {
    Low = 0,
    Normal = 1,
    Critical = 2,
};

// Typed callers can still construct an out-of-domain enum through a cast.
// Keep this validation public so adapters and direct service consumers share
// the same accepted domain rather than duplicating the wire-value range.
[[nodiscard]] bool isValidUrgency(Urgency urgency) noexcept;

// Values intentionally match the freedesktop NotificationClosed wire values.
enum class CloseReason : quint32 {
    Expired = 1,
    DismissedByUser = 2,
    ClosedByApplication = 3,
    Undefined = 4,
};

struct NotificationAction final {
    QString key;
    QString label;

    bool operator==(const NotificationAction &) const = default;
};

struct NotificationImage final {
    int width = 0;
    int height = 0;
    int rowStride = 0;
    bool hasAlpha = false;
    int bitsPerSample = 8;
    int channels = 0;
    QByteArray pixels;

    bool operator==(const NotificationImage &) const = default;
};

struct NotificationHints final {
    Urgency urgency = Urgency::Normal;
    QString category;
    QString desktopEntry;
    QString imagePath;
    QString soundFile;
    QString soundName;
    std::optional<NotificationImage> image;
    bool actionIcons = false;
    bool resident = false;
    bool transient = false;
    bool suppressSound = false;

    bool operator==(const NotificationHints &) const = default;
};

struct NotificationRequest final {
    // The protocol adapter supplies the authenticated unique bus name. It is
    // never accepted from an untrusted payload field.
    QString sourceService;
    QString applicationName;
    quint32 replacesId = 0;
    QString applicationIcon;
    QString summary;
    QString body;
    QVector<NotificationAction> actions;
    NotificationHints hints;
    // Freedesktop semantics: -1 chooses server policy, 0 never expires, and a
    // positive value requests milliseconds.
    int expireTimeoutMs = -1;
};

struct NotificationView final {
    quint32 id = 0;
    QString sourceService;
    QString applicationName;
    QString applicationIcon;
    QString summary;
    QString body;
    QVector<NotificationAction> actions;
    NotificationHints hints;
    qint64 createdAtMs = 0;
    std::optional<qint64> updatedAtMs;
    std::optional<qint64> expiresAtMs;

    bool operator==(const NotificationView &) const = default;
};

struct NotificationModelSnapshot final {
    quint64 revision = 0;
    // Stable ascending ID order keeps equivalent histories byte-for-byte
    // deterministic without imposing a presentation sort policy.
    QVector<NotificationView> notifications;
};

using NotificationSnapshotPtr = std::shared_ptr<const NotificationModelSnapshot>;

struct NotificationCloseEvent final {
    quint32 id = 0;
    QString sourceService;
    CloseReason reason = CloseReason::Undefined;
    quint64 revision = 0;

    bool operator==(const NotificationCloseEvent &) const = default;
};

struct NotificationActionEvent final {
    quint32 id = 0;
    QString sourceService;
    QString actionKey;
    QString activationToken;
    quint64 revision = 0;

    bool operator==(const NotificationActionEvent &) const = default;
};

enum class OperationStatus {
    Applied,
    InvalidRequest,
    InvalidPolicy,
    NotFound,
    NotOwner,
    UnknownAction,
    CapacityReached,
    RevisionExhausted,
    ClockFailure,
    ReentrantOperation,
};

struct NotificationOperationResult final {
    OperationStatus status = OperationStatus::InvalidRequest;
    quint64 revisionBefore = 0;
    quint64 revisionAfter = 0;
    quint32 notificationId = 0;
    QVector<quint32> affectedIds;
    bool replaced = false;
    QString message;

    [[nodiscard]] bool ok() const noexcept
    {
        return status == OperationStatus::Applied;
    }
};

struct NotificationPolicy final {
    qsizetype maximumActiveNotifications = 256;
    // AGENT-CONTRACT: Both per-source limits are keyed by the authenticated
    // NotificationRequest::sourceService supplied by the adapter. Never key
    // fairness to applicationName or another caller-controlled payload field.
    qsizetype maximumActiveNotificationsPerSource = 64;
    qsizetype maximumRetainedPayloadBytes = 64 * 1'024 * 1'024;
    qsizetype maximumRetainedPayloadBytesPerSource = 32 * 1'024 * 1'024;
    int defaultTimeoutMs = 5'000;
    int criticalDefaultTimeoutMs = 0;
    int maximumRequestedTimeoutMs = 24 * 60 * 60 * 1'000;
    bool closeNonResidentAfterAction = true;

    [[nodiscard]] bool validate(QString *error = nullptr) const;
};

// Trusted construction-time seam for carrying a monotonic revision across a
// future host migration and for qualifying integer exhaustion. Notification
// request clients never control this value, and it does not restore entries.
struct NotificationRevisionSeed final {
    quint64 value = 0;
};

[[nodiscard]] QString operationStatusName(OperationStatus status);

} // namespace QindaQt::Services::Notifications
