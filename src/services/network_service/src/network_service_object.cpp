// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_service_object_p.h"

#include <qindaqt/services/network_protocol/network_codec.h>

namespace QindaQt::Network::Service {

NetworkServiceObject::NetworkServiceObject(
    NetworkServiceCoordinator *coordinator, const QDBusConnection &connection,
    QObject *parent)
    : QObject(parent), m_coordinator(coordinator), m_connection(connection) {
  Q_ASSERT(m_coordinator != nullptr);
  connect(m_coordinator, &NetworkServiceCoordinator::invalidated, this,
          &NetworkServiceObject::Changed);
  connect(m_coordinator, &NetworkServiceCoordinator::operationCompleted, this,
          &NetworkServiceObject::finishOperation);
}

QByteArray NetworkServiceObject::GetSnapshot() const {
  return m_coordinator->snapshotPayload();
}

void NetworkServiceObject::RequestScan(const quint64 epoch,
                                       const quint64 revision,
                                       const qint64 deadlineMs) {
  NetworkServiceRequest request;
  request.kind = OperationKind::RequestScan;
  request.initiatingEpoch = epoch;
  request.initiatingRevision = revision;
  request.scanDeadlineMilliseconds = deadlineMs;
  beginOperation(request);
}

void NetworkServiceObject::ConnectKnownNetwork(const quint64 epoch,
                                               const quint64 revision,
                                               const QString &knownNetworkId) {
  beginOperation({.kind = OperationKind::ConnectKnownNetwork,
                  .initiatingEpoch = epoch,
                  .initiatingRevision = revision,
                  .identifier = knownNetworkId});
}

void NetworkServiceObject::DisconnectActive(const quint64 epoch,
                                            const quint64 revision,
                                            const QString &deviceInterface) {
  beginOperation({.kind = OperationKind::DisconnectActive,
                  .initiatingEpoch = epoch,
                  .initiatingRevision = revision,
                  .identifier = deviceInterface});
}

void NetworkServiceObject::SetRadio(const quint64 epoch, const quint64 revision,
                                    const quint32 radioKind,
                                    const bool enable) {
  NetworkServiceRequest request;
  request.kind = OperationKind::SetRadio;
  request.initiatingEpoch = epoch;
  request.initiatingRevision = revision;
  request.radioKind = static_cast<RadioKind>(radioKind);
  request.enable = enable;
  beginOperation(request);
}

void NetworkServiceObject::beginOperation(
    const NetworkServiceRequest &request) {
  if (!calledFromDBus()) {
    return;
  }
  const QDBusMessage call = message();
  setDelayedReply(true);
  const OperationSubmission submission = m_coordinator->submit(request);
  if (!submission.pending) {
    const EncodeResult encoded =
        encodeOperationResult(submission.immediateResult);
    m_connection.send(call.createReply(encoded.payload));
    return;
  }
  // AGENT-GUARD: One original call is retained per coordinator-owned
  // operation. Completion, timeout, stop, and authority loss all remove it
  // before sending exactly one reply; no path can replay the mutation.
  m_pendingReplies.insert(submission.operationId, call);
}

void NetworkServiceObject::finishOperation(const quint64 operationId,
                                           const OperationResult &result) {
  const auto it = m_pendingReplies.find(operationId);
  if (it == m_pendingReplies.end()) {
    return;
  }
  const QDBusMessage call = it.value();
  m_pendingReplies.erase(it);
  const EncodeResult encoded = encodeOperationResult(result);
  if (!encoded.succeeded()) {
    return;
  }
  m_connection.send(call.createReply(encoded.payload));
}

} // namespace QindaQt::Network::Service
