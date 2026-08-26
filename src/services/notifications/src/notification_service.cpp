// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/notifications/notification_service.h"

#include "notification_capacity_p.h"
#include "notification_id_allocator_p.h"
#include "notification_validation_p.h"

#include <QScopedValueRollback>

#include <algorithm>
#include <limits>

namespace QindaQt::Services::Notifications {

NotificationService::NotificationService(NotificationClock &clock,
                                         NotificationBackend &backend,
                                         NotificationPolicy policy,
                                         NotificationRevisionSeed revisionSeed)
    : m_clock(clock)
    , m_backend(backend)
    , m_policy(policy)
    , m_revision(revisionSeed.value)
    , m_idAllocator(std::make_unique<Private::NotificationIdAllocator>())
    , m_capacityLedger(
          std::make_unique<Private::NotificationCapacityLedger>(m_policy))
{
    const bool policyValid = m_policy.validate(&m_initializationError);
    if (!policyValid && m_initializationError.isEmpty()) {
        m_initializationError = QStringLiteral("notification policy is invalid");
    }
    m_snapshot = buildSnapshot(m_revision);
}

NotificationService::~NotificationService() = default;

bool NotificationService::isReady() const noexcept
{
    return m_initializationError.isEmpty();
}

const QString &NotificationService::initializationError() const noexcept
{
    return m_initializationError;
}

NotificationSnapshotPtr NotificationService::snapshot() const noexcept
{
    return m_snapshot;
}

std::optional<qint64> NotificationService::nextExpiryDeadlineMs() const noexcept
{
    std::optional<qint64> earliest;
    for (const auto &notification : m_active) {
        if (notification.expiresAtMs.has_value()
            && (!earliest.has_value() || *notification.expiresAtMs < *earliest)) {
            earliest = notification.expiresAtMs;
        }
    }
    return earliest;
}

NotificationOperationResult NotificationService::reject(OperationStatus status,
                                                         QString message,
                                                         quint32 id) const
{
    NotificationOperationResult result;
    result.status = status;
    result.revisionBefore = m_revision;
    result.revisionAfter = m_revision;
    result.notificationId = id;
    result.message = std::move(message);
    return result;
}

std::optional<qint64> NotificationService::checkedNow(QString *error)
{
    const qint64 now = m_clock.nowMs();
    if (now < 0 || now < m_lastNowMs) {
        *error = QStringLiteral("notification clock moved outside its monotonic domain");
        return std::nullopt;
    }
    return now;
}

std::optional<qint64> NotificationService::expirationFor(
    const NotificationRequest &request,
    qint64 now,
    QString *error) const
{
    int timeout = request.expireTimeoutMs;
    if (timeout == -1) {
        timeout = request.hints.urgency == Urgency::Critical
            ? m_policy.criticalDefaultTimeoutMs
            : m_policy.defaultTimeoutMs;
    } else if (timeout > m_policy.maximumRequestedTimeoutMs) {
        timeout = m_policy.maximumRequestedTimeoutMs;
    }

    if (timeout == 0) {
        return std::nullopt;
    }
    if (now > std::numeric_limits<qint64>::max() - qint64(timeout)) {
        *error = QStringLiteral("notification expiration would overflow the clock domain");
        return std::nullopt;
    }
    return now + qint64(timeout);
}

NotificationSnapshotPtr NotificationService::buildSnapshot(quint64 revision) const
{
    auto next = std::make_shared<NotificationModelSnapshot>();
    next->revision = revision;
    next->notifications.reserve(m_active.size());
    for (const auto &notification : m_active) {
        next->notifications.push_back(notification);
    }
    return next;
}

bool NotificationService::canAdvanceRevision() const noexcept
{
    return m_revision != std::numeric_limits<quint64>::max();
}

void NotificationService::publishModel()
{
    QScopedValueRollback guard(m_dispatching, true);
    m_backend.modelPublished(m_snapshot);
}

void NotificationService::dispatchClosed(const QVector<NotificationCloseEvent> &events)
{
    QScopedValueRollback guard(m_dispatching, true);
    for (const auto &event : events) {
        m_backend.notificationClosed(event);
    }
}

NotificationOperationResult NotificationService::submit(const NotificationRequest &request)
{
    if (m_dispatching) {
        return reject(OperationStatus::ReentrantOperation,
                      QStringLiteral("backend callbacks cannot mutate notification state"));
    }
    if (!isReady()) {
        return reject(OperationStatus::InvalidPolicy, m_initializationError);
    }

    QString error;
    if (!Private::validateRequest(request, &error)) {
        return reject(OperationStatus::InvalidRequest, std::move(error));
    }

    const auto existing = request.replacesId == 0
        ? m_active.end()
        : m_active.find(request.replacesId);
    const bool replacing = existing != m_active.end();
    if (replacing && existing->sourceService != request.sourceService) {
        return reject(OperationStatus::NotOwner,
                      QStringLiteral("a notification can only be replaced by its owner"),
                      request.replacesId);
    }
    const auto previousPayload = replacing
        ? m_payloadBytes.constFind(existing.key())
        : m_payloadBytes.cend();
    Q_ASSERT(!replacing || previousPayload != m_payloadBytes.cend());
    const qsizetype previousPayloadBytes = replacing ? previousPayload.value() : 0;
    const auto capacity = m_capacityLedger->evaluateSubmission(request,
                                                               replacing,
                                                               previousPayloadBytes);
    if (!capacity.ok()) {
        return reject(OperationStatus::CapacityReached, capacity.error);
    }
    if (!canAdvanceRevision()) {
        return reject(OperationStatus::RevisionExhausted,
                      QStringLiteral("notification revision is exhausted"));
    }
    const auto now = checkedNow(&error);
    if (!now.has_value()) {
        return reject(OperationStatus::ClockFailure, std::move(error));
    }
    const auto expiration = expirationFor(request, *now, &error);
    if (!error.isEmpty()) {
        return reject(OperationStatus::ClockFailure, std::move(error));
    }

    NotificationView next;
    // Freedesktop Notifications 1.3 requires a nonzero replaces_id to be
    // returned unchanged even when the prior notification is no longer active.
    if (request.replacesId != 0) {
        next.id = request.replacesId;
    } else {
        const auto allocatedId = m_idAllocator->allocate(
            [this](quint32 id) { return m_active.contains(id); });
        if (!allocatedId.has_value()) {
            return reject(OperationStatus::CapacityReached,
                          QStringLiteral("notification id space is exhausted for this service lifetime"));
        }
        next.id = *allocatedId;
    }
    next.sourceService = request.sourceService;
    next.applicationName = request.applicationName;
    next.applicationIcon = request.applicationIcon;
    next.summary = request.summary;
    next.body = request.body;
    next.actions = request.actions;
    next.hints = request.hints;
    next.createdAtMs = replacing ? existing->createdAtMs : *now;
    next.updatedAtMs = replacing ? std::optional<qint64>(*now) : std::nullopt;
    next.expiresAtMs = expiration;

    const quint64 before = m_revision;
    const quint32 notificationId = next.id;
    m_active.insert(next.id, std::move(next));
    m_payloadBytes.insert(notificationId, capacity.requestPayloadBytes);
    m_capacityLedger->commitSubmission(request.sourceService,
                                       replacing,
                                       previousPayloadBytes,
                                       capacity.requestPayloadBytes);
    m_lastNowMs = *now;
    ++m_revision;
    m_snapshot = buildSnapshot(m_revision);
    publishModel();

    NotificationOperationResult result;
    result.status = OperationStatus::Applied;
    result.revisionBefore = before;
    result.revisionAfter = m_revision;
    result.notificationId = notificationId;
    result.affectedIds = {notificationId};
    result.replaced = replacing;
    return result;
}

NotificationOperationResult NotificationService::closeFromApplication(
    const QString &sourceService,
    quint32 id)
{
    if (m_dispatching) {
        return reject(OperationStatus::ReentrantOperation,
                      QStringLiteral("backend callbacks cannot mutate notification state"),
                      id);
    }
    if (!isReady()) {
        return reject(OperationStatus::InvalidPolicy, m_initializationError, id);
    }

    QString error;
    if (!Private::validateSourceService(sourceService, &error) || id == 0) {
        return reject(OperationStatus::InvalidRequest,
                      id == 0 ? QStringLiteral("notification id must be nonzero") : error,
                      id);
    }
    const auto existing = m_active.find(id);
    if (existing == m_active.end()) {
        return reject(OperationStatus::NotFound,
                      QStringLiteral("notification id is not active"),
                      id);
    }
    if (existing->sourceService != sourceService) {
        return reject(OperationStatus::NotOwner,
                      QStringLiteral("a notification can only be closed by its owner"),
                      id);
    }
    if (!canAdvanceRevision()) {
        return reject(OperationStatus::RevisionExhausted,
                      QStringLiteral("notification revision is exhausted"),
                      id);
    }

    const quint64 before = m_revision;
    const QString owner = existing->sourceService;
    const auto payload = m_payloadBytes.find(id);
    Q_ASSERT(payload != m_payloadBytes.end());
    m_capacityLedger->release(existing->sourceService,
                              payload.value());
    m_payloadBytes.erase(payload);
    m_active.erase(existing);
    ++m_revision;
    m_snapshot = buildSnapshot(m_revision);
    publishModel();
    dispatchClosed({NotificationCloseEvent{id,
                                           owner,
                                           CloseReason::ClosedByApplication,
                                           m_revision}});

    NotificationOperationResult result;
    result.status = OperationStatus::Applied;
    result.revisionBefore = before;
    result.revisionAfter = m_revision;
    result.notificationId = id;
    result.affectedIds = {id};
    return result;
}

NotificationOperationResult NotificationService::dismiss(quint32 id)
{
    if (m_dispatching) {
        return reject(OperationStatus::ReentrantOperation,
                      QStringLiteral("backend callbacks cannot mutate notification state"),
                      id);
    }
    if (!isReady()) {
        return reject(OperationStatus::InvalidPolicy, m_initializationError, id);
    }
    if (id == 0) {
        return reject(OperationStatus::InvalidRequest,
                      QStringLiteral("notification id must be nonzero"));
    }
    const auto existing = m_active.find(id);
    if (existing == m_active.end()) {
        return reject(OperationStatus::NotFound,
                      QStringLiteral("notification id is not active"),
                      id);
    }
    if (!canAdvanceRevision()) {
        return reject(OperationStatus::RevisionExhausted,
                      QStringLiteral("notification revision is exhausted"),
                      id);
    }

    const quint64 before = m_revision;
    const QString owner = existing->sourceService;
    const auto payload = m_payloadBytes.find(id);
    Q_ASSERT(payload != m_payloadBytes.end());
    m_capacityLedger->release(existing->sourceService,
                              payload.value());
    m_payloadBytes.erase(payload);
    m_active.erase(existing);
    ++m_revision;
    m_snapshot = buildSnapshot(m_revision);
    publishModel();
    dispatchClosed({NotificationCloseEvent{id,
                                           owner,
                                           CloseReason::DismissedByUser,
                                           m_revision}});

    NotificationOperationResult result;
    result.status = OperationStatus::Applied;
    result.revisionBefore = before;
    result.revisionAfter = m_revision;
    result.notificationId = id;
    result.affectedIds = {id};
    return result;
}

NotificationOperationResult NotificationService::invokeAction(
    quint32 id,
    const QString &actionKey,
    const QString &activationToken)
{
    if (m_dispatching) {
        return reject(OperationStatus::ReentrantOperation,
                      QStringLiteral("backend callbacks cannot mutate notification state"),
                      id);
    }
    if (!isReady()) {
        return reject(OperationStatus::InvalidPolicy, m_initializationError, id);
    }

    QString error;
    if (id == 0
        || !Private::validateActionInvocation(actionKey, activationToken, &error)) {
        return reject(OperationStatus::InvalidRequest,
                      id == 0 ? QStringLiteral("notification id must be nonzero") : error,
                      id);
    }
    const auto existing = m_active.find(id);
    if (existing == m_active.end()) {
        return reject(OperationStatus::NotFound,
                      QStringLiteral("notification id is not active"),
                      id);
    }
    const bool actionExists = std::any_of(existing->actions.cbegin(),
                                          existing->actions.cend(),
                                          [&actionKey](const NotificationAction &action) {
                                              return action.key == actionKey;
                                          });
    if (!actionExists) {
        return reject(OperationStatus::UnknownAction,
                      QStringLiteral("notification does not expose the requested action"),
                      id);
    }

    const bool closeAfterAction = m_policy.closeNonResidentAfterAction
        && !existing->hints.resident;
    if (closeAfterAction && !canAdvanceRevision()) {
        return reject(OperationStatus::RevisionExhausted,
                      QStringLiteral("notification revision is exhausted"),
                      id);
    }

    const quint64 before = m_revision;
    const QString owner = existing->sourceService;
    if (closeAfterAction) {
        const auto payload = m_payloadBytes.find(id);
        Q_ASSERT(payload != m_payloadBytes.end());
        m_capacityLedger->release(existing->sourceService,
                                  payload.value());
        m_payloadBytes.erase(payload);
        m_active.erase(existing);
        ++m_revision;
        m_snapshot = buildSnapshot(m_revision);
    }

    const NotificationActionEvent actionEvent{
        id,
        owner,
        actionKey,
        activationToken,
        m_revision,
    };
    QScopedValueRollback guard(m_dispatching, true);
    m_backend.actionInvoked(actionEvent);
    if (closeAfterAction) {
        m_backend.modelPublished(m_snapshot);
        m_backend.notificationClosed(NotificationCloseEvent{
            id,
            owner,
            CloseReason::DismissedByUser,
            m_revision,
        });
    }

    NotificationOperationResult result;
    result.status = OperationStatus::Applied;
    result.revisionBefore = before;
    result.revisionAfter = m_revision;
    result.notificationId = id;
    result.affectedIds = {id};
    return result;
}

NotificationOperationResult NotificationService::expireDue()
{
    if (m_dispatching) {
        return reject(OperationStatus::ReentrantOperation,
                      QStringLiteral("backend callbacks cannot mutate notification state"));
    }
    if (!isReady()) {
        return reject(OperationStatus::InvalidPolicy, m_initializationError);
    }

    QString error;
    const auto now = checkedNow(&error);
    if (!now.has_value()) {
        return reject(OperationStatus::ClockFailure, std::move(error));
    }

    QVector<quint32> dueIds;
    QVector<NotificationCloseEvent> events;
    for (auto iterator = m_active.cbegin(); iterator != m_active.cend(); ++iterator) {
        if (iterator->expiresAtMs.has_value() && *iterator->expiresAtMs <= *now) {
            dueIds.push_back(iterator.key());
            events.push_back(NotificationCloseEvent{
                iterator.key(),
                iterator->sourceService,
                CloseReason::Expired,
                0,
            });
        }
    }
    if (dueIds.isEmpty()) {
        m_lastNowMs = *now;
        NotificationOperationResult result;
        result.status = OperationStatus::Applied;
        result.revisionBefore = m_revision;
        result.revisionAfter = m_revision;
        return result;
    }
    if (!canAdvanceRevision()) {
        return reject(OperationStatus::RevisionExhausted,
                      QStringLiteral("notification revision is exhausted"));
    }

    const quint64 before = m_revision;
    for (const quint32 id : dueIds) {
        const auto &notification = m_active.value(id);
        const auto payload = m_payloadBytes.find(id);
        Q_ASSERT(payload != m_payloadBytes.end());
        m_capacityLedger->release(notification.sourceService,
                                  payload.value());
        m_payloadBytes.erase(payload);
        m_active.remove(id);
    }
    m_lastNowMs = *now;
    ++m_revision;
    for (auto &event : events) {
        event.revision = m_revision;
    }
    m_snapshot = buildSnapshot(m_revision);
    publishModel();
    dispatchClosed(events);

    NotificationOperationResult result;
    result.status = OperationStatus::Applied;
    result.revisionBefore = before;
    result.revisionAfter = m_revision;
    result.affectedIds = dueIds;
    return result;
}

} // namespace QindaQt::Services::Notifications
