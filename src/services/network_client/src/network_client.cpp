// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/network_client/network_client.h>

#include <qindaqt/services/network_client/network_transport.h>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_redaction.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include <utility>

namespace QindaQt::Network::Client {
namespace {

constexpr int kNoDelay = 0;

bool isBenignStaleDuplicate(const Model::NetworkModel::ApplyResult result,
                            const std::optional<Model::Lineage> &lineage,
                            const Snapshot &reply) {
    return !result.accepted
           && result.reasonCode == QStringLiteral("snapshot-revision-not-newer")
           && lineage.has_value() && lineage->owner == reply.owner
           && lineage->epoch == reply.epoch;
}

} // namespace

bool ClientTiming::isValid() const noexcept {
    if (requestTimeoutMilliseconds < kMinimumRequestTimeoutMilliseconds
        || requestTimeoutMilliseconds > kMaximumRequestTimeoutMilliseconds
        || retryMilliseconds.isEmpty()
        || retryMilliseconds.size() > kMaximumRetryDelays) {
        return false;
    }
    int previous = 0;
    for (const int delay : retryMilliseconds) {
        if (delay <= 0 || delay > kMaximumRetryDelayMilliseconds
            || delay < previous) {
            return false;
        }
        previous = delay;
    }
    return true;
}

NetworkClient::NetworkClient(NetworkTransport &transport,
                             Model::MonotonicClock clock, ClientTiming timing,
                             QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_model(clock ? Model::NetworkModel(std::move(clock))
                    : Model::NetworkModel())
    , m_timing(std::move(timing))
{
    m_refreshTimer.setSingleShot(true);
    m_timeout.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this,
            &NetworkClient::handleRefreshTimer);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (!m_request) {
            return;
        }
        if (m_request->kind == RequestKind::Operation) {
            finishOperationAsUncertain(
                QStringLiteral("network operation timed out"));
            return;
        }
        m_request.reset();
        publish(ClientState::Degraded,
                QStringLiteral("network snapshot timed out"));
        scheduleRetry();
    });
    connect(&m_transport, &NetworkTransport::ownerChanged, this,
            &NetworkClient::handleOwnerChanged);
    connect(&m_transport, &NetworkTransport::snapshotInvalidated, this,
            &NetworkClient::handleInvalidation);
    connect(&m_transport, &NetworkTransport::snapshotReceived, this,
            &NetworkClient::handleSnapshot);
    connect(&m_transport, &NetworkTransport::operationReceived, this,
            &NetworkClient::handleOperation);
    connect(&m_transport, &NetworkTransport::requestFailed, this,
            &NetworkClient::handleFailure);
    connect(&m_transport, &NetworkTransport::busDisconnected, this,
            &NetworkClient::handleBusDisconnected);
}

NetworkClient::~NetworkClient() { stop(); }

bool NetworkClient::start(QString *error) {
    if (m_started) {
        setError(error, {});
        return true;
    }
    if (!m_timing.isValid()) {
        setError(error, QStringLiteral("network client timing is invalid"));
        return false;
    }
    m_started = true;
    QString transportError;
    if (!m_transport.start(&transportError)) {
        const QString message = transportError.isEmpty()
                                    ? QStringLiteral("network transport could not start")
                                    : transportError;
        // AGENT-GUARD: A transport may fail after partial setup or synchronous
        // owner notification. Roll back every live flag and public baseline so
        // retry invokes start again and can never inherit Ready truth.
        m_started = false;
        m_transportStarted = false;
        m_refreshTimer.stop();
        m_timeout.stop();
        m_request.reset();
        m_operation.reset();
        m_model.clear();
        m_owner.clear();
        m_dirty = false;
        m_retryIndex = 0;
        m_transport.stop();
        const QString redacted = redactDiagnostic(message);
        setError(error, redacted);
        publish(ClientState::Unavailable, redacted);
        return false;
    }
    m_transportStarted = true;
    setError(error, {});
    publish(ClientState::Connecting);
    return true;
}

