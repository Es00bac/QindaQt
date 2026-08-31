// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_service/network_service_coordinator.h>

#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_protocol/network_redaction.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include <QtCore/QRegularExpression>

#include <algorithm>
#include <chrono>
#include <limits>

namespace QindaQt::Network::Service {
namespace {

bool validReasonCode(const QString &reason) {
  static const QRegularExpression pattern(
      QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$"));
  return reason.isEmpty() || pattern.match(reason).hasMatch();
}

OperationStatus mapStatus(const BackendOperationStatus status) {
  switch (status) {
  case BackendOperationStatus::Succeeded:
    return OperationStatus::Succeeded;
  case BackendOperationStatus::Unsupported:
    return OperationStatus::Unsupported;
  case BackendOperationStatus::Failed:
    return OperationStatus::Failed;
  case BackendOperationStatus::Uncertain:
    return OperationStatus::Uncertain;
  case BackendOperationStatus::Busy:
    return OperationStatus::Busy;
  }
  return OperationStatus::Failed;
}

OperationStatus refusalStatus(const QString &reason) {
  return reason == QStringLiteral("scan-busy") ||
                 reason == QStringLiteral("scan-lease-held") ||
                 reason == QStringLiteral("operation-in-flight")
             ? OperationStatus::Busy
             : (reason.endsWith(QStringLiteral("unsupported"))
                    ? OperationStatus::Unsupported
                    : OperationStatus::Rejected);
}

quint64 bootMonotonicNanoseconds() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  const auto count =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
  return count <= 0 ? 1 : static_cast<quint64>(count);
}

} // namespace

NetworkServiceCoordinator::NetworkServiceCoordinator(
    NetworkBackend *backend, const int operationTimeoutMilliseconds,
    QObject *parent)
    : QObject(parent), m_backend(backend),
      m_operationTimeoutMilliseconds(operationTimeoutMilliseconds) {
  Q_ASSERT(m_backend != nullptr);
  m_operationTimer.setSingleShot(true);
  connect(&m_operationTimer, &QTimer::timeout, this,
          &NetworkServiceCoordinator::handleOperationTimeout);
  connect(m_backend, &NetworkBackend::observationReady, this,
          &NetworkServiceCoordinator::handleObservation);
  connect(m_backend, &NetworkBackend::operationFinished, this,
          &NetworkServiceCoordinator::handleOperation);
  connect(m_backend, &NetworkBackend::authorityReplaced, this,
          &NetworkServiceCoordinator::handleAuthorityReplacement);
}

NetworkServiceCoordinator::~NetworkServiceCoordinator() { stop(); }

bool NetworkServiceCoordinator::start(const QString &uniqueOwner) {
  if (m_running) {
    return true;
  }
  if (!isValidUniqueOwner(uniqueOwner) ||
      m_operationTimeoutMilliseconds < kMinimumRequestTimeoutMilliseconds ||
      m_operationTimeoutMilliseconds > kMaximumRequestTimeoutMilliseconds) {
    return false;
  }
  m_running = true;
  m_owner = uniqueOwner;
  m_model = Model::NetworkModel();
  m_epochHighWater = nextEpoch();
  m_snapshot = {};
  m_snapshot.protocolVersion = kProtocolVersion;
  m_snapshot.owner = m_owner;
  m_snapshot.epoch = m_epochHighWater;
  m_snapshot.revision = 1;
  m_snapshot.availability = Availability::Starting;
  const auto applied = m_model.applySnapshot(m_snapshot);
  Q_ASSERT(applied.accepted);
  Q_EMIT snapshotChanged();
  Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);

  m_generation = m_backend->start();
  if (m_generation == 0) {
    BackendObservation unavailable;
    unavailable.availability = Availability::Unavailable;
    unavailable.reasonCode = QStringLiteral("backend-start-failed");
    publishObservation(unavailable);
  }
  return true;
}

void NetworkServiceCoordinator::stop() {
  if (!m_running) {
    return;
  }
  m_operationTimer.stop();
  if (m_pending.has_value()) {
    m_backend->cancel(m_pending->id);
    finishPending(OperationStatus::Uncertain,
                  QStringLiteral("service-stopped"));
  }
  m_running = false;
  m_generation = 0;
  m_backend->stop();
}

