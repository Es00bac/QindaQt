// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtCore/QObject>

namespace QindaQt::Bluetooth
{

class FakeAdapterInterface;

class BluetoothModel : public QObject
{
    Q_OBJECT
public:
    explicit BluetoothModel(QObject *parent = nullptr);
    ~BluetoothModel() override;

    quint64 nextEpoch();
    quint64 nextRevision();
    quint64 nextSerial();

    Snapshot currentSnapshot() const;
    const QList<Adapter> &adapters() const { return m_adapters; }
    const QList<Device> &devices() const { return m_devices; }

    OperationResult executeOperation(const OperationRequest &request);

Q_SIGNALS:
    void snapshotChanged(quint64 epoch, quint64 revision);

private:
    quint64 m_epoch = 0;
    quint64 m_revision = 0;
    quint64 m_nextSerial = 1;
    QList<Adapter> m_adapters;
    QList<Device> m_devices;
    std::unique_ptr<FakeAdapterInterface> m_fakeAdapter;

    void initializeFakeState();
    OperationResult executePairOperation(const OperationRequest &request);
    OperationResult executeConnectOperation(const OperationRequest &request);
    OperationResult executeDisconnectOperation(const OperationRequest &request);
    OperationResult executeTrustOperation(const OperationRequest &request);
};

} // namespace QindaQt::Bluetooth