void NetworkClient::stop() {
    if (!m_started) {
        return;
    }
    // AGENT-GUARD: Teardown must clear every pending request before the
    // transport stops, and every later transport signal is ignored through
    // m_started; otherwise a late fake/real backend reply would mutate a torn
    // client or resurrect stale lineage as fresh truth.
    m_started = false;
    m_refreshTimer.stop();
    m_timeout.stop();
    const bool hadOperation = m_operation.has_value();
    m_request.reset();
    m_operation.reset();
    m_model.clear();
    m_owner.clear();
    m_dirty = false;
    m_retryIndex = 0;
    if (m_transportStarted) {
        m_transportStarted = false;
        m_transport.stop();
    }
    publish(ClientState::Unavailable);
    if (hadOperation) {
        Q_EMIT operationInFlightChanged();
    }
}

void NetworkClient::refresh() {
    if (!m_started) {
        return;
    }
    if (m_owner.isEmpty()) {
        publish(ClientState::Connecting);
        return;
    }
    if (m_request) {
        m_dirty = true;
        return;
    }
    m_refreshTimer.start(kNoDelay);
}

bool NetworkClient::requestScan(const qint64 deadlineMilliseconds, QString *error) {
    const Model::IntentVerdict verdict =
        m_model.requestScan(RequestScanIntent{deadlineMilliseconds});
    if (!verdict.allowed) {
        setError(error, verdict.reasonCode);
        return false;
    }
    QVariantMap parameters;
    parameters.insert(QStringLiteral("deadlineMs"), deadlineMilliseconds);
    return beginOperation(OperationKind::RequestScan, parameters, error);
}

bool NetworkClient::connectKnownNetwork(const QString &knownNetworkId,
                                        QString *error) {
    const Model::IntentVerdict verdict =
        m_model.connectKnown(ConnectIntent{knownNetworkId});
    if (!verdict.allowed) {
        setError(error, verdict.reasonCode);
        return false;
    }
    QVariantMap parameters;
    parameters.insert(QStringLiteral("knownNetworkId"), knownNetworkId);
    return beginOperation(OperationKind::ConnectKnownNetwork, parameters, error);
}

bool NetworkClient::disconnectDevice(const QString &deviceInterface,
                                     QString *error) {
    const Model::IntentVerdict verdict =
        m_model.disconnectDevice(DisconnectIntent{deviceInterface});
    if (!verdict.allowed) {
        setError(error, verdict.reasonCode);
        return false;
    }
    QVariantMap parameters;
    parameters.insert(QStringLiteral("deviceInterface"), deviceInterface);
    return beginOperation(OperationKind::DisconnectActive, parameters, error);
}

bool NetworkClient::setRadio(const RadioKind kind, const bool enable,
                             QString *error) {
    const Model::IntentVerdict verdict =
        m_model.setRadio(SetRadioIntent{kind, enable});
    if (!verdict.allowed) {
        setError(error, verdict.reasonCode);
        return false;
    }
    QVariantMap parameters;
    parameters.insert(QStringLiteral("radioKind"),
                      static_cast<qint32>(kind));
    parameters.insert(QStringLiteral("enable"), enable);
    return beginOperation(OperationKind::SetRadio, parameters, error);
}

void NetworkClient::handleOwnerChanged(const QString &owner) {
    if (!m_started || owner == m_owner) {
        return;
    }
    m_refreshTimer.stop();
    m_timeout.stop();
    abortInFlight(QStringLiteral("network service owner changed during operation"));
    // Owner replacement invalidates the whole lineage: last-confirmed data is
    // never shown as current, and A/B/A replays are fenced by the epoch gate.
    m_model.clear();
    m_owner.clear();
    m_dirty = false;
    m_retryIndex = 0;
    if (owner.isEmpty()) {
        publish(ClientState::Unavailable,
                QStringLiteral("network service is unavailable"));
        scheduleRetry();
        return;
    }
    if (!isValidUniqueOwner(owner)) {
        publish(ClientState::Degraded,
                QStringLiteral("network service owner is invalid"));
        return;
    }
    m_owner = owner;
    publish(ClientState::Connecting);
    m_refreshTimer.start(kNoDelay);
}

