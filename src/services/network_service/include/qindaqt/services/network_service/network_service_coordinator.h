// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_model/network_model.h>
#include <qindaqt/services/network_service/network_backend.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <optional>

namespace QindaQt::Network::Service {

struct NetworkServiceRequest final {
  OperationKind kind = OperationKind::RequestScan;
  quint64 initiatingEpoch = 0;
  quint64 initiatingRevision = 0;
  qint64 scanDeadlineMilliseconds = 0;
  QString identifier;
  RadioKind radioKind = RadioKind::Wifi;
  bool enable = false;
};

struct OperationSubmission final {
  bool pending = false;
  quint64 operationId = 0;
  OperationResult immediateResult;
};

// Owns one public Network1 owner/epoch, validated snapshot publication, and a
// single serialized mutation. It owns no D-Bus or libnm object and borrows the
// backend for its lifetime. All methods are Qt-thread-confined. Backend values
// are hostile input: malformed observations degrade atomically and malformed
// outcomes become a protocol-valid failure. Timeout or authority loss resolves
// a dispatched operation exactly once as Uncertain and never replays it.
// Accepted backend dispatch is queued onto this object's Qt thread so a D-Bus
// owner can retain its delayed reply before synchronous backend completion.
class NetworkServiceCoordinator final : public QObject {
  Q_OBJECT

public:
  explicit NetworkServiceCoordinator(NetworkBackend *backend,
                                     int operationTimeoutMilliseconds = 5'000,
                                     QObject *parent = nullptr);
  ~NetworkServiceCoordinator() override;

  [[nodiscard]] bool start(const QString &uniqueOwner);
  void stop();
  [[nodiscard]] bool isRunning() const noexcept { return m_running; }
  [[nodiscard]] const Snapshot &snapshot() const noexcept { return m_snapshot; }
  [[nodiscard]] QByteArray snapshotPayload() const;
  [[nodiscard]] OperationSubmission
  submit(const NetworkServiceRequest &request);

Q_SIGNALS:
  void snapshotChanged();
  void invalidated(quint64 epoch, quint64 revision);
  void operationCompleted(quint64 operationId,
                          const QindaQt::Network::OperationResult &result);
  void restartRequired();

private:
  struct PendingOperation final {
    quint64 id = 0;
    NetworkServiceRequest request;
  };

  void handleObservation(quint64 generation,
                         const BackendObservation &observation);
  void handleOperation(quint64 generation, quint64 operationId,
                       const BackendOperationOutcome &outcome);
  void handleAuthorityReplacement(quint64 generation);
  void handleOperationTimeout();
  void publishObservation(const BackendObservation &observation);
  void publishMalformedFallback();
  [[nodiscard]] OperationResult immediate(const NetworkServiceRequest &request,
                                          OperationStatus status,
                                          const QString &reason) const;
  void finishPending(OperationStatus status, const QString &reason,
                     const QString &diagnostic = {});
  [[nodiscard]] quint64 nextEpoch();
  [[nodiscard]] quint64 nextOperationId();

  NetworkBackend *m_backend = nullptr;
  Model::NetworkModel m_model;
  QTimer m_operationTimer;
  Snapshot m_snapshot;
  std::optional<PendingOperation> m_pending;
  QString m_owner;
  quint64 m_generation = 0;
  quint64 m_epochHighWater = 0;
  quint64 m_nextOperation = 1;
  int m_operationTimeoutMilliseconds = 5'000;
  bool m_running = false;
};

} // namespace QindaQt::Network::Service