QByteArray NetworkServiceCoordinator::snapshotPayload() const {
  const EncodeResult encoded = encodeSnapshot(m_snapshot);
  return encoded.succeeded() ? encoded.payload : QByteArray{};
}

OperationSubmission
NetworkServiceCoordinator::submit(const NetworkServiceRequest &request) {
  if (!m_running || !m_model.snapshot().has_value()) {
    return {false, 0,
            immediate(request, OperationStatus::Rejected,
                      QStringLiteral("service-not-ready"))};
  }
  if (request.initiatingEpoch != m_snapshot.epoch ||
      request.initiatingRevision != m_snapshot.revision) {
    return {false, 0,
            immediate(request, OperationStatus::Rejected,
                      QStringLiteral("stale-lineage"))};
  }
  if (m_pending.has_value()) {
    return {false, 0,
            immediate(request, OperationStatus::Busy,
                      QStringLiteral("operation-in-flight"))};
  }

  Model::IntentVerdict verdict;
  BackendOperationRequest backendRequest;
  backendRequest.kind = request.kind;
  switch (request.kind) {
  case OperationKind::RequestScan:
    verdict = m_model.requestScan(
        RequestScanIntent{request.scanDeadlineMilliseconds});
    backendRequest.scanDeadlineMilliseconds = request.scanDeadlineMilliseconds;
    break;
  case OperationKind::ConnectKnownNetwork:
    verdict = m_model.connectKnown(ConnectIntent{request.identifier});
    backendRequest.identifier = request.identifier;
    break;
  case OperationKind::DisconnectActive:
    verdict = m_model.disconnectDevice(DisconnectIntent{request.identifier});
    backendRequest.identifier = request.identifier;
    break;
  case OperationKind::SetRadio:
    verdict =
        m_model.setRadio(SetRadioIntent{request.radioKind, request.enable});
    backendRequest.radioKind = request.radioKind;
    backendRequest.enable = request.enable;
    break;
  }
  if (!verdict.allowed) {
    return {false, 0,
            immediate(request, refusalStatus(verdict.reasonCode),
                      verdict.reasonCode)};
  }

  const quint64 operationId = nextOperationId();
  if (operationId == 0) {
    return {false, 0,
            immediate(request, OperationStatus::Failed,
                      QStringLiteral("operation-id-exhausted"))};
  }
  m_pending = PendingOperation{operationId, request};
  m_operationTimer.start(m_operationTimeoutMilliseconds);
  m_backend->submit(operationId, backendRequest);
  return {true, operationId, {}};
}

void NetworkServiceCoordinator::handleObservation(
    const quint64 generation, const BackendObservation &observation) {
  if (!m_running || generation == 0 || generation != m_generation) {
    return;
  }
  publishObservation(observation);
}

