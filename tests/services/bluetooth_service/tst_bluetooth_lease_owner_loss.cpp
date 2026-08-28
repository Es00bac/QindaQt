// SPDX-License-Identifier: GPL-3.0-or-later

#include "deterministic_adapter_backend.h"
#include "support/private_bus.h"

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>
#include <qindaqt/services/bluetooth_client/qt_bluetooth_transport.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_dbus.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>

#include <QtDBus/QDBusConnection>
#include <QtTest>

#include <memory>

using namespace QindaQt::Bluetooth;
using namespace QindaQt::Tests;

namespace
{

BackendInventory poweredAdapterInventory()
{
    BackendInventory inventory;
    inventory.adapters = {{.address = QStringLiteral("AA:BB:CC:00:11:22"),
                           .name = QStringLiteral("Internal adapter"),
                           .powered = true,
                           .discovering = false}};
    return inventory;
}

} // namespace

// AGENT-CONTRACT under test: a discovery lease is bound to its caller's
// unique D-Bus name. When that exact connection disappears from the bus
// without replacement, the resident service must drop the caller's lease and
// stop discovery, even though the client process never released politely.
class BluetoothLeaseOwnerLossTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void vanishedCallerReleasesDiscoveryLease();
};

void BluetoothLeaseOwnerLossTests::vanishedCallerReleasesDiscoveryLease()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.BluetoothLeaseTest.p%1")
                                    .arg(QCoreApplication::applicationPid());

    const QString serviceConnectionName = bus.name + QStringLiteral("-service");
    QDBusConnection serviceConnection =
        QDBusConnection::connectToBus(bus.address, serviceConnectionName);
    QVERIFY(serviceConnection.isConnected());

    auto backend = std::make_unique<DeterministicAdapterBackend>();
    DeterministicAdapterBackend *backendPtr = backend.get();
    auto service = std::make_unique<ResidentBluetoothService>(
        std::move(backend), serviceConnection, serviceName, 8101);
    QCOMPARE(service->start(), ServiceStartStatus::Started);
    backendPtr->setInventory(poweredAdapterInventory());
    QTRY_COMPARE(backendPtr->inventory().adapters.constFirst().powered, true);

    // The lease-holding caller is a distinct bus connection, exactly like a
    // separate client process.
    const QString leaseConnectionName = bus.name + QStringLiteral("-lease-holder");
    QDBusConnection leaseConnection =
        QDBusConnection::connectToBus(bus.address, leaseConnectionName);
    QVERIFY(leaseConnection.isConnected());

    auto transport = std::make_unique<QtBluetoothTransport>(leaseConnection, serviceName);
    auto client = std::make_unique<BluetoothClient>(transport.get());
    client->start();
    QTRY_COMPARE(client->state(), ClientState::Ready);
    const Handle adapter = client->snapshot().adapters.constFirst().handle;
    const quint64 requestId = client->acquireDiscovery(adapter);
    QTRY_COMPARE(backendPtr->inventory().adapters.constFirst().discovering, true);
    QCOMPARE(backendPtr->inventory().leases.size(), 1);
    QVERIFY(requestId != 0);

    // Simulate caller death: destroy the client/transport without a polite
    // stop-release and disconnect the exact bus connection. The service must
    // observe the unique-name loss and release the lease on its own.
    client.reset();
    transport.reset();
    leaseConnection = QDBusConnection(QString{});
    QDBusConnection::disconnectFromBus(leaseConnectionName);
    QTRY_COMPARE(backendPtr->inventory().leases.size(), 0);
    QTRY_COMPARE(backendPtr->inventory().adapters.constFirst().discovering, false);

    service->stop();
    service.reset();
    QDBusConnection::disconnectFromBus(serviceConnectionName);
}

QTEST_GUILESS_MAIN(BluetoothLeaseOwnerLossTests)
#include "tst_bluetooth_lease_owner_loss.moc"
