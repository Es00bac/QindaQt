// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtCore/QObject>

namespace QindaQt::Bluetooth
{

// Transport implementations bind every request and signal to one exact unique
// owner. They never decide publication, retries, or operation replay policy.
// Implementations and callers share one Qt thread; every completion is emitted
// asynchronously as a bounded value and late completion is permitted.
class BluetoothTransport : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothTransport(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~BluetoothTransport() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fetchSnapshot(const QString &owner, quint64 requestId) = 0;
    virtual void submitOperation(const QString &owner, quint64 requestId,
                                 const OperationRequest &request) = 0;

Q_SIGNALS:
    void ownerChanged(const QString &owner);
    void invalidated(const QString &owner, quint64 epoch, quint64 revision);
    void snapshotReply(const QString &owner, quint64 requestId, bool transportSuccess,
                       const QindaQt::Bluetooth::Snapshot &snapshot,
                       const QString &reasonCode);
    void operationReply(const QString &owner, quint64 requestId, bool transportSuccess,
                        const QindaQt::Bluetooth::OperationResult &result,
                        const QString &reasonCode);
};

} // namespace QindaQt::Bluetooth
