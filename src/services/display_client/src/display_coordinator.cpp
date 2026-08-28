// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_client/display_coordinator.h>

#include <QtCore/QMetaType>

namespace QindaQt::DisplayClient {
namespace {

bool isOkStatus(Display::OperationStatus status) {
  return status == Display::OperationStatus::Accepted ||
         status == Display::OperationStatus::Succeeded;
}

QString reasonCode(Display::TransactionReason reason) {
  switch (reason) {
  case Display::TransactionReason::None:
    return QStringLiteral("transaction-ended");
  case Display::TransactionReason::Cancelled:
    return QStringLiteral("cancelled");
  case Display::TransactionReason::ConfirmationDeadline:
    return QStringLiteral("confirmation-deadline");
  case Display::TransactionReason::Locked:
    return QStringLiteral("locked");
  case Display::TransactionReason::Suspend:
    return QStringLiteral("suspend");
  case Display::TransactionReason::TopologyChanged:
    return QStringLiteral("topology-changed");
  case Display::TransactionReason::ExternalChange:
    return QStringLiteral("external-change");
  case Display::TransactionReason::Recovery:
    return QStringLiteral("recovery");
  case Display::TransactionReason::ApplyRejected:
    return QStringLiteral("apply-rejected");
  case Display::TransactionReason::ApplyTimeout:
    return QStringLiteral("apply-timeout");
  case Display::TransactionReason::ObservationMismatch:
    return QStringLiteral("observation-mismatch");
  case Display::TransactionReason::ObservationTimeout:
    return QStringLiteral("observation-timeout");
  case Display::TransactionReason::RevertFailed:
    return QStringLiteral("revert-failed");
  case Display::TransactionReason::JournalFailure:
    return QStringLiteral("journal-failure");
  case Display::TransactionReason::TransportUncertain:
    return QStringLiteral("transport-uncertain");
  }
  return QStringLiteral("transaction-ended");
}

} // namespace

Coordinator::Coordinator(Client *client, QObject *parent)
    : QObject(parent), m_client(client) {
  Q_ASSERT(m_client != nullptr);
  qRegisterMetaType<CoordinatorState>();
  qRegisterMetaType<CoordinatorOutcome>();

  m_confirmationDeadlineTimer.setSingleShot(true);
  connect(&m_confirmationDeadlineTimer, &QTimer::timeout, this,
          &Coordinator::onConfirmationDeadlineExpired);
  connect(m_client, &Client::stateChanged, this,
          &Coordinator::onClientStateChanged);
  connect(m_client, &Client::operationCompleted, this,
          &Coordinator::onOperationCompleted);
  connect(m_client, &Client::snapshotChanged, this,
          &Coordinator::onSnapshotChanged);

  m_state = m_client->state() == ClientState::Ready
                ? CoordinatorState::Idle
                : CoordinatorState::Unavailable;
}

void Coordinator::setConfirmationDeadline(int milliseconds) {
  m_confirmationDeadlineMs = qMax(25'000, milliseconds);
}

CoordinatorState Coordinator::state() const noexcept { return m_state; }

QString Coordinator::transactionId() const { return m_transactionId; }

bool Coordinator::begin(const QString &transactionId,
                        const Display::Candidate &candidate) {
  const auto snapshot = m_client->snapshot();
  if (isActive() || transactionId.isEmpty() ||
      m_client->state() != ClientState::Ready || !snapshot.has_value()) {
    return false;
  }

  m_transactionId = transactionId;
  m_ownerAtBegin = m_client->owner();
  m_epochAtBegin = snapshot->serviceEpoch;
  m_pendingReasonCode.clear();
  m_previewAccepted = false;
  m_summaryObserved = false;
  m_cancelFollowsUncertain = false;
  m_pendingRequestId = m_client->stage(transactionId, candidate);
  publishState(CoordinatorState::Staging, {});
  return m_pendingRequestId != 0;
}

bool Coordinator::confirm() {
  if (m_state != CoordinatorState::AwaitingConfirmation) {
    return false;
  }
  m_confirmationDeadlineTimer.stop();
  m_pendingRequestId = m_client->confirm(m_transactionId);
  publishState(CoordinatorState::Confirming, {});
  return m_pendingRequestId != 0;
}

bool Coordinator::cancel() {
  if (m_state != CoordinatorState::Staging &&
      m_state != CoordinatorState::Previewing &&
      m_state != CoordinatorState::AwaitingConfirmation) {
    return false;
  }
  m_cancelFollowsUncertain = m_client->operationPending();
  beginCancel(QStringLiteral("cancelled"));
  return true;
}

void Coordinator::onClientStateChanged(ClientState state,
                                       const QString &reasonCodeValue) {
  if (isActive()) {
    if (state == ClientState::Unavailable || state == ClientState::Stopped ||
        m_client->owner() != m_ownerAtBegin) {
      finish(CoordinatorOutcome::Uncertain, QStringLiteral("lineage-lost"));
    }
    return;
  }

  if (state == ClientState::Unavailable || state == ClientState::Stopped) {
    publishState(CoordinatorState::Unavailable, reasonCodeValue);
  } else if (m_state == CoordinatorState::Unavailable) {
    publishState(CoordinatorState::Idle, {});
  }
}

void Coordinator::onSnapshotChanged(const Display::Snapshot &snapshot) {
  if (!isActive()) {
    return;
  }
  if (m_client->owner() != m_ownerAtBegin ||
      snapshot.serviceEpoch != m_epochAtBegin) {
    finish(CoordinatorOutcome::Uncertain, QStringLiteral("lineage-lost"));
    return;
  }

  const Display::TransactionSummary *summary = nullptr;
  for (const auto &candidate : snapshot.transactions) {
    if (candidate.transactionId == m_transactionId) {
      summary = &candidate;
      break;
    }
  }

  if (summary == nullptr) {
    if (m_summaryObserved && m_state != CoordinatorState::Confirming) {
      finish(m_cancelFollowsUncertain ? CoordinatorOutcome::Uncertain
                                      : CoordinatorOutcome::Reverted,
             m_pendingReasonCode.isEmpty() ? QStringLiteral("transaction-ended")
                                           : m_pendingReasonCode);
    }
    return;
  }

  m_summaryObserved = true;
  if (m_state == CoordinatorState::Previewing && m_previewAccepted &&
      summary->state == Display::TransactionState::AwaitingConfirmation) {
    m_confirmationDeadlineTimer.start(m_confirmationDeadlineMs);
    publishState(CoordinatorState::AwaitingConfirmation, {});
  } else if (summary->state == Display::TransactionState::Stuck) {
    finish(CoordinatorOutcome::Uncertain, reasonCode(summary->reason));
  }
}

void Coordinator::onOperationCompleted(quint64 requestId,
                                       const Display::OperationResult &result) {
  if (requestId != m_pendingRequestId) {
    return;
  }
  const bool ok = isOkStatus(result.status);
  const QString reason = result.diagnostic.isEmpty()
                             ? QStringLiteral("operation-rejected")
                             : result.diagnostic;

  switch (m_state) {
  case CoordinatorState::Staging:
    if (result.status == Display::OperationStatus::Succeeded &&
        result.diagnostic == QStringLiteral("no-op")) {
      finish(CoordinatorOutcome::NoOp, QStringLiteral("no-op"));
    } else if (ok) {
      m_pendingRequestId = m_client->preview(m_transactionId);
      publishState(CoordinatorState::Previewing, {});
    } else {
      finish(result.status == Display::OperationStatus::Uncertain
                 ? CoordinatorOutcome::Uncertain
                 : CoordinatorOutcome::Reverted,
             reason);
    }
    break;
  case CoordinatorState::Previewing:
    if (ok) {
      m_previewAccepted = true;
      if (const auto snapshot = m_client->snapshot(); snapshot.has_value()) {
        onSnapshotChanged(*snapshot);
      }
    } else {
      m_cancelFollowsUncertain =
          result.status == Display::OperationStatus::Uncertain;
      beginCancel(m_cancelFollowsUncertain ? QStringLiteral("preview-uncertain")
                                           : reason);
    }
    break;
  case CoordinatorState::Confirming:
    finish(ok ? CoordinatorOutcome::Confirmed : CoordinatorOutcome::Uncertain,
           ok ? QString{} : reason);
    break;
  case CoordinatorState::Cancelling:
    finish(ok || !m_cancelFollowsUncertain ? CoordinatorOutcome::Reverted
                                           : CoordinatorOutcome::Uncertain,
           m_pendingReasonCode);
    break;
  case CoordinatorState::Idle:
  case CoordinatorState::Unavailable:
  case CoordinatorState::AwaitingConfirmation:
  case CoordinatorState::Confirmed:
  case CoordinatorState::Reverted:
  case CoordinatorState::Uncertain:
  case CoordinatorState::NoOp:
    break;
  }
}

void Coordinator::onConfirmationDeadlineExpired() {
  if (m_state == CoordinatorState::AwaitingConfirmation) {
    m_cancelFollowsUncertain = false;
    beginCancel(QStringLiteral("rescue-deadline"));
  }
}

void Coordinator::beginCancel(const QString &reasonCodeValue) {
  m_confirmationDeadlineTimer.stop();
  m_pendingReasonCode = reasonCodeValue;
  m_pendingRequestId = m_client->cancel(m_transactionId);
  publishState(CoordinatorState::Cancelling, reasonCodeValue);
}

void Coordinator::finish(CoordinatorOutcome outcome,
                         const QString &reasonCodeValue) {
  m_confirmationDeadlineTimer.stop();
  m_pendingRequestId = 0;
  CoordinatorState terminal = CoordinatorState::Uncertain;
  switch (outcome) {
  case CoordinatorOutcome::Confirmed:
    terminal = CoordinatorState::Confirmed;
    break;
  case CoordinatorOutcome::Reverted:
    terminal = CoordinatorState::Reverted;
    break;
  case CoordinatorOutcome::Uncertain:
    terminal = CoordinatorState::Uncertain;
    break;
  case CoordinatorOutcome::NoOp:
    terminal = CoordinatorState::NoOp;
    break;
  }
  publishState(terminal, reasonCodeValue);
  Q_EMIT transactionFinished(m_transactionId, outcome, reasonCodeValue);
}

bool Coordinator::isActive() const noexcept {
  return m_state == CoordinatorState::Staging ||
         m_state == CoordinatorState::Previewing ||
         m_state == CoordinatorState::AwaitingConfirmation ||
         m_state == CoordinatorState::Confirming ||
         m_state == CoordinatorState::Cancelling;
}

void Coordinator::publishState(CoordinatorState state,
                               const QString &reasonCodeValue) {
  if (m_state == state) {
    return;
  }
  m_state = state;
  Q_EMIT stateChanged(state, reasonCodeValue);
}

} // namespace QindaQt::DisplayClient
