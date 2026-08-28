// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_client/client.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>

namespace QindaQt::DisplayClient {

enum class CoordinatorState {
  Idle,
  Unavailable,
  Staging,
  Previewing,
  AwaitingConfirmation,
  Confirming,
  Cancelling,
  Confirmed,
  Reverted,
  Uncertain,
  NoOp,
};

enum class CoordinatorOutcome {
  Confirmed,
  Reverted,
  Uncertain,
  NoOp,
};

// AGENT-CONTRACT: Coordinator is a thin caller-facing policy layer above
// Client; it owns exactly one reversible transaction at a time end to end
// (stage -> preview -> confirm|cancel) and never lets a second one start
// while one is in flight. It does not duplicate Client's protocol plumbing:
// preview/confirm/revert authority stays server-side (see ADR-0016 and
// docs/wiki/architecture/display-service.md#transaction-model); this class
// adds only single-transaction sequencing, fail-closed gating on Client's
// reported state/lineage, and a rescue deadline that never preempts the
// server's apply/observe/confirmation window. AwaitingConfirmation is a
// projection of a validated server snapshot, never inferred from Preview's
// acknowledgement. The borrowed Client must share this object's thread and
// outlive it.
class Coordinator : public QObject {
  Q_OBJECT

public:
  explicit Coordinator(Client *client, QObject *parent = nullptr);

  // Rescue time after the server snapshot reports AwaitingConfirmation.
  // Values are clamped to at least 25 seconds (server 15 s window plus
  // apply/observation/grace); this is not transaction timer authority.
  void setConfirmationDeadline(int milliseconds);

  [[nodiscard]] CoordinatorState state() const noexcept;
  [[nodiscard]] QString transactionId() const;

  // Starts a brand-new reversible transaction: stage() then preview() the
  // candidate. Fails closed (returns false, no state change, nothing sent)
  // unless idle/terminal and the underlying client currently reports Ready.
  [[nodiscard]] bool begin(const QString &transactionId,
                           const Display::Candidate &candidate);
  // Valid only while AwaitingConfirmation; otherwise fails closed.
  [[nodiscard]] bool confirm();
  // Accepted while staging, previewing, or awaiting confirmation. Confirming
  // is the point of no return and cannot be superseded safely.
  [[nodiscard]] bool cancel();

Q_SIGNALS:
  void stateChanged(QindaQt::DisplayClient::CoordinatorState state,
                    const QString &reasonCode);
  void transactionFinished(const QString &transactionId,
                           QindaQt::DisplayClient::CoordinatorOutcome outcome,
                           const QString &reasonCode);

private Q_SLOTS:
  void onClientStateChanged(QindaQt::DisplayClient::ClientState state,
                            const QString &reasonCode);
  void onOperationCompleted(quint64 requestId,
                            const QindaQt::Display::OperationResult &result);
  void onSnapshotChanged(const QindaQt::Display::Snapshot &snapshot);
  void onConfirmationDeadlineExpired();

private:
  void publishState(CoordinatorState state, const QString &reasonCode);
  void beginCancel(const QString &reasonCode);
  void finish(CoordinatorOutcome outcome, const QString &reasonCode);
  [[nodiscard]] bool isActive() const noexcept;

  Client *m_client = nullptr;
  CoordinatorState m_state = CoordinatorState::Idle;
  QString m_transactionId;
  QString m_pendingReasonCode;
  QString m_ownerAtBegin;
  QString m_epochAtBegin;
  quint64 m_pendingRequestId = 0;
  bool m_previewAccepted = false;
  bool m_summaryObserved = false;
  bool m_cancelFollowsUncertain = false;
  QTimer m_confirmationDeadlineTimer;
  int m_confirmationDeadlineMs = 25'000;
};

} // namespace QindaQt::DisplayClient

Q_DECLARE_METATYPE(QindaQt::DisplayClient::CoordinatorState)
Q_DECLARE_METATYPE(QindaQt::DisplayClient::CoordinatorOutcome)
