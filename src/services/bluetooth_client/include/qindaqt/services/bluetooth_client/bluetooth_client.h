// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

#include <QtCore/QObject>
#include <QtCore/QMetaType>

class QDBusConnection;
class QDBusPendingCallWatcher;

namespace QindaQt::Bluetooth
{

// Client for exact-owner discovery, snapshot fetching, and operation execution.
// AGENT-NOTE: Binds to the unique service owner, not the well-known name, to
// enable precise authority change detection. A unique-owner change means the
// client must discard its snapshot and refetch from the new owner.
class BluetoothClient : public QObject
{
    Q_OBJECT
public:
    explicit BluetoothClient(QDBusConnection &connection, QObject *parent = nullptr);
    ~BluetoothClient() override;

    // Discover and bind to the exact service owner
    bool bindService();

    // Fetch current snapshot atomically
    Snapshot snapshot() const;

    // Execute typed operation; must refetch on uncertainty
    OperationResult executeOperation(const OperationRequest &request);

    bool isConnected() const { return m_serviceConnected; }

Q_SIGNALS:
    void connected();
    void disconnected();
    void snapshotChanged(quint64 epoch, quint64 revision);

private Q_SLOTS:
    void onSnapshotChanged(quint64 epoch, quint64 revision);
    void onServiceOwnerChanged(const QString &name, const QString &oldOwner,
                               const QString &newOwner);

private:
    void updateServiceOwner();
    void refetchSnapshot();

    QDBusConnection &m_connection;
    QString m_uniqueOwner;
    Snapshot m_snapshot;
    bool m_serviceConnected = false;
};

} // namespace QindaQt::Bluetooth

Q_DECLARE_METATYPE(QindaQt::Bluetooth::Snapshot)
Q_DECLARE_METATYPE(QindaQt::Bluetooth::OperationResult)
