// SPDX-License-Identifier: GPL-3.0-or-later

#include "deterministic_adapter_backend.h"

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtTest>

using namespace QindaQt::Bluetooth;

namespace
{

constexpr quint64 kAnyOperationId = 1;

QString kCaller()
{
    return QStringLiteral(":1.7");
}

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
                       const bool powered = false, const QString &caller = {})
{
    return {.kind = kind,
            .adapterAddress = kAdapterAddress(),
            .deviceAddress = deviceAddress,
            .powered = powered,
            .callerId = caller.isEmpty() ? kCaller() : caller};
}

} // namespace

class DeterministicAdapterBackendTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsEmptyAndPublishes();
    void powerTransitionsDropConnectionsAndDiscovery();
    void powerOffClearsLeasesAndPowerOnDoesNotResurrect();
    void stopClearsLeases();
    void totalLeaseCapEnforced();
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
    QVERIFY(backend.start() != 0);

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
    // Power-off terminates the discovery session and releases its lease.
    QCOMPARE(backend.inventory().leases.size(), 0);
    QTRY_COMPARE(outcomes.count(), 2);
    // AGENT-CONTRACT under test: powering off is a successful typed result even
    // though it terminates discovery and connections, matching BlueZ truth.
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("adapter-power-set"));
    QVERIFY(inventories.count() >= 3);
}

void DeterministicAdapterBackendTests::powerOffClearsLeasesAndPowerOnDoesNotResurrect()
{
    DeterministicAdapterBackend backend;
    QVERIFY(backend.start() != 0);
    backend.setInventory(adapterInventory());

    backend.submit(1, request(OperationKind::AcquireDiscovery));
    QTRY_COMPARE(backend.inventory().adapters.constFirst().discovering, true);
    QCOMPARE(backend.inventory().leases.size(), 1);

    backend.submit(2, request(OperationKind::SetAdapterPower, {}, false));
    QTRY_COMPARE(backend.inventory().adapters.constFirst().powered, false);
    QTRY_COMPARE(backend.inventory().leases.size(), 0);
    QCOMPARE(backend.inventory().adapters.constFirst().discovering, false);

    // A later power-on must not resurrect discovery: no lease survived.
    backend.submit(3, request(OperationKind::SetAdapterPower, {}, true));
    QTRY_COMPARE(backend.inventory().adapters.constFirst().powered, true);
    QCOMPARE(backend.inventory().adapters.constFirst().discovering, false);
    QCOMPARE(backend.inventory().leases.size(), 0);
}

void DeterministicAdapterBackendTests::stopClearsLeases()
{
    DeterministicAdapterBackend backend;
    QVERIFY(backend.start() != 0);
    backend.setInventory(adapterInventory());
    backend.submit(1, request(OperationKind::AcquireDiscovery));
    QTRY_COMPARE(backend.inventory().leases.size(), 1);

    backend.stop();
    QCOMPARE(backend.isRunning(), false);
    // AGENT-GUARD under test: lease holds are per-run state and must never
    // cross a stop/start boundary.
    QCOMPARE(backend.inventory().leases.size(), 0);

    QVERIFY(backend.start() != 0);
    QTRY_COMPARE(backend.inventory().leases.size(), 0);
    QTRY_COMPARE(backend.inventory().adapters.constFirst().discovering, false);
}

