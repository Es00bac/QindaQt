// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_client/network_transport.h>
#include <qindaqt/services/network_protocol/network_codec.h>

#include <QtCore/QList>
#include <QtCore/QPair>

#include <optional>

namespace QindaQt::Apps::SettingsNetwork::TestSupport {

using namespace QindaQt::Network;

inline QString networkId(const QChar fill) { return QString(64, fill); }

inline Snapshot readySnapshot(const QString &owner = QStringLiteral(":1.20"),
                              const quint64 epoch = 20,
                              const quint64 revision = 1) {
  const QString homeId = networkId(u'a');
  const QString cafeId = networkId(u'b');
  Snapshot snapshot;
  snapshot.owner = owner;
  snapshot.epoch = epoch;
  snapshot.revision = revision;
  snapshot.availability = Availability::Ready;
  snapshot.capabilities = Capability::Connectivity | Capability::Scan |
                          Capability::KnownNetworkControl |
                          Capability::ActiveConnectionControl;
  snapshot.connectivity = ConnectivityKind::Full;
  snapshot.radios = {
      {RadioKind::Wifi, true, true, true},
      {RadioKind::Wwan, false, true, false},
  };
  snapshot.devices = {
      {QStringLiteral("wlan0"), DeviceKind::Wifi, DeviceState::Connected},
      {QStringLiteral("enp3s0"), DeviceKind::Ethernet,
       DeviceState::Disconnected},
  };
  snapshot.accessPoints = {
      {QStringLiteral("wlan0"), QStringLiteral("Home"), false,
       QStringLiteral("00:11:22:33:44:55"), SecuritySuite::Wpa3Personal,
       5'180, 88},
      {QStringLiteral("wlan0"), QStringLiteral("Cafe"), false,
       QStringLiteral("66:77:88:99:aa:bb"), SecuritySuite::Open, 2'412, 52},
  };
  snapshot.knownNetworks = {
      {homeId, QStringLiteral("Home"), false, SecuritySuite::Wpa3Personal,
       true},
      {cafeId, QStringLiteral("Cafe"), false, SecuritySuite::Open, false},
  };
  snapshot.activeConnections = {{QStringLiteral("wlan0"), homeId}};
  snapshot.scanPhase = ScanPhase::Idle;
  return snapshot;
}

inline OperationResult operationResult(const OperationKind kind,
                                       const OperationStatus status,
                                       const quint64 epoch = 20,
                                       const quint64 revision = 1,
                                       QString reason = {}) {
  return OperationResult{kind, status, epoch, revision, std::move(reason), {},
                         true};
}

class FakeNetworkTransport final
    : public QindaQt::Network::Client::NetworkTransport {
  Q_OBJECT

public:
  struct OperationCall final {
    quint64 token = 0;
    QString owner;
    quint64 epoch = 0;
    quint64 revision = 0;
    OperationKind kind = OperationKind::RequestScan;
    QVariantMap parameters;
  };

  bool start(QString *error = nullptr) override {
    ++startCalls;
    if (startFails) {
      if (error != nullptr) {
        *error = startError;
      }
      return false;
    }
    running = true;
    return true;
  }

  void stop() override {
    ++stopCalls;
    running = false;
  }

  void requestSnapshot(const quint64 token, const QString &owner) override {
    snapshotRequests.append(qMakePair(token, owner));
    if (running && autoSnapshotPayload.has_value()) {
      Q_EMIT snapshotReceived(token, owner, *autoSnapshotPayload);
    }
  }

  void requestOperation(const quint64 token, const QString &owner,
                        const quint64 epoch, const quint64 revision,
                        const OperationKind kind,
                        const QVariantMap &parameters) override {
    operations.append({token, owner, epoch, revision, kind, parameters});
  }

  void setSnapshot(const Snapshot &snapshot) {
    const EncodeResult encoded = encodeSnapshot(snapshot);
    Q_ASSERT(encoded.succeeded());
    autoSnapshotPayload = encoded.payload;
  }

  void setPayload(QByteArray payload) {
    autoSnapshotPayload = std::move(payload);
  }

  void clearSnapshot() { autoSnapshotPayload.reset(); }

  void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }

  void invalidate(const QString &owner = QStringLiteral(":1.20")) {
    Q_EMIT snapshotInvalidated(owner);
  }

  void finishLatestSnapshot(const Snapshot &snapshot) {
    Q_ASSERT(!snapshotRequests.isEmpty());
    const EncodeResult encoded = encodeSnapshot(snapshot);
    Q_ASSERT(encoded.succeeded());
    const auto &request = snapshotRequests.constLast();
    Q_EMIT snapshotReceived(request.first, request.second, encoded.payload);
  }

  void finishLast(const OperationResult &result,
                  const QString &replyOwner = {}) {
    Q_ASSERT(!operations.isEmpty());
    const EncodeResult encoded = encodeOperationResult(result);
    Q_ASSERT(encoded.succeeded());
    const OperationCall &call = operations.constLast();
    Q_EMIT operationReceived(
        call.token, replyOwner.isEmpty() ? call.owner : replyOwner,
        encoded.payload);
  }

  void failLast(const QString &message = QStringLiteral("transport-failed")) {
    Q_ASSERT(!operations.isEmpty());
    const OperationCall &call = operations.constLast();
    Q_EMIT requestFailed(call.token, call.owner,
                         QStringLiteral("org.qindaqt.Network1.Failed"),
                         message);
  }

  QList<QPair<quint64, QString>> snapshotRequests;
  QList<OperationCall> operations;
  std::optional<QByteArray> autoSnapshotPayload;
  QString startError = QStringLiteral("transport unavailable");
  int startCalls = 0;
  int stopCalls = 0;
  bool startFails = false;
  bool running = false;
};

inline QindaQt::Network::Client::ClientTiming fastTiming() {
  QindaQt::Network::Client::ClientTiming timing;
  timing.requestTimeoutMilliseconds = 100;
  timing.retryMilliseconds = {100};
  return timing;
}

} // namespace QindaQt::Apps::SettingsNetwork::TestSupport
