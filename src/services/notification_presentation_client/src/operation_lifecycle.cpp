// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"

#include "operation_validation.h"
#include "qindaqt/services/notification_presentation/wire_contract.h"
#include "qindaqt/services/notification_presentation_client/presentation_transport.h"

#include <algorithm>
#include <utility>

namespace QindaQt::Services::NotificationPresentationClient {
namespace {

constexpr auto AccessDenied = "org.freedesktop.DBus.Error.AccessDenied";

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

void NotificationPresentationClient::scheduleOperationRecovery()
{
    if (!m_started || m_owner.isEmpty() || !m_authenticated) {
        return;
    }
    if (m_request) {
        // AGENT-GUARD: the in-flight snapshot may predate an uncertain
        // operation. Force a second fetch before treating it as authoritative.
        m_dirty = true;
    } else {
        scheduleRequest(0);
    }
}

bool NotificationPresentationClient::dismiss(quint32 id, QString *error)
{
    bool targetResident = false;
    if (!validateOperation(id, nullptr, nullptr, &targetResident, error)) {
        return false;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        setError(error, QStringLiteral("notification operation token is exhausted"));
        return false;
    }
    m_operation = InFlightOperation{token, m_owner, id, m_snapshot->revision,
                                    OperationKind::Dismiss, targetResident};
    Q_EMIT operationInFlightChanged();
    m_operationTimeout.start(m_timing.requestTimeoutMilliseconds);
    m_transport.dismiss(token, m_owner, id);
    setError(error, {});
    return true;
}

bool NotificationPresentationClient::invokeAction(
    quint32 id, const QString &actionKey, const QString &activationToken,
    QString *error)
{
    bool targetResident = false;
    if (!validateOperation(id, &actionKey, &activationToken,
                           &targetResident, error)) {
        return false;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        setError(error, QStringLiteral("notification operation token is exhausted"));
        return false;
    }
    m_operation = InFlightOperation{token, m_owner, id, m_snapshot->revision,
                                    OperationKind::Action, targetResident};
    Q_EMIT operationInFlightChanged();
    m_operationTimeout.start(m_timing.requestTimeoutMilliseconds);
    m_transport.invokeAction(token, m_owner, id, actionKey, activationToken);
    setError(error, {});
    return true;
}

bool NotificationPresentationClient::validateOperation(
    quint32 id, const QString *actionKey, const QString *activationToken,
    bool *targetResident, QString *error) const
{
    if (!m_started || m_state != ClientState::Ready || !m_authenticated ||
        !m_snapshot || m_operation) {
        setError(error, QStringLiteral("notification presenter is not ready"));
        return false;
    }
    const auto item = std::find_if(
        m_snapshot->notifications.cbegin(), m_snapshot->notifications.cend(),
        [id](const auto &notification) { return notification.id == id; });
    if (id == 0 || item == m_snapshot->notifications.cend()) {
        setError(error, QStringLiteral("notification is not in the current snapshot"));
        return false;
    }
    *targetResident = item->resident;
    if (actionKey != nullptr) {
        const auto action = std::find_if(
            item->actions.cbegin(), item->actions.cend(),
            [actionKey](const auto &candidate) { return candidate.key == *actionKey; });
        if (action == item->actions.cend() || activationToken == nullptr ||
            (!activationToken->isEmpty() &&
             !Private::validBoundedText(
                 *activationToken,
                 NotificationPresentation::WireContract::MaximumActivationTokenBytes))) {
            setError(error, QStringLiteral("notification action request is invalid"));
            return false;
        }
    }
    return true;
}

void NotificationPresentationClient::handleOperationResult(
    quint64 token, const QString &uniqueOwner, const QVariantMap &result)
{
    if (!m_started || !m_operation || token != m_operation->token ||
        uniqueOwner != m_operation->owner || uniqueOwner != m_owner) {
        return;
    }
    const InFlightOperation operation = *m_operation;
    const quint32 id = operation.notificationId;
    m_operationTimeout.stop();
    m_operation.reset();
    Q_EMIT operationInFlightChanged();
    quint64 revisionAfter = 0;
    if (!Private::validOperationResult(
            result, id, operation.initiatingRevision,
            operation.kind == OperationKind::Action && operation.targetResident,
            &revisionAfter)) {
        Q_EMIT operationRejected(
            id, QStringLiteral("notification operation reply is invalid"));
        scheduleOperationRecovery();
        return;
    }
    m_targetRevision = std::max(m_targetRevision, revisionAfter);
    Q_EMIT operationSucceeded(id);
    if (!m_request) {
        scheduleRequest(0);
    } else {
        m_dirty = true;
    }
}

void NotificationPresentationClient::handleOperationFailure(
    quint64 token, const QString &uniqueOwner, const QString &errorName,
    const QString &message)
{
    if (!m_started || !m_operation || token != m_operation->token ||
        uniqueOwner != m_operation->owner || uniqueOwner != m_owner) {
        return;
    }
    const quint32 id = m_operation->notificationId;
    const bool authorizationLost = errorName == QLatin1String(AccessDenied);
    m_operationTimeout.stop();
    m_operation.reset();
    if (authorizationLost) {
        // AGENT-GUARD: an operation denial invalidates every request made with
        // that presenter binding. Only a fresh Register reply may restore it.
        m_refreshTimer.stop();
        m_requestTimeout.stop();
        m_request.reset();
        m_authenticated = false;
        m_snapshot.reset();
        m_dirty = false;
        m_targetRevision = 0;
        publish(ClientState::Authenticating,
                QStringLiteral("notification presenter authorization was lost"));
        scheduleRequest(0);
    }
    Q_EMIT operationInFlightChanged();
    Q_EMIT operationRejected(
        id, Private::normalizedOperationError(
                message, QStringLiteral("notification operation failed")));
    if (!authorizationLost) {
        scheduleOperationRecovery();
    }
}

} // namespace QindaQt::Services::NotificationPresentationClient
