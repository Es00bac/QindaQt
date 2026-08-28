// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Bluetooth
{

BluetoothClient::BluetoothClient(QDBusConnection &connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
{
    registerDBusTypes();

    auto watcher = new QDBusServiceWatcher(QString::fromLatin1(kServiceName),
                                           m_connection, QDBusServiceWatcher::WatchForOwnerChange,
                                           this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            &BluetoothClient::onServiceOwnerChanged);
}

BluetoothClient::~BluetoothClient() = default;

bool BluetoothClient::bindService()
{
    updateServiceOwner();
    if (m_uniqueOwner.isEmpty()) {
        return false;
    }
    refetchSnapshot();
    m_serviceConnected = true;
    Q_EMIT connected();
    return true;
}

Snapshot BluetoothClient::snapshot() const
{
    return m_snapshot;
}

OperationResult BluetoothClient::executeOperation(const OperationRequest &request)
{
    OperationResult result;
    result.kind = request.kind;
    result.status = OperationStatus::Failed;
    result.reasonCode = QStringLiteral("client-unavailable");
    result.wireValid = true;

    if (m_uniqueOwner.isEmpty()) {
        return result;
    }

    // In a full implementation, this would be an async D-Bus call.
    // For the B0 foundation, we return a placeholder uncertain result
    // that forces the caller to refetch.
    result.status = OperationStatus::Uncertain;
    result.reasonCode = QStringLiteral("operation-timeout");
    result.initiatingEpoch = m_snapshot.epoch;
    result.initiatingRevision = m_snapshot.revision;
    result.observedEpoch = m_snapshot.epoch;
    result.observedRevision = m_snapshot.revision;

    return result;
}

void BluetoothClient::onSnapshotChanged(quint64 epoch, quint64 revision)
{
    refetchSnapshot();
    Q_EMIT snapshotChanged(epoch, revision);
}

void BluetoothClient::onServiceOwnerChanged(const QString &name, const QString &oldOwner,
                                            const QString &newOwner)
{
    if (name != QString::fromLatin1(kServiceName)) {
        return;
    }

    if (oldOwner == m_uniqueOwner && !newOwner.isEmpty()) {
        // Service restarted with new owner
        m_uniqueOwner = newOwner;
        refetchSnapshot();
    } else if (newOwner.isEmpty()) {
        // Service disappeared
        m_serviceConnected = false;
        m_uniqueOwner.clear();
        Q_EMIT disconnected();
    }
}

void BluetoothClient::updateServiceOwner()
{
    QDBusReply<QString> reply = m_connection.interface()->serviceOwner(
        QString::fromLatin1(kServiceName));
    if (reply.isValid()) {
        m_uniqueOwner = reply.value();
    }
}

void BluetoothClient::refetchSnapshot()
{
    if (m_uniqueOwner.isEmpty()) {
        return;
    }

    // In a full implementation, this would be an async D-Bus call.
    // For B0, we retain the last known snapshot.
}

} // namespace QindaQt::Bluetooth
