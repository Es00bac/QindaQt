// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notifications/notification_backend.h"
#include "qindaqt/services/notifications/notification_clock.h"
#include "qindaqt/services/notifications/notification_types.h"

#include <QMap>

#include <memory>
#include <optional>

namespace QindaQt::Services::Notifications::Private {
class NotificationCapacityLedger;
class NotificationIdAllocator;
}

namespace QindaQt::Services::Notifications {

// Single-thread-confined notification policy and state. The class owns no
// timer, bus, or QML object: a host schedules expireDue() from
// nextExpiryDeadlineMs(), while an adapter translates authenticated IPC into
// these typed calls.
class NotificationService final {
public:
    NotificationService(NotificationClock &clock,
                        NotificationBackend &backend,
                        NotificationPolicy policy = {},
                        NotificationRevisionSeed revisionSeed = {});
    ~NotificationService();

    NotificationService(const NotificationService &) = delete;
    NotificationService &operator=(const NotificationService &) = delete;
    NotificationService(NotificationService &&) = delete;
    NotificationService &operator=(NotificationService &&) = delete;

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] const QString &initializationError() const noexcept;
    [[nodiscard]] NotificationSnapshotPtr snapshot() const noexcept;
    [[nodiscard]] std::optional<qint64> nextExpiryDeadlineMs() const noexcept;

    [[nodiscard]] NotificationOperationResult submit(const NotificationRequest &request);
    [[nodiscard]] NotificationOperationResult closeFromApplication(
        const QString &sourceService,
        quint32 id);
    [[nodiscard]] NotificationOperationResult dismiss(quint32 id);
    [[nodiscard]] NotificationOperationResult invokeAction(
        quint32 id,
        const QString &actionKey,
        const QString &activationToken = {});
    [[nodiscard]] NotificationOperationResult expireDue();

private:
    [[nodiscard]] NotificationOperationResult reject(OperationStatus status,
                                                     QString message,
                                                     quint32 id = 0) const;
    [[nodiscard]] std::optional<qint64> checkedNow(QString *error);
    [[nodiscard]] std::optional<qint64> expirationFor(const NotificationRequest &request,
                                                     qint64 now,
                                                     QString *error) const;
    [[nodiscard]] NotificationSnapshotPtr buildSnapshot(quint64 revision) const;
    [[nodiscard]] bool canAdvanceRevision() const noexcept;
    void publishModel();
    void dispatchClosed(const QVector<NotificationCloseEvent> &events);

    NotificationClock &m_clock;
    NotificationBackend &m_backend;
    NotificationPolicy m_policy;
    QString m_initializationError;
    QMap<quint32, NotificationView> m_active;
    // Admitted payload sizes are retained beside entries so quota release and
    // replacement never re-encode attacker-controlled text or rescan images.
    QMap<quint32, qsizetype> m_payloadBytes;
    NotificationSnapshotPtr m_snapshot;
    quint64 m_revision = 0;
    std::unique_ptr<Private::NotificationIdAllocator> m_idAllocator;
    std::unique_ptr<Private::NotificationCapacityLedger> m_capacityLedger;
    qint64 m_lastNowMs = -1;
    bool m_dispatching = false;
};

} // namespace QindaQt::Services::Notifications
