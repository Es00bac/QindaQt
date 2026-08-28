// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_client/client.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include <QtCore/QMetaType>

#include <limits>

namespace QindaQt::DisplayClient {
Client::Client(DisplayTransport *transport, QObject *parent)
    : QObject(parent), m_transport(transport) {
  Q_ASSERT(m_transport != nullptr);
  qRegisterMetaType<ClientState>();
  qRegisterMetaType<Display::Snapshot>();
  qRegisterMetaType<Display::OperationResult>();

  m_fetchTimer.setSingleShot(true);
  m_operationTimer.setSingleShot(true);
  m_retryTimer.setSingleShot(true);

  connect(m_transport, &DisplayTransport::ownerChanged, this,
          &Client::acceptOwner);
  connect(m_transport, &DisplayTransport::invalidated, this,
          &Client::acceptInvalidation);
  connect(m_transport, &DisplayTransport::snapshotReply, this,
          &Client::acceptSnapshotReply);
  connect(m_transport, &DisplayTransport::operationReply, this,
          &Client::acceptOperationReply);
  connect(m_transport, &DisplayTransport::activationFinished, this,
          &Client::acceptActivationFinished);

  connect(&m_fetchTimer, &QTimer::timeout, this, &Client::onFetchTimeout);
  connect(&m_operationTimer, &QTimer::timeout, this,
          &Client::onOperationTimeout);
  connect(&m_retryTimer, &QTimer::timeout, this, [this]() {
    if (m_state == ClientState::Stopped) {
      return;
    }
    if (m_owner.isEmpty() && !m_activationInFlight) {
      m_activationInFlight = true;
      m_transport->requestActivation();
    } else {
      requestSnapshot();
    }
  });
}

void Client::start() {
  if (m_state != ClientState::Stopped) {
    return;
  }
  m_owner.clear();
  m_snapshot.reset();
  m_operation.reset();
  m_fetchInFlight = false;
  m_refetchNeeded = false;
  m_activationInFlight = false;
  m_announcedEpoch.clear();
  m_fetchTimer.stop();
  m_operationTimer.stop();
  m_retryTimer.stop();

  publishState(ClientState::Starting, {});
  m_transport->start();
}

void Client::stop() {
  if (m_state == ClientState::Stopped) {
    return;
  }

  // AGENT-GUARD: set the terminal state before touching the transport. The
  // transport's stop() can synchronously re-emit ownerChanged (owner ->
  // empty); every accept*/on*Timeout handler below gates on m_state ==
  // Stopped so that reentrant delivery cannot smuggle in an extra
  // stateChanged before the one this function emits itself.
  m_state = ClientState::Stopped;
  m_reasonCode.clear();

  m_fetchTimer.stop();
  m_operationTimer.stop();
  m_retryTimer.stop();
  m_fetchInFlight = false;
  m_refetchNeeded = false;
  if (m_operation.has_value()) {
    completeUncertain(QStringLiteral("client-stopped"));
  }

  m_transport->stop();

  m_owner.clear();
  m_snapshot.reset();
  m_announcedEpoch.clear();
  m_activationInFlight = false;

  Q_EMIT stateChanged(ClientState::Stopped, {});
}

void Client::refresh() {
  if (m_state == ClientState::Stopped) {
    return;
  }
  requestSnapshot();
}

ClientState Client::state() const noexcept { return m_state; }

QString Client::reasonCode() const { return m_reasonCode; }

QString Client::owner() const { return m_owner; }

bool Client::hasSnapshot() const noexcept { return m_snapshot.has_value(); }

std::optional<Display::Snapshot> Client::snapshot() const { return m_snapshot; }

bool Client::operationPending() const noexcept {
  return m_operation.has_value();
}

void Client::setRequestTimeout(int milliseconds) {
  m_requestTimeoutMs = qBound(10, milliseconds, 60'000);
}

quint64 Client::stage(const QString &transactionId,
                      const Display::Candidate &candidate) {
  if (m_state == ClientState::Stopped) {
    return rejectLocally(OperationKind::Stage,
                         QStringLiteral("client-not-running"));
  }
  if (operationPending()) {
    return rejectLocally(OperationKind::Stage,
                         QStringLiteral("operation-pending"));
  }
  if (!m_snapshot.has_value()) {
    return rejectLocally(OperationKind::Stage, QStringLiteral("no-snapshot"));
  }
  if (transactionId.isEmpty() ||
      !Display::isBoundedText(transactionId,
                              Display::kMaxTransactionIdUtf8Bytes)) {
    return rejectLocally(OperationKind::Stage,
                         QStringLiteral("invalid-transaction-id"));
  }
  if (const auto validation = Display::validateCandidate(candidate);
      !validation.accepted) {
    return rejectLocally(OperationKind::Stage,
                         QStringLiteral("invalid-candidate"));
  }
  // AGENT-GUARD: authenticate owner/epoch/revision before this candidate is
  // ever forwarded. A candidate whose lineage does not exactly match the
  // held snapshot is stale (or was built against a replaced A/B/A owner)
  // and must never reach the transport.
  if (candidate.baseEpoch != m_snapshot->serviceEpoch ||
      candidate.baseRevision != m_snapshot->revision) {
    return rejectLocally(OperationKind::Stage,
                         QStringLiteral("stale-revision"));
  }

  const quint64 requestId = beginOperation(OperationKind::Stage, transactionId);
  if (requestId == 0) {
    return 0;
  }
  m_transport->submitStage(m_owner, requestId, transactionId, candidate);
  return requestId;
}

quint64 Client::preview(const QString &transactionId) {
  if (m_state == ClientState::Stopped) {
    return rejectLocally(OperationKind::Preview,
                         QStringLiteral("client-not-running"));
  }
  if (operationPending()) {
    return rejectLocally(OperationKind::Preview,
                         QStringLiteral("operation-pending"));
  }
  if (!m_snapshot.has_value()) {
    return rejectLocally(OperationKind::Preview, QStringLiteral("no-snapshot"));
  }
  if (transactionId.isEmpty() ||
      !Display::isBoundedText(transactionId,
                              Display::kMaxTransactionIdUtf8Bytes)) {
    return rejectLocally(OperationKind::Preview,
                         QStringLiteral("invalid-transaction-id"));
  }

  const quint64 requestId =
      beginOperation(OperationKind::Preview, transactionId);
  if (requestId == 0) {
    return 0;
  }
  m_transport->submitPreview(m_owner, requestId, transactionId);
  return requestId;
}

quint64 Client::confirm(const QString &transactionId) {
  if (m_state == ClientState::Stopped) {
    return rejectLocally(OperationKind::Confirm,
                         QStringLiteral("client-not-running"));
  }
  if (operationPending()) {
    return rejectLocally(OperationKind::Confirm,
                         QStringLiteral("operation-pending"));
  }
  if (!m_snapshot.has_value()) {
    return rejectLocally(OperationKind::Confirm, QStringLiteral("no-snapshot"));
  }
  if (transactionId.isEmpty() ||
      !Display::isBoundedText(transactionId,
                              Display::kMaxTransactionIdUtf8Bytes)) {
    return rejectLocally(OperationKind::Confirm,
                         QStringLiteral("invalid-transaction-id"));
  }

  const quint64 requestId =
      beginOperation(OperationKind::Confirm, transactionId);
  if (requestId == 0) {
    return 0;
  }
  m_transport->submitConfirm(m_owner, requestId, transactionId);
  return requestId;
}

quint64 Client::cancel(const QString &transactionId) {
  // AGENT-GUARD: cancel() is deliberately idempotent and never blocked by
  // operationPending(): a caller must always be able to attempt to abort.
  // A currently in-flight operation is superseded (completed Uncertain, its
  // eventual late reply is then dropped by requestId mismatch) rather than
  // left to block this call.
  if (m_state == ClientState::Stopped) {
    return rejectLocally(OperationKind::Cancel,
                         QStringLiteral("client-not-running"));
  }
  if (!m_snapshot.has_value()) {
    return rejectLocally(OperationKind::Cancel, QStringLiteral("no-snapshot"));
  }
  if (transactionId.isEmpty() ||
      !Display::isBoundedText(transactionId,
                              Display::kMaxTransactionIdUtf8Bytes)) {
    return rejectLocally(OperationKind::Cancel,
                         QStringLiteral("invalid-transaction-id"));
  }
  if (m_operation.has_value()) {
    completeUncertain(QStringLiteral("superseded-by-cancel"));
  }

  const quint64 requestId =
      beginOperation(OperationKind::Cancel, transactionId);
  if (requestId == 0) {
    return 0;
  }
  m_transport->submitCancel(m_owner, requestId, transactionId);
  return requestId;
}

void Client::acceptOwner(const QString &owner) {
  if (m_state == ClientState::Stopped) {
    return;
  }
  const bool sameOwner = owner == m_owner;

  if (owner.isEmpty()) {
    if (!sameOwner) {
      m_owner.clear();
      m_snapshot.reset();
      m_announcedEpoch.clear();
      m_fetchInFlight = false;
      m_refetchNeeded = false;
      m_fetchTimer.stop();
      m_retryTimer.stop();
      if (m_operation.has_value()) {
        completeUncertain(QStringLiteral("owner-changed"));
      }
    }
    publishState(ClientState::Unavailable, QStringLiteral("owner-unavailable"));
    if (!m_activationInFlight) {
      m_activationInFlight = true;
      m_transport->requestActivation();
    }
    return;
  }
  if (sameOwner) {
    return;
  }

  m_owner = owner;
  m_snapshot.reset();
  m_announcedEpoch.clear();
  m_activationInFlight = false;
  m_fetchInFlight = false;
  m_refetchNeeded = false;
  m_fetchTimer.stop();
  m_retryTimer.stop();

  if (m_operation.has_value()) {
    completeUncertain(QStringLiteral("owner-changed"));
  }

  publishState(ClientState::Starting, {});
  requestSnapshot();
}

void Client::acceptInvalidation(const QString &owner, const QString &epoch,
                                quint64 revision, bool available) {
  Q_UNUSED(revision)

  if (m_state == ClientState::Stopped) {
    return;
  }
  // AGENT-GUARD: an invalidation naming a different owner than the one this
  // client currently binds to is stale (a signal from a replaced or
  // about-to-be-replaced owner) and must never trigger a refetch or an
  // availability transition here; acceptOwner() already owns that path.
  if (owner.isEmpty() || owner != m_owner) {
    return;
  }

  if (!available) {
    m_snapshot.reset();
    m_announcedEpoch.clear();
    m_fetchTimer.stop();
    m_fetchInFlight = false;
    m_refetchNeeded = false;
    if (m_operation.has_value()) {
      completeUncertain(QStringLiteral("service-unavailable"));
    }
    publishState(ClientState::Unavailable,
                 QStringLiteral("service-unavailable"));
    return;
  }

  if (!Display::isBoundedText(epoch, Display::kMaxServiceEpochUtf8Bytes) ||
      epoch.isEmpty()) {
    publishState(operationPending() ? ClientState::Busy : ClientState::Degraded,
                 QStringLiteral("malformed-invalidation"));
    return;
  }
  // Changed() remains only a complete-read invalidation. Remembering its
  // bounded epoch fences the next full reply; no state is reconstructed from
  // the signal payload.
  m_announcedEpoch = epoch;
  requestSnapshot();
}

void Client::acceptActivationFinished(bool success, const QString &reasonCode) {
  if (m_state == ClientState::Stopped || !m_activationInFlight) {
    return;
  }
  m_activationInFlight = false;
  if (m_owner.isEmpty()) {
    publishState(ClientState::Unavailable,
                 success ? QStringLiteral("activation-awaiting-owner")
                         : reasonCode);
    m_retryTimer.start(2'000);
  }
}

void Client::onFetchTimeout() {
  if (m_state == ClientState::Stopped || !m_fetchInFlight) {
    return;
  }
  m_fetchInFlight = false;
  const bool unavailable = m_owner.isEmpty();
  scheduleRefetch();
  publishState(operationPending() ? ClientState::Busy
                                  : (unavailable ? ClientState::Unavailable
                                                 : ClientState::Degraded),
               QStringLiteral("transport-timeout"));
}

void Client::onOperationTimeout() {
  if (m_state == ClientState::Stopped || !m_operation.has_value()) {
    return;
  }
  completeUncertain(QStringLiteral("transport-timeout"));
  publishState(m_owner.isEmpty() ? ClientState::Unavailable
                                 : ClientState::Degraded,
               QStringLiteral("transport-timeout"));
  // A mutation timeout never authorizes replay, but the old snapshot is no
  // longer proof of live topology. A complete read is the only safe path back
  // to Ready.
  requestSnapshot();
}

void Client::publishState(ClientState state, const QString &reasonCode) {
  if (m_state == state && m_reasonCode == reasonCode) {
    return;
  }
  m_state = state;
  m_reasonCode = reasonCode;
  Q_EMIT stateChanged(state, reasonCode);
}

void Client::publishSnapshotState(const Display::Snapshot &snapshot) {
  m_snapshot = snapshot;
  Q_EMIT snapshotChanged(snapshot);
}

void Client::requestSnapshot() {
  if (m_state == ClientState::Stopped || m_owner.isEmpty()) {
    return;
  }
  if (m_fetchInFlight) {
    m_refetchNeeded = true;
    return;
  }
  if (m_nextRequestId == 0 ||
      m_nextRequestId == std::numeric_limits<quint64>::max()) {
    publishState(operationPending() ? ClientState::Busy : ClientState::Degraded,
                 QStringLiteral("request-id-exhausted"));
    return;
  }
  m_fetchInFlight = true;
  m_refetchNeeded = false;
  m_fetchRequestId = m_nextRequestId++;
  m_fetchTimer.start(m_requestTimeoutMs);
  m_transport->fetchSnapshot(m_owner, m_fetchRequestId);
}

void Client::scheduleRefetch() {
  if (m_state == ClientState::Stopped || m_owner.isEmpty()) {
    return;
  }
  m_retryTimer.start(2'000);
}

quint64 Client::beginOperation(OperationKind kind,
                               const QString &transactionId) {
  if (m_nextRequestId == 0 ||
      m_nextRequestId == std::numeric_limits<quint64>::max()) {
    return 0;
  }
  const quint64 requestId = m_nextRequestId++;
  PendingOperation op;
  op.requestId = requestId;
  op.kind = kind;
  op.transactionId = transactionId;
  op.epochAtSubmit = m_snapshot ? m_snapshot->serviceEpoch : QString();
  op.revisionAtSubmit = m_snapshot ? m_snapshot->revision : 0;
  m_operation = op;
  m_operationTimer.start(m_requestTimeoutMs);
  publishState(ClientState::Busy, {});
  return requestId;
}

quint64 Client::rejectLocally(OperationKind kind, const QString &reasonCode) {
  if (m_nextRequestId == 0 ||
      m_nextRequestId == std::numeric_limits<quint64>::max()) {
    return 0;
  }
  const quint64 requestId = m_nextRequestId++;
  queueOperationCompletion(
      requestId,
      localResult(kind, Display::OperationStatus::Rejected, reasonCode));
  return requestId;
}

void Client::completeUncertain(const QString &reasonCode) {
  if (!m_operation.has_value()) {
    return;
  }
  const PendingOperation op = *m_operation;
  m_operation.reset();
  m_operationTimer.stop();
  Display::OperationResult result =
      localResult(op.kind, Display::OperationStatus::Uncertain, reasonCode);
  // AGENT-GUARD: owner loss and explicit unavailability clear m_snapshot
  // before this path can run. The final result still names the exact lineage
  // and transaction that was submitted; never reconstruct it from current
  // client state.
  result.initiatingEpoch = op.epochAtSubmit;
  result.initiatingRevision = op.revisionAtSubmit;
  result.observedRevision = op.revisionAtSubmit;
  result.transactionId = op.transactionId;
  queueOperationCompletion(op.requestId, result);
}

ClientState Client::baseState() const noexcept {
  if (m_owner.isEmpty()) {
    return ClientState::Unavailable;
  }
  if (!m_snapshot.has_value()) {
    return ClientState::Starting;
  }
  return ClientState::Ready;
}

} // namespace QindaQt::DisplayClient