void NetworkClient::handleInvalidation(const QString &owner) {
    if (!m_started || owner != m_owner || !m_model.snapshot()) {
        return;
    }
    // Coalesce invalidations: one refetch per settled burst, never a rebuild
    // from the hint itself. A pending request already carries the fetch.
    if (m_request) {
        m_dirty = true;
        return;
    }
    m_refreshTimer.start(kNoDelay);
}

void NetworkClient::handleSnapshot(const quint64 token, const QString &owner,
                                   const QByteArray &payload) {
    if (!m_started || !m_request || m_request->kind != RequestKind::Snapshot
        || m_request->token != token || m_request->owner != owner
        || owner != m_owner) {
        return;
    }
    m_request.reset();
    m_timeout.stop();
    Snapshot reply;
    const DecodeResult decoded = decodeSnapshot(payload, reply);
    const bool exactPayloadOwner = decoded.succeeded() && reply.owner == owner;
    const Model::NetworkModel::ApplyResult applied =
        exactPayloadOwner
            ? m_model.applySnapshot(reply)
            : Model::NetworkModel::ApplyResult{
                  false,
                  decoded.succeeded()
                      ? QStringLiteral("snapshot-payload-owner-mismatch")
                      : decoded.reasonCode};
    if (applied.accepted) {
        m_retryIndex = 0;
        publish(ClientState::Ready);
        Q_EMIT snapshotChanged();
    } else if (isBenignStaleDuplicate(applied, m_model.lineage(), reply)) {
        // Out-of-order duplicate of the already-current lineage: not an error.
        m_retryIndex = 0;
        publish(ClientState::Ready);
    } else {
        publish(ClientState::Degraded,
                QStringLiteral("network snapshot is malformed or regressed"));
        scheduleRetry();
    }
    if (m_dirty) {
        m_dirty = false;
        m_refreshTimer.start(kNoDelay);
    }
}

void NetworkClient::handleOperation(const quint64 token, const QString &owner,
                                    const QByteArray &payload) {
    if (!m_started || !m_request || m_request->kind != RequestKind::Operation
        || !m_operation || m_request->token != token
        || m_request->owner != owner || owner != m_owner) {
        return;
    }
    const Request request = *m_request;
    m_request.reset();
    m_timeout.stop();
    m_operation.reset();
    Q_EMIT operationInFlightChanged();
    OperationResult result;
    if (!decodeOperationResult(payload, result).succeeded()) {
        finishOperationAsUncertain(
            QStringLiteral("network operation reply is malformed"));
        return;
    }
    if (result.initiatingEpoch != request.epoch
        || result.initiatingRevision != request.revision
        || result.kind != request.operationKind) {
        // A reply that does not carry the initiating lineage is not evidence
        // about this operation; it is reported uncertain, never guessed.
        finishOperationAsUncertain(
            QStringLiteral("network operation reply lineage mismatch"));
        return;
    }
    Q_EMIT operationFinished(result);
    // The authoritative revision follows as an invalidation; fetch it instead
    // of manufacturing state from the operation reply itself.
    refresh();
}

void NetworkClient::handleFailure(const quint64 token, const QString &owner,
                                  const QString &errorName,
                                  const QString &message) {
    if (!m_started || !m_request || m_request->token != token
        || m_request->owner != owner || owner != m_owner) {
        return;
    }
    if (m_request->kind == RequestKind::Operation) {
        finishOperationAsUncertain(
            message.isEmpty()
                ? QStringLiteral("network operation transport failed: %1").arg(
                      errorName)
                : message);
        return;
    }
    m_request.reset();
    m_timeout.stop();
    publish(ClientState::Degraded,
            QStringLiteral("network snapshot transport failed: %1").arg(
                errorName));
    scheduleRetry();
}

