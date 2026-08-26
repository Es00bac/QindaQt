// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"

#include "qindaqt/services/notification_presentation_client/presentation_transport.h"

#include <algorithm>
#include <utility>

namespace QindaQt::Services::NotificationPresentationClient {
namespace {

constexpr int MaximumRuntimeDelayMilliseconds = 60'000;
constexpr auto AccessDenied = "org.freedesktop.DBus.Error.AccessDenied";

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

bool ClientTiming::isValid() const noexcept
{
    if (debounceMilliseconds < 0 ||
        debounceMilliseconds > MaximumRuntimeDelayMilliseconds ||
        requestTimeoutMilliseconds <= 0 ||
        requestTimeoutMilliseconds > MaximumRuntimeDelayMilliseconds ||
        retryMilliseconds.isEmpty()) {
        return false;
    }
    int previous = 0;
    for (const int delay : retryMilliseconds) {
        if (delay <= 0 || delay > MaximumRuntimeDelayMilliseconds || delay < previous) {
            return false;
        }
        previous = delay;
    }
    return true;
}

NotificationPresentationClient::NotificationPresentationClient(
    PresentationTransport &transport,
    NotificationPresentation::PresentationAccessToken accessToken,
    ClientTiming timing,
    QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_accessToken(std::move(accessToken))
    , m_timing(std::move(timing))
{
    m_refreshTimer.setSingleShot(true);
    m_requestTimeout.setSingleShot(true);
    m_operationTimeout.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this,
            &NotificationPresentationClient::requestNow);
    connect(&m_requestTimeout, &QTimer::timeout, this, [this] {
        failCurrentRequest(QStringLiteral("notification snapshot request timed out"),
                           false);
    });
    connect(&m_operationTimeout, &QTimer::timeout, this, [this] {
        if (!m_operation) {
            return;
        }
        const quint32 id = m_operation->notificationId;
        m_operation.reset();
        Q_EMIT operationInFlightChanged();
        Q_EMIT operationRejected(id,
                                 QStringLiteral("notification operation timed out"));
        scheduleOperationRecovery();
    });
    connect(&m_transport, &PresentationTransport::serviceOwnerChanged, this,
            &NotificationPresentationClient::handleOwnerChanged);
    connect(&m_transport, &PresentationTransport::snapshotInvalidated, this,
            &NotificationPresentationClient::handleInvalidation);
    connect(&m_transport, &PresentationTransport::snapshotReceived, this,
            &NotificationPresentationClient::handleSnapshot);
    connect(&m_transport, &PresentationTransport::requestFailed, this,
            &NotificationPresentationClient::handleRequestFailure);
    connect(&m_transport, &PresentationTransport::operationFinished, this,
            &NotificationPresentationClient::handleOperationResult);
    connect(&m_transport, &PresentationTransport::operationFailed, this,
            &NotificationPresentationClient::handleOperationFailure);
}

NotificationPresentationClient::~NotificationPresentationClient()
{
    stop();
}

bool NotificationPresentationClient::start(QString *error)
{
    if (m_started) {
        setError(error, {});
        return true;
    }
    if (!m_timing.isValid()) {
        setError(error, QStringLiteral("notification client timing is invalid"));
        return false;
    }
    m_started = true;
    if (!m_transport.start(error)) {
        m_started = false;
        return false;
    }
    setError(error, {});
    return true;
}

void NotificationPresentationClient::stop()
{
    if (!m_started) {
        return;
    }
    if (m_authenticated && !m_owner.isEmpty()) {
        m_transport.releasePresenter(m_owner);
    }
    m_started = false;
    m_refreshTimer.stop();
    m_requestTimeout.stop();
    m_operationTimeout.stop();
    m_request.reset();
    const bool operationWasInFlight = m_operation.has_value();
    m_operation.reset();
    m_snapshot.reset();
    m_owner.clear();
    m_lastError.clear();
    m_authenticated = false;
    m_dirty = false;
    m_targetRevision = 0;
    m_retryIndex = 0;
    m_state = ClientState::Unavailable;
    m_transport.stop();
    if (operationWasInFlight) {
        Q_EMIT operationInFlightChanged();
    }
}

ClientState NotificationPresentationClient::state() const noexcept
{
    return m_state;
}

const QString &NotificationPresentationClient::lastError() const noexcept
{
    return m_lastError;
}

const std::optional<NotificationPresentation::PresentationSnapshot> &
NotificationPresentationClient::snapshot() const noexcept
{
    return m_snapshot;
}

bool NotificationPresentationClient::requestInFlight() const noexcept
{
    return m_request.has_value();
}

bool NotificationPresentationClient::operationInFlight() const noexcept
{
    return m_operation.has_value();
}

void NotificationPresentationClient::handleOwnerChanged(const QString &uniqueOwner)
{
    if (!m_started || uniqueOwner == m_owner) {
        return;
    }
    m_refreshTimer.stop();
    m_requestTimeout.stop();
    m_operationTimeout.stop();
    m_request.reset();
    const std::optional<InFlightOperation> interruptedOperation = m_operation;
    m_operation.reset();
    m_snapshot.reset();
    m_authenticated = false;
    m_dirty = false;
    m_targetRevision = 0;
    m_retryIndex = 0;
    m_owner = uniqueOwner;
    if (m_owner.isEmpty()) {
        publish(ClientState::Unavailable);
    } else {
        publish(ClientState::Authenticating);
        scheduleRequest(0);
    }
    if (interruptedOperation) {
        // AGENT-GUARD: publish the new lineage before callbacks can retry.
        Q_EMIT operationInFlightChanged();
        Q_EMIT operationRejected(
            interruptedOperation->notificationId,
            QStringLiteral("notification service changed during operation"));
    }
}

void NotificationPresentationClient::handleInvalidation(
    const QString &uniqueOwner, const QString &epoch, quint64 revision)
{
    if (!m_started || uniqueOwner != m_owner || !m_authenticated || !m_snapshot) {
        return;
    }
    if (epoch != m_snapshot->epoch) {
        m_authenticated = false;
        m_snapshot.reset();
        publish(ClientState::Authenticating,
                QStringLiteral("notification presentation lineage changed"));
        scheduleRequest(0);
        return;
    }
    if (revision <= m_snapshot->revision) {
        return;
    }
    m_targetRevision = std::max(m_targetRevision, revision);
    if (m_request) {
        m_dirty = true;
        return;
    }
    scheduleRequest(m_timing.debounceMilliseconds);
}

void NotificationPresentationClient::handleSnapshot(
    quint64 token, const QString &uniqueOwner, const QVariantMap &wire)
{
    if (!m_started || !m_request || token != m_request->token ||
        uniqueOwner != m_request->owner || uniqueOwner != m_owner) {
        return;
    }
    const RequestKind kind = m_request->kind;
    m_request.reset();
    m_requestTimeout.stop();
    auto decoded = NotificationPresentation::PresentationSnapshotCodec::decode(wire);
    if (!decoded.ok()) {
        failCurrentRequest(decoded.error, kind == RequestKind::Register);
        return;
    }
    if (kind == RequestKind::Snapshot && m_snapshot &&
        (decoded.snapshot->epoch != m_snapshot->epoch ||
         decoded.snapshot->revision < m_snapshot->revision)) {
        const bool lineageChanged = decoded.snapshot->epoch != m_snapshot->epoch;
        failCurrentRequest(
            lineageChanged
                ? QStringLiteral("notification presentation lineage changed")
                : QStringLiteral("notification presentation revision regressed"),
            lineageChanged);
        return;
    }

    m_authenticated = true;
    m_retryIndex = 0;
    m_snapshot = std::move(*decoded.snapshot);
    m_state = ClientState::Ready;
    m_lastError.clear();
    const bool followUp = m_dirty || m_targetRevision > m_snapshot->revision;
    m_dirty = false;
    if (m_targetRevision <= m_snapshot->revision) {
        m_targetRevision = 0;
    }
    Q_EMIT stateChanged();
    if (followUp) {
        scheduleRequest(0);
    }
}

void NotificationPresentationClient::handleRequestFailure(
    quint64 token, const QString &uniqueOwner, const QString &errorName,
    const QString &message)
{
    if (!m_started || !m_request || token != m_request->token ||
        uniqueOwner != m_request->owner || uniqueOwner != m_owner) {
        return;
    }
    failCurrentRequest(message, errorName == QLatin1String(AccessDenied));
}

void NotificationPresentationClient::failCurrentRequest(
    QString message, bool authenticationFailed)
{
    m_request.reset();
    m_requestTimeout.stop();
    m_snapshot.reset();
    if (authenticationFailed) {
        m_authenticated = false;
    }
    publish(authenticationFailed ? ClientState::Authenticating
                                 : ClientState::Degraded,
            message.trimmed().isEmpty()
                ? QStringLiteral("notification presentation request failed")
                : std::move(message));
    scheduleRetry();
}

void NotificationPresentationClient::scheduleRequest(int milliseconds)
{
    if (!m_started || m_owner.isEmpty() || m_request) {
        return;
    }
    m_refreshTimer.start(milliseconds);
}

void NotificationPresentationClient::scheduleRetry()
{
    if (!m_started || m_owner.isEmpty()) {
        return;
    }
    const qsizetype last = m_timing.retryMilliseconds.size() - 1;
    const qsizetype index = std::min(m_retryIndex, last);
    if (m_retryIndex < last) {
        ++m_retryIndex;
    }
    scheduleRequest(m_timing.retryMilliseconds.at(index));
}

void NotificationPresentationClient::requestNow()
{
    if (!m_started || m_owner.isEmpty() || m_request) {
        return;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        publish(ClientState::Degraded,
                QStringLiteral("notification request token is exhausted"));
        return;
    }
    const RequestKind kind = m_authenticated ? RequestKind::Snapshot
                                             : RequestKind::Register;
    m_request = InFlightRequest{token, m_owner, kind};
    m_requestTimeout.start(m_timing.requestTimeoutMilliseconds);
    if (kind == RequestKind::Register) {
        // AGENT-GUARD: this is the sole conversion of the stored secret into a
        // transport value. Never add diagnostics around this call.
        m_transport.registerPresenter(token, m_owner, m_accessToken.toHex());
    } else {
        m_transport.requestSnapshot(token, m_owner);
    }
}

void NotificationPresentationClient::publish(ClientState state, QString error)
{
    if (m_state == state && m_lastError == error) {
        return;
    }
    m_state = state;
    m_lastError = std::move(error);
    Q_EMIT stateChanged();
}

quint64 NotificationPresentationClient::nextToken()
{
    if (m_nextToken == 0) {
        return 0;
    }
    const quint64 result = m_nextToken++;
    if (m_nextToken == 0) {
        m_nextToken = 0;
    }
    return result;
}

} // namespace QindaQt::Services::NotificationPresentationClient
