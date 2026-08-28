// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_client/bluetooth_transport.h>

namespace QindaQt::Tests
{

class FakeBluetoothTransport final : public Bluetooth::BluetoothTransport
{
public:
    using BluetoothTransport::BluetoothTransport;

    void start() override
    {
        ++startCalls;
        running = true;
    }
    void stop() override
    {
        ++stopCalls;
        running = false;
        setOwner({});
    }
    void fetchSnapshot(const QString &owner, const quint64 requestId) override
    {
        fetches.push_back({owner, requestId});
    }
    void submitOperation(const QString &owner, const quint64 requestId,
                         const Bluetooth::OperationRequest &request) override
    {
        submissions.push_back({owner, requestId, request});
    }

    void setOwner(const QString &owner)
    {
        currentOwner = owner;
        Q_EMIT ownerChanged(currentOwner);
    }
    void emitInvalidated(const QString &owner, const quint64 epoch, const quint64 revision)
    {
        Q_EMIT invalidated(owner, epoch, revision);
    }
    void emitSnapshotReply(const QString &owner, const quint64 requestId,
                           const bool success, const Bluetooth::Snapshot &snapshot,
                           const QString &reason = {})
    {
        Q_EMIT snapshotReply(owner, requestId, success, snapshot, reason);
    }
    void emitOperationReply(const QString &owner, const quint64 requestId,
                            const bool success,
                            const Bluetooth::OperationResult &result,
                            const QString &reason = {})
    {
        Q_EMIT operationReply(owner, requestId, success, result, reason);
    }

    struct RecordedFetch {
        QString owner;
        quint64 requestId = 0;
    };
    struct RecordedSubmission {
        QString owner;
        quint64 requestId = 0;
        Bluetooth::OperationRequest request;
    };

    QList<RecordedFetch> fetches;
    QList<RecordedSubmission> submissions;
    QString currentOwner;
    int startCalls = 0;
    int stopCalls = 0;
    bool running = false;
};

inline Bluetooth::Snapshot bluetoothClientSnapshot(const quint64 epoch = 61,
                                                   const quint64 revision = 5)
{
    using Bluetooth::Adapter;
    using Bluetooth::Availability;
    using Bluetooth::Capability;
    using Bluetooth::Capabilities;
    using Bluetooth::Device;
    using Bluetooth::DeviceClass;
    using Bluetooth::Snapshot;

    Snapshot snapshot;
    snapshot.schemaVersion = Bluetooth::kSchemaVersion;
    snapshot.epoch = epoch;
    snapshot.revision = revision;
    snapshot.availability = Availability::Ready;
    snapshot.capabilities = Capability::SetAdapterPower | Capability::DiscoveryLease
        | Capability::ConnectPaired | Capability::DisconnectPaired;
    snapshot.reasonCode = QStringLiteral("ready");
    snapshot.adapters = {{.handle = {.epoch = epoch, .serial = 400},
                          .address = QStringLiteral("AA:BB:CC:00:11:22"),
                          .name = QStringLiteral("Internal adapter"),
                          .powered = true,
                          .discovering = false}};
    snapshot.devices = {{.handle = {.epoch = epoch, .serial = 700},
                         .adapterHandle = {.epoch = epoch, .serial = 400},
                         .address = QStringLiteral("AA:BB:CC:33:44:55"),
                         .name = QStringLiteral("Keyboard"),
                         .deviceClass = DeviceClass::Keyboard,
                         .paired = true,
                         .connected = true,
                         .rssiKnown = false,
                         .rssi = 0}};
    return snapshot;
}

} // namespace QindaQt::Tests