void DeterministicAdapterBackendTests::totalLeaseCapEnforced()
{
    DeterministicAdapterBackend backend;
    QVERIFY(backend.start() != 0);
    BackendInventory inventory;
    // Five powered adapters: each stays below the per-adapter cap while the
    // total cap trips on the 65th acquisition.
    for (int index = 0; index < 5; ++index) {
        inventory.adapters.push_back(
            {.address = QStringLiteral("AA:BB:CC:00:11:%1").arg(22 + index, 2, 10,
                                                                QLatin1Char('0')),
             .name = QStringLiteral("Adapter %1").arg(index),
             .powered = true,
             .discovering = false});
    }
    backend.setInventory(inventory);

    for (int adapterIndex = 0; adapterIndex < 4; ++adapterIndex) {
        const QString adapterAddr = inventory.adapters.at(adapterIndex).address;
        for (int leaseIndex = 0; leaseIndex < kMaxDiscoveryLeasesPerAdapter;
             ++leaseIndex) {
            backend.submit(static_cast<quint64>(adapterIndex * 100 + leaseIndex),
                           {.kind = OperationKind::AcquireDiscovery,
                            .adapterAddress = adapterAddr,
                            .deviceAddress = {},
                            .powered = false,
                            .callerId = QStringLiteral(":1.%1-%2")
                                            .arg(adapterIndex)
                                            .arg(leaseIndex)});
        }
    }
    QTRY_COMPARE(backend.inventory().leases.size(), 4 * kMaxDiscoveryLeasesPerAdapter);

    backend.submit(9001,
                   {.kind = OperationKind::AcquireDiscovery,
                    .adapterAddress = QStringLiteral("AA:BB:CC:00:11:26"),
                    .deviceAddress = {},
                    .powered = false,
                    .callerId = QStringLiteral(":1.999")});
    QTRY_COMPARE(backend.inventory().leases.size(), 4 * kMaxDiscoveryLeasesPerAdapter);
    // The fifth adapter acquired nothing: the TOTAL bound rejected it.
    quint32 total = 0;
    for (const BackendLease &lease : backend.inventory().leases) {
        QVERIFY2(lease.adapterAddress != QStringLiteral("AA:BB:CC:00:11:26"),
                 qPrintable(lease.adapterAddress));
        total += lease.refcount;
    }
    QCOMPARE(total, quint32(kMaxDiscoveryLeasesTotal));
}

void DeterministicAdapterBackendTests::discoveryLeasesReferenceCountByCaller()
{
    DeterministicAdapterBackend backend;
    QSignalSpy outcomes(&backend, &AdapterBackend::operationFinished);
    QVERIFY(backend.start() != 0);
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
    QVERIFY(backend.start() != 0);
    backend.setInventory(adapterInventory());
    backend.submit(1, request(OperationKind::AcquireDiscovery));
    backend.submit(2, request(OperationKind::AcquireDiscovery, {}, false,
                              QStringLiteral(":1.9")));
    QTRY_COMPARE(backend.inventory().leases.size(), 2);

    backend.releaseOwner(kCaller());
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
    QVERIFY(backend.start() != 0);
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

    // Connecting an already-connected device is a typed rejection.
    backend.submit(5, request(OperationKind::Connect,
                              QStringLiteral("AA:BB:CC:33:44:55")));
    QTRY_COMPARE(outcomes.count(), 2);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().status,
             BackendOperationStatus::Rejected);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("already-connected"));

    backend.submit(2, request(OperationKind::Connect,
                              QStringLiteral("AA:BB:CC:33:44:66")));
    QTRY_COMPARE(outcomes.count(), 3);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().status,
             BackendOperationStatus::Rejected);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("not-paired"));

    backend.submit(3, request(OperationKind::Disconnect,
                              QStringLiteral("AA:BB:CC:33:44:66")));
    QTRY_COMPARE(outcomes.count(), 4);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("not-connected"));

    backend.submit(4, request(OperationKind::Disconnect,
                              QStringLiteral("AA:BB:CC:33:44:55")));
    QTRY_COMPARE(outcomes.count(), 5);
    QCOMPARE(outcomes.last()[2].value<BackendOperationOutcome>().reasonCode,
             QStringLiteral("disconnected"));
    QTRY_COMPARE(backend.inventory().devices.at(0).connected, false);
}

void DeterministicAdapterBackendTests::rejectsUnknownTargets()
{
    DeterministicAdapterBackend backend;
    QSignalSpy outcomes(&backend, &AdapterBackend::operationFinished);
    QVERIFY(backend.start() != 0);

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