void NetworkServiceCoordinator::publishObservation(
    const BackendObservation &observation) {
  Snapshot candidate;
  candidate.protocolVersion = kProtocolVersion;
  candidate.owner = m_owner;
  candidate.epoch = m_snapshot.epoch;
  candidate.revision = m_snapshot.revision + 1;
  candidate.availability = observation.availability;
  candidate.capabilities = observation.capabilities;
  candidate.connectivity = observation.connectivity;
  candidate.reasonCode = redactDiagnostic(observation.reasonCode);
  candidate.diagnostic = redactDiagnostic(observation.diagnostic);
  candidate.radios = observation.radios;
  candidate.devices = observation.devices;
  candidate.accessPoints = observation.accessPoints;
  candidate.knownNetworks = observation.knownNetworks;
  candidate.activeConnections = observation.activeConnections;
  candidate.scanPhase = observation.scanPhase;
  if (candidate.scanPhase != ScanPhase::Idle) {
    candidate.scanLease = {QStringLiteral("scan-%1-%2")
                               .arg(candidate.epoch)
                               .arg(candidate.revision),
                           candidate.epoch, candidate.revision,
                           observation.scanLeaseRemainingMilliseconds};
  }
  if (candidate.revision == 0 || !validateSnapshot(candidate).accepted ||
      !m_model.applySnapshot(candidate).accepted) {
    publishMalformedFallback();
    return;
  }
  m_snapshot = std::move(candidate);
  Q_EMIT snapshotChanged();
  Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void NetworkServiceCoordinator::publishMalformedFallback() {
  Snapshot fallback;
  fallback.protocolVersion = kProtocolVersion;
  fallback.owner = m_owner;
  fallback.epoch = m_snapshot.epoch;
  fallback.revision = m_snapshot.revision + 1;
  fallback.availability = Availability::Degraded;
  fallback.reasonCode = QStringLiteral("backend-malformed");
  fallback.diagnostic = QStringLiteral("Network backend data was rejected");
  if (fallback.revision == 0 || !m_model.applySnapshot(fallback).accepted) {
    return;
  }
  m_snapshot = std::move(fallback);
  Q_EMIT snapshotChanged();
  Q_EMIT invalidated(m_snapshot.epoch, m_snapshot.revision);
}

void NetworkServiceCoordinator::handleOperation(
    const quint64 generation, const quint64 operationId,
    const BackendOperationOutcome &outcome) {
  if (!m_running || generation != m_generation || !m_pending.has_value() ||
      m_pending->id != operationId) {
    return;
  }
  const auto rawStatus = static_cast<unsigned int>(outcome.status);
  if (rawStatus > static_cast<unsigned int>(BackendOperationStatus::Busy) ||
      !validReasonCode(outcome.reasonCode)) {
    finishPending(OperationStatus::Failed, QStringLiteral("backend-malformed"));
    return;
  }
  const OperationStatus status = mapStatus(outcome.status);
  const QString reason =
      status == OperationStatus::Succeeded
          ? QString{}
          : (outcome.reasonCode.isEmpty() ? QStringLiteral("backend-malformed")
                                          : outcome.reasonCode);
  finishPending(status, reason, redactDiagnostic(outcome.diagnostic));
}

void NetworkServiceCoordinator::handleAuthorityReplacement(
    const quint64 generation) {
  if (!m_running || generation != m_generation) {
    return;
  }
  if (m_pending.has_value()) {
    m_backend->cancel(m_pending->id);
    finishPending(OperationStatus::Uncertain,
                  QStringLiteral("authority-replaced"));
  }
  m_generation = 0;
  m_backend->stop();
  BackendObservation unavailable;
  unavailable.availability = Availability::Unavailable;
  unavailable.reasonCode = QStringLiteral("networkmanager-replaced");
  publishObservation(unavailable);
  Q_EMIT restartRequired();
}

void NetworkServiceCoordinator::handleOperationTimeout() {
  if (!m_pending.has_value()) {
    return;
  }
  m_backend->cancel(m_pending->id);
  finishPending(OperationStatus::Uncertain, QStringLiteral("backend-timeout"));
}

OperationResult
NetworkServiceCoordinator::immediate(const NetworkServiceRequest &request,
                                     const OperationStatus status,
                                     const QString &reason) const {
  return {.kind = request.kind,
          .status = status,
          .initiatingEpoch = request.initiatingEpoch == 0
                                 ? m_snapshot.epoch
                                 : request.initiatingEpoch,
          .initiatingRevision = request.initiatingRevision == 0
                                    ? m_snapshot.revision
                                    : request.initiatingRevision,
          .reasonCode = reason,
          .diagnostic = {},
          .wireValid = true};
}

void NetworkServiceCoordinator::finishPending(const OperationStatus status,
                                              const QString &reason,
                                              const QString &diagnostic) {
  if (!m_pending.has_value()) {
    return;
  }
  m_operationTimer.stop();
  const PendingOperation pending = *m_pending;
  m_pending.reset();
  OperationResult result = immediate(pending.request, status, reason);
  result.diagnostic = redactDiagnostic(diagnostic);
  if (!validateOperationResult(result).accepted) {
    result = immediate(pending.request, OperationStatus::Failed,
                       QStringLiteral("backend-malformed"));
  }
  Q_EMIT operationCompleted(pending.id, result);
}

quint64 NetworkServiceCoordinator::nextEpoch() {
  const quint64 clock = bootMonotonicNanoseconds();
  const quint64 minimum =
      m_epochHighWater == std::numeric_limits<quint64>::max()
          ? 0
          : m_epochHighWater + 1;
  return std::max(clock, minimum);
}

quint64 NetworkServiceCoordinator::nextOperationId() {
  if (m_nextOperation == 0) {
    return 0;
  }
  const quint64 value = m_nextOperation++;
  if (m_nextOperation == 0) {
    m_nextOperation = 0;
  }
  return value;
}

} // namespace QindaQt::Network::Service
