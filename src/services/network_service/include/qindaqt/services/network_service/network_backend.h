// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QObject>

namespace QindaQt::Network::Service {

struct BackendObservation final {
  Availability availability = Availability::Unavailable;
  Capabilities capabilities;
  ConnectivityKind connectivity = ConnectivityKind::Unknown;
  QString reasonCode;
  QString diagnostic;
  QList<Radio> radios;
  QList<Device> devices;
  QList<AccessPoint> accessPoints;
  QList<KnownNetwork> knownNetworks;
  QList<ActiveConnection> activeConnections;
  ScanPhase scanPhase = ScanPhase::Idle;
  qint64 scanLeaseRemainingMilliseconds = 0;

  friend bool operator==(const BackendObservation &,
                         const BackendObservation &) = default;
};

struct BackendOperationRequest final {
  OperationKind kind = OperationKind::RequestScan;
  qint64 scanDeadlineMilliseconds = 0;
  QString identifier;
  RadioKind radioKind = RadioKind::Wifi;
  bool enable = false;

  friend bool operator==(const BackendOperationRequest &,
                         const BackendOperationRequest &) = default;
};

enum class BackendOperationStatus {
  Succeeded,
  Unsupported,
  Failed,
  Uncertain,
  Busy,
};

struct BackendOperationOutcome final {
  BackendOperationStatus status = BackendOperationStatus::Failed;
  QString reasonCode;
  QString diagnostic;

  friend bool operator==(const BackendOperationOutcome &,
                         const BackendOperationOutcome &) = default;
};

// AGENT-CONTRACT: A backend is owned by one NetworkServiceCoordinator and is
// called only on its constructing Qt thread. start() returns a fresh nonzero
// equality generation before any queued publication. All emitted values are
// immutable secret-free copies; no platform or libnm handle may cross this
// boundary. stop()/cancel() are idempotent. `authorityReplaced` means that a
// previously observed NetworkManager unique owner is gone or different, so
// the resident Network1 process must terminate instead of changing epoch under
// the same public unique owner. See ADR-0052.
class NetworkBackend : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~NetworkBackend() override = default;

  [[nodiscard]] virtual quint64 start() = 0;
  virtual void stop() = 0;
  virtual void submit(quint64 operationId,
                      const BackendOperationRequest &request) = 0;
  virtual void cancel(quint64 operationId) = 0;

Q_SIGNALS:
  void observationReady(
      quint64 generation,
      const QindaQt::Network::Service::BackendObservation &observation);
  void operationFinished(
      quint64 generation, quint64 operationId,
      const QindaQt::Network::Service::BackendOperationOutcome &outcome);
  void authorityReplaced(quint64 generation);
};

} // namespace QindaQt::Network::Service

Q_DECLARE_METATYPE(QindaQt::Network::Service::BackendObservation)
Q_DECLARE_METATYPE(QindaQt::Network::Service::BackendOperationOutcome)
