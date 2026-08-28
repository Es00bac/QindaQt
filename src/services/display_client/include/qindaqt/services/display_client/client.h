// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_client/display_transport.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <optional>

namespace QindaQt::DisplayClient {

enum class ClientState {
  Stopped,
  Starting,
  Ready,
  Unavailable,
  Degraded,
  Busy,
};

// AGENT-CONTRACT: This QObject is owned and used on one Qt thread. It binds to
// an exact D-Bus owner, publishes only validated snapshots, serializes
// mutations, and never replays a timed-out or owner-interrupted mutation. The
// borrowed transport must share this object's thread and outlive it; all
// completion/error reporting is asynchronous through signals. stop() retains
// already-final queued results and schedules one client-stopped uncertain
// result for a mutation still transport-backed; start()/stop() cycles never
// reuse request ids, while destruction safely drops queued delivery.
//
// State compatibility: Starting means owner resolution or the first fetch is
// in flight; Unavailable means no owner or owner-declared unavailability;
// Degraded means an owner exists but the last transport/decode/publication
// attempt failed (a last-known-good snapshot may remain); Busy means exactly
// one mutation is pending; Ready means a validated snapshot is held and no
// mutation is pending. Local preconditions map to InvalidTransition,
// operation-pending maps to TransactionActive, malformed replies to
// MalformedPayload, and uncertain connectivity to CompositorUnavailable.
class Client : public QObject {
  Q_OBJECT

public:
  explicit Client(DisplayTransport *transport, QObject *parent = nullptr);

  void start();
  void stop();
  void refresh();

  [[nodiscard]] ClientState state() const noexcept;
  [[nodiscard]] QString reasonCode() const;
  [[nodiscard]] QString owner() const;
  [[nodiscard]] bool hasSnapshot() const noexcept;
  [[nodiscard]] std::optional<Display::Snapshot> snapshot() const;
  [[nodiscard]] bool operationPending() const noexcept;

  void setRequestTimeout(int milliseconds);

  [[nodiscard]] quint64 stage(const QString &transactionId,
                              const Display::Candidate &candidate);
  [[nodiscard]] quint64 preview(const QString &transactionId);
  [[nodiscard]] quint64 confirm(const QString &transactionId);
  [[nodiscard]] quint64 cancel(const QString &transactionId);

Q_SIGNALS:
  void stateChanged(QindaQt::DisplayClient::ClientState state,
                    const QString &reasonCode);
  void snapshotChanged(const QindaQt::Display::Snapshot &snapshot);
  void operationCompleted(quint64 requestId,
                          const QindaQt::Display::OperationResult &result);

private Q_SLOTS:
  void acceptOwner(const QString &owner);
  void acceptInvalidation(const QString &owner, const QString &epoch,
                          quint64 revision, bool available);
  void acceptSnapshotReply(const QString &owner, quint64 requestId,
                           bool transportSuccess,
                           const Display::Snapshot &snapshot,
                           const QString &reasonCode);
  void acceptOperationReply(const QString &owner, quint64 requestId,
                            bool transportSuccess,
                            const Display::OperationResult &result,
                            const QString &reasonCode);
  void acceptActivationFinished(bool success, const QString &reasonCode);
  void onFetchTimeout();
  void onOperationTimeout();

private:
  enum class OperationKind { Stage, Preview, Confirm, Cancel };
  struct PendingOperation {
    quint64 requestId = 0;
    OperationKind kind = OperationKind::Stage;
    QString transactionId;
    // AGENT-GUARD: retain the lineage the request was submitted against so
    // acceptOperationReply can authenticate a successful result after an
    // owner or snapshot transition. serviceEpoch is a QString everywhere
    // else in this protocol (see Display::Snapshot); storing it numerically
    // would make that cross-boundary check impossible.
    QString epochAtSubmit;
    quint64 revisionAtSubmit = 0;
  };

  void publishState(ClientState state, const QString &reasonCode);
  void publishSnapshotState(const Display::Snapshot &snapshot);
  void requestSnapshot();
  void scheduleRefetch();
  [[nodiscard]] quint64 beginOperation(OperationKind kind,
                                       const QString &transactionId);
  // Allocates a fresh request id and queues an immediate local rejection for
  // it without ever touching the transport or m_operation. Used for every
  // fail-closed precondition a caller can violate before a wire call would
  // even make sense (not started, already busy, no snapshot, stale lineage).
  [[nodiscard]] quint64 rejectLocally(OperationKind kind,
                                      const QString &reasonCode);
  void queueOperationCompletion(quint64 requestId,
                                const Display::OperationResult &result);
  void completeUncertain(const QString &reasonCode);
  [[nodiscard]] Display::OperationResult
  localResult(OperationKind kind, Display::OperationStatus status,
              const QString &reasonCode) const;
  [[nodiscard]] static Display::OperationKind publicKind(OperationKind kind);
  [[nodiscard]] ClientState baseState() const noexcept;

  DisplayTransport *m_transport = nullptr;
  ClientState m_state = ClientState::Stopped;
  QString m_reasonCode;
  QString m_owner;
  std::optional<Display::Snapshot> m_snapshot;
  std::optional<PendingOperation> m_operation;
  QHash<quint64, Display::OperationResult> m_queuedOperationCompletions;
  QTimer m_fetchTimer;
  QTimer m_operationTimer;
  QTimer m_retryTimer;
  quint64 m_nextRequestId = 1;
  quint64 m_fetchRequestId = 0;
  bool m_fetchInFlight = false;
  bool m_refetchNeeded = false;
  bool m_activationInFlight = false;
  QString m_announcedEpoch;
  int m_requestTimeoutMs = 5'000;
};

} // namespace QindaQt::DisplayClient

Q_DECLARE_METATYPE(QindaQt::DisplayClient::ClientState)
