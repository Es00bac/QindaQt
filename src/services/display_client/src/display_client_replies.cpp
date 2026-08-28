// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_client/client.h>

#include <qindaqt/services/display_protocol/display_validation.h>

namespace QindaQt::DisplayClient {
namespace {

Display::ErrorCode errorCodeForReason(const QString &reasonCode) {
  if (reasonCode == QStringLiteral("stale-revision")) {
    return Display::ErrorCode::StaleRevision;
  }
  if (reasonCode == QStringLiteral("operation-pending")) {
    return Display::ErrorCode::TransactionActive;
  }
  if (reasonCode == QStringLiteral("invalid-candidate")) {
    return Display::ErrorCode::InvalidCandidate;
  }
  if (reasonCode == QStringLiteral("invalid-transaction-id")) {
    return Display::ErrorCode::InvalidCandidate;
  }
  if (reasonCode == QStringLiteral("client-not-running") ||
      reasonCode == QStringLiteral("no-snapshot")) {
    return Display::ErrorCode::InvalidTransition;
  }
  if (reasonCode == QStringLiteral("transport-timeout")) {
    return Display::ErrorCode::Timeout;
  }
  if (reasonCode == QStringLiteral("malformed-reply") ||
      reasonCode == QStringLiteral("lineage-mismatch")) {
    return Display::ErrorCode::MalformedPayload;
  }
  return Display::ErrorCode::CompositorUnavailable;
}

bool acceptsSnapshot(const Display::Snapshot &incoming,
                     const std::optional<Display::Snapshot> &current) {
  if (!current.has_value() || incoming.serviceEpoch != current->serviceEpoch) {
    return true;
  }
  if (incoming.revision < current->revision) {
    return false;
  }
  if (incoming.revision > current->revision) {
    return true;
  }
  // Display1's revision is live-output inventory lineage. Legitimate
  // transaction-machine transitions therefore update transactions[] at the
  // same revision. Accept only that narrow delta; any same-revision topology
  // or fingerprint change is a hybrid publication and is rejected.
  return incoming.protocolVersion == current->protocolVersion &&
         incoming.serviceEpoch == current->serviceEpoch &&
         incoming.liveFingerprint == current->liveFingerprint &&
         incoming.outputs == current->outputs;
}

} // namespace

void Client::acceptSnapshotReply(const QString &owner, quint64 requestId,
                                 bool transportSuccess,
                                 const Display::Snapshot &snapshot,
                                 const QString &reasonCode) {
  if (m_state == ClientState::Stopped || !m_fetchInFlight ||
      requestId != m_fetchRequestId) {
    return;
  }
  m_fetchInFlight = false;
  m_fetchTimer.stop();

  if (owner != m_owner) {
    publishState(operationPending() ? ClientState::Busy : ClientState::Degraded,
                 QStringLiteral("lineage-mismatch"));
    return;
  }
  if (!transportSuccess) {
    const bool unavailable =
        reasonCode == QStringLiteral("owner-unavailable") ||
        reasonCode == QStringLiteral("service-unavailable");
    if (!unavailable) {
      scheduleRefetch();
    } else {
      // The service or owner can disappear before its Changed/owner signal is
      // delivered. Do not expose an old snapshot beside Unavailable.
      m_snapshot.reset();
      m_announcedEpoch.clear();
    }
    publishState(operationPending() ? ClientState::Busy
                                    : (unavailable ? ClientState::Unavailable
                                                   : ClientState::Degraded),
                 reasonCode);
    return;
  }

  const auto validation = Display::validateSnapshot(snapshot);
  // AGENT-GUARD: an accepted Changed(epoch, ...) fences even the first read.
  // Checking "no current snapshot" first would let an older in-flight reply
  // establish the wrong epoch after the server already announced a new one.
  const bool expectedEpoch =
      m_announcedEpoch.isEmpty()
          ? (!m_snapshot.has_value() ||
             snapshot.serviceEpoch == m_snapshot->serviceEpoch)
          : snapshot.serviceEpoch == m_announcedEpoch;
  if (!validation.accepted || !expectedEpoch) {
    scheduleRefetch();
    publishState(operationPending() ? ClientState::Busy : ClientState::Degraded,
                 expectedEpoch ? QStringLiteral("malformed-reply")
                               : QStringLiteral("lineage-mismatch"));
    return;
  }

  if (!acceptsSnapshot(snapshot, m_snapshot)) {
    publishState(operationPending() ? ClientState::Busy : ClientState::Degraded,
                 QStringLiteral("snapshot-rejected"));
    return;
  }

  const bool exactDuplicate = m_snapshot.has_value() && snapshot == *m_snapshot;
  m_announcedEpoch.clear();
  if (!exactDuplicate) {
    publishSnapshotState(snapshot);
  }
  publishState(operationPending() ? ClientState::Busy : ClientState::Ready, {});

  if (m_refetchNeeded) {
    m_refetchNeeded = false;
    requestSnapshot();
  }
}

void Client::acceptOperationReply(const QString &owner, quint64 requestId,
                                  bool transportSuccess,
                                  const Display::OperationResult &result,
                                  const QString &reasonCode) {
  if (m_state == ClientState::Stopped || !m_operation.has_value() ||
      m_operation->requestId != requestId) {
    return;
  }

  const PendingOperation op = *m_operation;
  m_operation.reset();
  m_operationTimer.stop();

  QString failure;
  if (!transportSuccess) {
    failure =
        reasonCode.isEmpty() ? QStringLiteral("transport-error") : reasonCode;
  } else if (!Display::validateOperationResult(result).accepted) {
    failure = QStringLiteral("malformed-reply");
  } else {
    const bool success = result.status == Display::OperationStatus::Accepted ||
                         result.status == Display::OperationStatus::Succeeded;
    const bool matchingLineage =
        owner == m_owner && result.kind == publicKind(op.kind) &&
        (result.transactionId.isEmpty() ||
         result.transactionId == op.transactionId) &&
        (!success || result.initiatingEpoch == op.epochAtSubmit);
    if (!matchingLineage) {
      failure = QStringLiteral("lineage-mismatch");
    }
  }

  if (!failure.isEmpty()) {
    Display::OperationResult uncertain =
        localResult(op.kind, Display::OperationStatus::Uncertain, failure);
    uncertain.initiatingEpoch = op.epochAtSubmit;
    uncertain.initiatingRevision = op.revisionAtSubmit;
    uncertain.observedRevision = op.revisionAtSubmit;
    uncertain.transactionId = op.transactionId;
    queueOperationCompletion(requestId, uncertain);
    const bool unavailable = failure == QStringLiteral("owner-unavailable") ||
                             failure == QStringLiteral("service-unavailable");
    if (unavailable) {
      m_snapshot.reset();
      m_announcedEpoch.clear();
    }
    publishState(unavailable ? ClientState::Unavailable : ClientState::Degraded,
                 failure);
    return;
  }

  queueOperationCompletion(requestId, result);
  publishState(baseState(), {});
  requestSnapshot();
}

void Client::queueOperationCompletion(quint64 requestId,
                                      const Display::OperationResult &result) {
  m_queuedOperationCompletions.insert(requestId, result);
  // AGENT-CONTRACT: retain final queued results across stop()/start() and
  // always defer them at least one event-loop turn so public methods return
  // the request id before observers can receive its completion.
  QTimer::singleShot(0, this, [this, requestId]() {
    const auto it = m_queuedOperationCompletions.find(requestId);
    if (it == m_queuedOperationCompletions.end()) {
      return;
    }
    const Display::OperationResult queued = it.value();
    m_queuedOperationCompletions.erase(it);
    Q_EMIT operationCompleted(requestId, queued);
  });
}

Display::OperationResult Client::localResult(OperationKind kind,
                                             Display::OperationStatus status,
                                             const QString &reasonCode) const {
  Display::OperationResult result;
  result.kind = publicKind(kind);
  result.status = status;
  result.error = errorCodeForReason(reasonCode);
  result.diagnostic = reasonCode;
  result.initiatingEpoch =
      m_snapshot ? m_snapshot->serviceEpoch : QStringLiteral("client-local");
  result.initiatingRevision = m_snapshot ? m_snapshot->revision : 1;
  result.observedRevision = result.initiatingRevision;
  return result;
}

Display::OperationKind Client::publicKind(OperationKind kind) {
  switch (kind) {
  case OperationKind::Stage:
    return Display::OperationKind::Stage;
  case OperationKind::Preview:
    return Display::OperationKind::Preview;
  case OperationKind::Confirm:
    return Display::OperationKind::Confirm;
  case OperationKind::Cancel:
    return Display::OperationKind::Cancel;
  }
  return Display::OperationKind::Stage;
}

} // namespace QindaQt::DisplayClient
