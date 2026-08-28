// SPDX-License-Identifier: GPL-3.0-or-later

#include "deterministic_adapter_backend.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtTest>

using namespace QindaQt::Bluetooth;

namespace
{

constexpr quint64 kAnyOperationId = 1;
constexpr QString kCaller = QStringLiteral(":1.7");

QString kAdapterAddress()
{
    return QStringLiteral("AA:BB:CC:00:11:22");
}

BackendInventory adapterInventory(const bool powered = true)
{
    BackendInventory inventory;
    inventory.adapters = {{.address = kAdapterAddress(),
                           .name = QStringLiteral("Internal adapter"),
                           .powered = powered,
                           .discovering = false}};
    return inventory;
}

BackendRequest request(const OperationKind kind, const QString &deviceAddress = {},
                       const bool powered = false, const QString &caller = kCaller)
{
    return {.kind = kind,
            .adapterAddress = kAdapterAddress(),
            .deviceAddress = deviceAddress,
            .powered = powered,
            .callerId = caller};
}

} // namespace

class DeterministicAdapterBackendTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsEmptyAndPublishes();
    void powerTransitionsDropConnectionsAndDiscovery();
    void discoveryLeasesReferenceCountByCaller();
    void releasingOwnerStopsDiscovery();
    void connectAndDisconnectEnforcePairedPolicy();
    void rejectsUnknownTargets();
};

void DeterministicAdapterBackendTests::startsEmptyAndPublishes()
{
    DeterministicAdapterBackend backend;
    QSignalSpy inventories(&backend, &AdapterBackend::inventoryChanged);
    const quint64 generation = backend.start();
    QVERIFY(generation != 0);
    QCOMPARE(backend.isRunning(), true);
    QTRY_COMPARE(inventories.count(), 1);
    QVERIFY(backend.inventory().adapters.isEmpty());
    QCOMPARE(backend.submitCalls(), 0);
}

void DeterministicAdapterBackendTests::powerTransitionsDropConnectionsAndDiscovery()
{
    DeterministicAdapterBackend backend;
    QSignalSpy inventories(&backend, &AdapterBackend::inventoryChanged);
    QSignalSpy outcomes(&backend, &AdapterBackend::operationFinished);
    backend.start();

    BackendInventory inventory = adapterInventory();
    inventory.devices = {{.adapterAddress = kAdapterAddress(),
                          .address = QStringLiteral("AA:BB:CC:33:44:55"),
                          .name = QStringLiteral("Keyboard"),
                          .deviceClass = DeviceClass::Keyboard,
                          .paired = true,
                          .connected = true,
                          .rssiKnown = false,
                          .rssi = 0}};
    backend.setInventory(inventory);
    backend.submit(kAnyOperationId, request(OperationKind::AcquireDiscovery));
    QTRY_COMPARE(backend.inventory().adapters.constFirst().discovering, true);

    backend.submit(kAnyOperationId + 1,
                   request(OperationKind::SetAdapterPower, {}, false));
    QTRY_COMPARE(backend.inventory().adapters.constFirst().powered, false);
    QCOMPARE(backend.inventory().adapters.constFirst().discovering, false);
    QCOMPARE(backend.inventory().devices.constFirst().connected, false);
    QTRY_COMPARE(outcomes.count(), 2);
    // AGENT-CONTRACT under test: powering off is a successful typed result even
    // though it terminates discovery and connections, matching BlueZ truth.
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("adapter-power-set"));
    QVERIFY(inventories.count() >= 3);
}

void DeterministicAdapterBackendTests::discoveryLeasesReferenceCountByCaller()
{
    DeterministicAdapterBackend backend;
    QSignalSpy outcomes(&backend, &AdapterBackend::operationFinished);
    backend.start();
    backend.setInventory(adapterInventory());

    backend.submit(1, request(OperationKind::AcquireDiscovery));
    backend.submit(2, request(OperationKind::AcquireDiscovery));
    QTRY_COMPARE(backend.inventory().leases.size(), 1);
    QCOMPARE(backend.inventory().leases.constFirst().refcount, quint32(2));
    QCOMPARE(backend.inventory().adapters.constFirst().discovering, true);

    backend.submit(3, request(OperationKind::ReleaseDiscovery));
    QTRY_COMPARE(backend.inventory().leases.constFirst().refcount, quint32(1));
    QCOMPARE(backend.inventory().adapters.constFirst().discovering, true);

    backend.submit(4, request(OperationKind::ReleaseDiscovery));
    QTRY_COMPARE(backend.inventory().leases.size(), 0);
    QCOMPARE(backend.inventory().adapters.constFirst().discovering, false);
    QTRY_COMPARE(outcomes.count(), 4);

    // Releasing with no held lease is a typed rejection, not a crash.
    backend.submit(5, request(OperationKind::ReleaseDiscovery));
    QTRY_COMPARE(outcomes.count(), 5);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().status,
             BackendOperationStatus::Rejected);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("no-lease"));
}