void NetworkClient::handleBusDisconnected() {
    if (!m_started) {
        return;
    }
    m_refreshTimer.stop();
    m_timeout.stop();
    abortInFlight(QStringLiteral("network transport disconnected"));
    m_model.clear();
    m_owner.clear();
    m_transportStarted = false;
    publish(ClientState::Unavailable, QStringLiteral("network transport disconnected"));
}

void NetworkClient::handleRefreshTimer() { requestSnapshotNow(); }

void NetworkClient::requestSnapshotNow() {
    if (!m_started || m_owner.isEmpty() || m_request) {
        return;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        publish(ClientState::Degraded,
                QStringLiteral("network request token is exhausted"));
        return;
    }
    m_dirty = false;
    m_request = Request{token, m_owner, RequestKind::Snapshot, 0, 0,
                        OperationKind::RequestScan};
    m_timeout.start(m_timing.requestTimeoutMilliseconds);
    m_transport.requestSnapshot(token, m_owner);
}

bool NetworkClient::beginOperation(const OperationKind kind,
                                   const QVariantMap &parameters,
                                   QString *error) {
    if (m_state != ClientState::Ready || !m_model.snapshot()
        || m_request || m_operation) {
        setError(error,
                 m_operation || (m_request && m_request->kind == RequestKind::Operation)
                     ? QStringLiteral("operation-in-flight")
                     : QStringLiteral("client-not-ready"));
        return false;
    }
    if (wireContainsSecrets(parameters)) {
        // Structural defense: even a caller bug can never transport a
        // credential-bearing parameter map through this boundary.
        setError(error, QStringLiteral("operation-parameters-contain-secrets"));
        return false;
    }
    const quint64 token = nextToken();
    if (token == 0) {
        setError(error, QStringLiteral("network request token is exhausted"));
        return false;
    }
    const auto lineage = m_model.lineage();
    m_operation = Operation{kind, lineage->epoch, lineage->revision};
    m_request = Request{token, m_owner, RequestKind::Operation, lineage->epoch,
                        lineage->revision, kind};
    m_timeout.start(m_timing.requestTimeoutMilliseconds);
    Q_EMIT operationInFlightChanged();
    m_transport.requestOperation(token, m_owner, lineage->epoch,
                                 lineage->revision, kind, parameters);
    return true;
}

void NetworkClient::finishOperationAsUncertain(const QString &message) {
    m_timeout.stop();
    m_request.reset();
    const bool hadOperation = m_operation.has_value();
    m_operation.reset();
    if (hadOperation) {
        Q_EMIT operationInFlightChanged();
    }
    publish(ClientState::Degraded, message);
    Q_EMIT operationUncertain(m_lastError);
    refresh();
}

void NetworkClient::abortInFlight(const QString &reason) {
    m_timeout.stop();
    m_request.reset();
    const bool hadOperation = m_operation.has_value();
    m_operation.reset();
    if (hadOperation) {
        Q_EMIT operationInFlightChanged();
        Q_EMIT operationUncertain(redactDiagnostic(reason));
    }
}

void NetworkClient::scheduleRetry() {
    if (!m_started) {
        return;
    }
    const qsizetype last = m_timing.retryMilliseconds.size() - 1;
    const qsizetype index = qMin(m_retryIndex, last);
    if (m_retryIndex < last) {
        ++m_retryIndex;
    }
    m_refreshTimer.start(m_timing.retryMilliseconds.at(index));
}

void NetworkClient::publish(const ClientState state, QString error) {
    QString redacted = redactDiagnostic(std::move(error));
    if (m_state == state && m_lastError == redacted) {
        return;
    }
    m_state = state;
    m_lastError = std::move(redacted);
    Q_EMIT stateChanged();
}

void NetworkClient::setError(QString *output, QString message) const {
    if (output != nullptr) {
        *output = std::move(message);
    }
}

quint64 NetworkClient::nextToken() {
    if (m_nextToken == 0) {
        return 0;
    }
    const quint64 result = m_nextToken++;
    if (m_nextToken == 0) {
        m_nextToken = 0;
    }
    return result;
}

} // namespace QindaQt::Network::Client
