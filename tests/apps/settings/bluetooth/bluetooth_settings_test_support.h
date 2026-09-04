// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_client/bluetooth_transport.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

namespace QindaQt::Apps::SettingsBluetooth::TestSupport {

class FakeBluetoothTransport final
    : public QindaQt::Bluetooth::BluetoothTransport {
public:
  using BluetoothTransport::BluetoothTransport;

  struct Submission {
    QString owner;
    quint64 requestId = 0;
    QindaQt::Bluetooth::OperationRequest request;
  };

  void start() override { ++startCount; }
  void stop() override { ++stopCount; }
  void fetchSnapshot(const QString &owner, quint64 requestId) override {
    fetches.append({owner, requestId});
  }
  void submitOperation(
      const QString &owner, quint64 requestId,
      const QindaQt::Bluetooth::OperationRequest &request) override {
    submissions.append({owner, requestId, request});
  }
  void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
  void finishSnapshot(const QString &owner, quint64 requestId,
                      const QindaQt::Bluetooth::Snapshot &snapshot) {
    Q_EMIT snapshotReply(owner, requestId, true, snapshot, {});
  }
  void finishOperation(
      const Submission &submission,
      const QindaQt::Bluetooth::OperationResult &result) {
    Q_EMIT operationReply(submission.owner, submission.requestId, true,
                          result, {});
  }

  QList<QPair<QString, quint64>> fetches;
  QList<Submission> submissions;
  int startCount = 0;
  int stopCount = 0;
};

inline QindaQt::Bluetooth::Snapshot readySnapshot(
    quint64 epoch = 61, quint64 revision = 5,
    bool discovering = false) {
  using namespace QindaQt::Bluetooth;
  Snapshot snapshot;
  snapshot.schemaVersion = kSchemaVersion;
  snapshot.epoch = epoch;
  snapshot.revision = revision;
  snapshot.availability = Availability::Ready;
  snapshot.capabilities = Capability::SetAdapterPower
      | Capability::DiscoveryLease | Capability::ConnectPaired
      | Capability::DisconnectPaired | Capability::Pair
      | Capability::RemoveDevice | Capability::SetTrusted
      | Capability::PairingPrompt;
  snapshot.reasonCode = QStringLiteral("ready");
  snapshot.adapters = {{.handle = {epoch, 400},
                        .address = QStringLiteral("AA:BB:CC:00:11:22"),
                        .name = QStringLiteral("AA:BB:CC:00:11:22"),
                        .powered = true,
                        .discovering = discovering}};
  snapshot.devices = {
      {.handle = {epoch, 700},
       .adapterHandle = {epoch, 400},
       .address = QStringLiteral("AA:BB:CC:33:44:55"),
       .name = QStringLiteral("Headphones"),
       .deviceClass = DeviceClass::Headphones,
       .paired = true,
       .connected = true,
       .rssiKnown = true,
       .rssi = -42},
      {.handle = {epoch, 701},
       .adapterHandle = {epoch, 400},
       .address = QStringLiteral("AA:BB:CC:33:44:56"),
       .name = QStringLiteral("Keyboard"),
       .deviceClass = DeviceClass::Keyboard,
       .paired = true,
       .connected = false,
       .rssiKnown = true,
       .rssi = -58},
      {.handle = {epoch, 702},
       .adapterHandle = {epoch, 400},
       .address = QStringLiteral("AA:BB:CC:33:44:57"),
       .name = QStringLiteral("New phone"),
       .deviceClass = DeviceClass::Phone,
       .paired = false,
       .connected = false,
       .rssiKnown = false,
       .rssi = 0},
  };
  return snapshot;
}

inline QindaQt::Bluetooth::OperationResult successResult(
    const FakeBluetoothTransport::Submission &submission,
    quint64 observedRevision, quint64 initiatingRevision = 5) {
  using namespace QindaQt::Bluetooth;
  return {.kind = submission.request.kind,
          .status = OperationStatus::Succeeded,
          .initiatingEpoch = submission.request.target.epoch,
          .initiatingRevision = initiatingRevision,
          .observedEpoch = submission.request.target.epoch,
          .observedRevision = observedRevision,
          .reasonCode = QStringLiteral("ready"),
          .diagnostic = {},
          .wireValid = true};
}

} // namespace QindaQt::Apps::SettingsBluetooth::TestSupport