void DeterministicAdapterBackendTests::releasingOwnerStopsDiscovery()
{
    DeterministicAdapterBackend backend;
    QSignalSpy inventories(&backend, &AdapterBackend::inventoryChanged);
    backend.start();
    backend.setInventory(adapterInventory());
    backend.submit(1, request(OperationKind::AcquireDiscovery));
    backend.submit(2, request(OperationKind::AcquireDiscovery, {}, false,
                              QStringLiteral(":1.9")));
    QTRY_COMPARE(backend.inventory().leases.size(), 2);

    backend.releaseOwner(kCaller);
    QCOMPARE(backend.releaseOwnerCalls(), 1);
    QTRY_COMPARE(backend.inventory().leases.size(), 1);
    QCOMPARE(backend.inventory().adapters.constFirst().discovering, true);

    backend.releaseOwner(QStringLiteral(":1.9"));
    QTRY_COMPARE(backend.inventory().leases.size(), 0);
    QCOMPARE(backend.inventory().adapters.constFirst().discovering, false);
    QVERIFY(inventories.count() >= 3);
}

void DeterministicAdapterBackendTests::connectAndDisconnectEnforcePairedPolicy()
{
    DeterministicAdapterBackend backend;
    QSignalSpy outcomes(&backend, &AdapterBackend::operationFinished);
    backend.start();
    BackendInventory inventory = adapterInventory();
    inventory.devices = {
        {.adapterAddress = kAdapterAddress(),
         .address = QStringLiteral("AA:BB:CC:33:44:55"),
         .name = QStringLiteral("Keyboard"),
         .deviceClass = DeviceClass::Keyboard,
         .paired = true,
         .connected = false,
         .rssiKnown = false,
         .rssi = 0},
        {.adapterAddress = kAdapterAddress(),
         .address = QStringLiteral("AA:BB:CC:33:44:66"),
         .name = QStringLiteral("Mouse"),
         .deviceClass = DeviceClass::Mouse,
         .paired = false,
         .connected = false,
         .rssiKnown = false,
         .rssi = 0}};
    backend.setInventory(inventory);

    backend.submit(1, request(OperationKind::Connect,
                             QStringLiteral("AA:BB:CC:33:44:55")));
    QTRY_COMPARE(outcomes.count(), 1);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().status,
             BackendOperationStatus::Succeeded);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("connected"));
    QTRY_COMPARE(backend.inventory().devices.at(0).connected, true);

    backend.submit(2, request(OperationKind::Connect,
                              QStringLiteral("AA:BB:CC:33:44:66")));
    QTRY_COMPARE(outcomes.count(), 2);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().status,
             BackendOperationStatus::Rejected);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("not-paired"));

    backend.submit(3, request(OperationKind::Disconnect,
                              QStringLiteral("AA:BB:CC:33:44:66")));
    QTRY_COMPARE(outcomes.count(), 3);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("not-connected"));

    backend.submit(4, request(OperationKind::Disconnect,
                              QStringLiteral("AA:BB:CC:33:44:55")));
    QTRY_COMPARE(outcomes.count(), 4);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("disconnected"));
    QTRY_COMPARE(backend.inventory().devices.at(0).connected, false);
}

void DeterministicAdapterBackendTests::rejectsUnknownTargets()
{
    DeterministicAdapterBackend backend;
    QSignalSpy outcomes(&backend, &AdapterBackend::operationFinished);
    backend.start();

    backend.submit(1, request(OperationKind::SetAdapterPower, {}, true));
    QTRY_COMPARE(outcomes.count(), 1);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().status,
             BackendOperationStatus::Rejected);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("stale-handle"));

    backend.submit(2, request(OperationKind::Connect,
                              QStringLiteral("AA:BB:CC:33:44:55")));
    QTRY_COMPARE(outcomes.count(), 2);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("stale-handle"));
}

QTEST_GUILESS_MAIN(DeterministicAdapterBackendTests)
#include "tst_deterministic_adapter_backend.moc"
