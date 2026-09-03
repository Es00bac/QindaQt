// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/bluez_harness.h"

#include <QtTest>

using namespace QindaQt::Bluetooth;
using BluezHarness = QindaQt::Tests::BluezHarness;
using FakeBluez = QindaQt::Tests::FakeBluez;

// Discovery leases, connect/disconnect operations, caller loss, and
// authority-loss uncertainty for the production BlueZ adapter. Every row runs
// against a private bus carrying a fake org.bluez.
class BluezAdapterOperationsTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void discoveryLeaseAcquireReleaseRefcount();
    void externalDiscoverySessionReconciled();
    void backendEnforcesLeaseCaps();
    void connectSuccessAndFailureReplies();
    void disconnectSuccessAndFailureReplies();
    void leaseDiesWithAdapterPowerOff();
    void ownerVanishedReleasesLeases();
    void ownerLossDuringDeferredConnectIsUncertain();

private:
    [[nodiscard]] static QString adapterAddress()
    {
        return QStringLiteral("AA:BB:CC:00:11:22");
    }
};

void BluezAdapterOperationsTests::discoveryLeaseAcquireReleaseRefcount()
{
    BluezHarness harness(7101);
    QVERIFY(harness.ready());
    (void)harness.fake->addAdapter(QStringLiteral("hci0"), adapterAddress(),
                             QStringLiteral("Internal"), true);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle handle = harness.snapshotAdapter().handle;

    const OperationSubmission first = harness.model->submit(
        {.kind = OperationKind::AcquireDiscovery, .target = handle},
        QStringLiteral(":1.10"));
    QVERIFY(first.pending);
    QCOMPARE(harness.awaitResult(completed, first.operationId)->status,
             OperationStatus::Succeeded);
    QVERIFY(harness.waitUntil(
        [&harness] { return harness.snapshotAdapter().discovering; }));

    // The same caller's second reference reuses the one platform session.
    const OperationSubmission second = harness.model->submit(
        {.kind = OperationKind::AcquireDiscovery, .target = handle},
        QStringLiteral(":1.10"));
    QVERIFY(second.pending);
    QCOMPARE(harness.awaitResult(completed, second.operationId)->status,
             OperationStatus::Succeeded);
    QCOMPARE(harness.fake->startDiscoveryCalls, 1);

    const OperationSubmission releaseOne = harness.model->submit(
        {.kind = OperationKind::ReleaseDiscovery, .target = handle},
        QStringLiteral(":1.10"));
    QVERIFY(releaseOne.pending);
    QCOMPARE(harness.awaitResult(completed, releaseOne.operationId)->status,
             OperationStatus::Succeeded);
    QVERIFY(harness.snapshotAdapter().discovering);
    QCOMPARE(harness.fake->stopDiscoveryCalls, 0);

    const OperationSubmission releaseTwo = harness.model->submit(
        {.kind = OperationKind::ReleaseDiscovery, .target = handle},
        QStringLiteral(":1.10"));
    QVERIFY(releaseTwo.pending);
    QCOMPARE(harness.awaitResult(completed, releaseTwo.operationId)->status,
             OperationStatus::Succeeded);
    QVERIFY(harness.waitUntil(
        [&harness] { return !harness.snapshotAdapter().discovering; }));
    QCOMPARE(harness.fake->stopDiscoveryCalls, 1);

    const OperationSubmission releaseAgain = harness.model->submit(
        {.kind = OperationKind::ReleaseDiscovery, .target = handle},
        QStringLiteral(":1.10"));
    QVERIFY(releaseAgain.pending);
    const std::optional<OperationResult> missingLease =
        harness.awaitResult(completed, releaseAgain.operationId);
    QVERIFY(missingLease.has_value());
    QCOMPARE(missingLease->status, OperationStatus::Rejected);
    QCOMPARE(missingLease->reasonCode, QStringLiteral("no-lease"));
}

void BluezAdapterOperationsTests::externalDiscoverySessionReconciled()
{
    BluezHarness harness(7102);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), adapterAddress(), QStringLiteral("Internal"), true);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle handle = harness.snapshotAdapter().handle;

    // Another BlueZ client (bluetoothctl) starts discovery. Bluetooth1 holds
    // no lease, but the platform truth must stay representable and the model
    // must never classify it as malformed.
    harness.fake->setExternalDiscovery(adapterPath, true);
    QVERIFY(harness.waitUntil(
        [&harness] { return harness.snapshotAdapter().discovering; }));
    QCOMPARE(harness.model->snapshot().availability, Availability::Ready);

    const OperationSubmission acquire = harness.model->submit(
        {.kind = OperationKind::AcquireDiscovery, .target = handle},
        QStringLiteral(":1.10"));
    QVERIFY(acquire.pending);
    QCOMPARE(harness.awaitResult(completed, acquire.operationId)->status,
             OperationStatus::Succeeded);

    const OperationSubmission release = harness.model->submit(
        {.kind = OperationKind::ReleaseDiscovery, .target = handle},
        QStringLiteral(":1.10"));
    QVERIFY(release.pending);
    QCOMPARE(harness.awaitResult(completed, release.operationId)->status,
             OperationStatus::Succeeded);
    // The external session outlives our release.
    QVERIFY(harness.snapshotAdapter().discovering);
    QCOMPARE(harness.model->snapshot().availability, Availability::Ready);

    harness.fake->setExternalDiscovery(adapterPath, false);
    QVERIFY(harness.waitUntil(
        [&harness] { return !harness.snapshotAdapter().discovering; }));
    QCOMPARE(harness.model->snapshot().availability, Availability::Ready);
}

void BluezAdapterOperationsTests::backendEnforcesLeaseCaps()
{
    BluezHarness harness(7103);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(QStringLiteral("hci0"), adapterAddress(),
                             QStringLiteral("Internal"), true);
    QVERIFY(harness.fake->takeOwnership());
    QSignalSpy publications(harness.backend.get(), &AdapterBackend::inventoryChanged);
    (void)harness.backend->start();
    QVERIFY(QTest::qWaitFor([&] {
        return !publications.isEmpty()
            && publications.last().value(1).value<BackendInventory>().adapters.size()
                   == 1;
    }));
    // The backend owns the live table even when the model is not dispatching:
    // seventeen distinct callers cannot all hold leases on one adapter.
    QSignalSpy completed(harness.backend.get(), &AdapterBackend::operationFinished);
    for (int index = 1; index <= 17; ++index) {
        BackendRequest request;
        request.kind = OperationKind::AcquireDiscovery;
        request.adapterAddress = adapterAddress();
        request.callerId = QStringLiteral(":1.%1").arg(index);
        harness.backend->submit(static_cast<quint64>(index), request);
    }
    int succeeded = 0;
    int rejected = 0;
    QVERIFY(QTest::qWaitFor([&] {
        succeeded = 0;
        rejected = 0;
        for (const QList<QVariant> &arguments : completed) {
            const auto outcome =
                arguments.value(2).value<BackendOperationOutcome>();
            if (outcome.status == BackendOperationStatus::Succeeded) {
                ++succeeded;
            } else if (outcome.reasonCode == QLatin1String("too-many-leases")) {
                ++rejected;
            }
        }
        return succeeded + rejected == 17;
    }));
    QCOMPARE(succeeded, 16);
    QCOMPARE(rejected, 1);
    QCOMPARE(harness.fake->startDiscoveryCalls, 1);
}

void BluezAdapterOperationsTests::connectSuccessAndFailureReplies()
{
    BluezHarness harness(7104);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), adapterAddress(), QStringLiteral("Internal"), true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("Keyboard"));
    FakeBluez::DeviceEntity *device = harness.fake->device(devicePath);
    device->paired = true;
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle deviceHandle = harness.model->snapshot().devices.constFirst().handle;

    const OperationSubmission connect = harness.model->submit(
        {.kind = OperationKind::Connect, .target = deviceHandle},
        QStringLiteral(":1.10"));
    QVERIFY(connect.pending);
    const std::optional<OperationResult> connected =
        harness.awaitResult(completed, connect.operationId);
    QVERIFY(connected.has_value());
    QCOMPARE(connected->status, OperationStatus::Succeeded);
    QCOMPARE(connected->reasonCode, QStringLiteral("connected"));
    QVERIFY(harness.waitUntil([&harness] {
        return harness.model->snapshot().devices.constFirst().connected;
    }));

    // Store truth and platform truth drift: the backend re-checks, dispatches,
    // and maps BlueZ's AlreadyConnected rejection without mutating anything.
    harness.fake->emitDeviceProperties(devicePath, {{QStringLiteral("Connected"), false}});
    QVERIFY(harness.waitUntil([&harness] {
        return !harness.model->snapshot().devices.constFirst().connected;
    }));
    QSignalSpy backendCompleted(harness.backend.get(),
                                &AdapterBackend::operationFinished);
    BackendRequest request;
    request.kind = OperationKind::Connect;
    request.adapterAddress = adapterAddress();
    request.deviceAddress = QStringLiteral("AA:BB:CC:33:44:55");
    request.callerId = QStringLiteral(":1.10");
    harness.backend->submit(501, request);
    std::optional<BackendOperationOutcome> outcome;
    QVERIFY(QTest::qWaitFor([&] {
        for (const QList<QVariant> &arguments : backendCompleted) {
            if (arguments.value(1).toULongLong() == 501) {
                outcome = arguments.value(2).value<BackendOperationOutcome>();
                return true;
            }
        }
        return false;
    }));
    QVERIFY(outcome.has_value());
    QCOMPARE(outcome->status, BackendOperationStatus::Rejected);
    QCOMPARE(outcome->reasonCode, QStringLiteral("already-connected"));

    // A raw BlueZ failure reply maps to the stable failed classification.
    FakeBluez::DeviceEntity *failing = harness.fake->device(devicePath);
    failing->connected = false;
    failing->connectError = QStringLiteral("org.bluez.Error.Failed");
    harness.fake->emitDeviceProperties(devicePath, {{QStringLiteral("Connected"), false}});
    QVERIFY(harness.waitUntil([&harness] {
        return !harness.model->snapshot().devices.constFirst().connected;
    }));
    const OperationSubmission failedConnect = harness.model->submit(
        {.kind = OperationKind::Connect, .target = deviceHandle},
        QStringLiteral(":1.10"));
    QVERIFY(failedConnect.pending);
    const std::optional<OperationResult> failed =
        harness.awaitResult(completed, failedConnect.operationId);
    QVERIFY(failed.has_value());
    QCOMPARE(failed->status, OperationStatus::Failed);
    QCOMPARE(failed->reasonCode, QStringLiteral("bluez-error"));

    // The model rejects an unpaired device before any platform contact.
    FakeBluez::DeviceEntity *unpaired = harness.fake->device(devicePath);
    unpaired->connectError.clear();
    unpaired->paired = false;
    harness.fake->emitDeviceProperties(devicePath, {{QStringLiteral("Paired"), false}});
    QVERIFY(harness.waitUntil(
        [&harness] { return !harness.model->snapshot().devices.constFirst().paired; }));
    const OperationSubmission unpairedConnect = harness.model->submit(
        {.kind = OperationKind::Connect, .target = deviceHandle},
        QStringLiteral(":1.10"));
    QVERIFY(!unpairedConnect.pending);
    QCOMPARE(unpairedConnect.immediateResult.reasonCode, QStringLiteral("not-paired"));
}

void BluezAdapterOperationsTests::disconnectSuccessAndFailureReplies()
{
    BluezHarness harness(7105);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), adapterAddress(), QStringLiteral("Internal"), true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("Keyboard"));
    FakeBluez::DeviceEntity *device = harness.fake->device(devicePath);
    device->paired = true;
    device->connected = true;
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle deviceHandle = harness.model->snapshot().devices.constFirst().handle;

    const OperationSubmission disconnect = harness.model->submit(
        {.kind = OperationKind::Disconnect, .target = deviceHandle},
        QStringLiteral(":1.10"));
    QVERIFY(disconnect.pending);
    const std::optional<OperationResult> disconnected =
        harness.awaitResult(completed, disconnect.operationId);
    QVERIFY(disconnected.has_value());
    QCOMPARE(disconnected->status, OperationStatus::Succeeded);
    QCOMPARE(disconnected->reasonCode, QStringLiteral("disconnected"));
    QVERIFY(harness.waitUntil([&harness] {
        return !harness.model->snapshot().devices.constFirst().connected;
    }));

    // Platform-side NotConnected while the stored truth still says connected.
    FakeBluez::DeviceEntity *silent = harness.fake->device(devicePath);
    silent->connected = true;
    harness.fake->emitDeviceProperties(devicePath, {{QStringLiteral("Connected"), false}});
    QVERIFY(harness.waitUntil([&harness] {
        return !harness.model->snapshot().devices.constFirst().connected;
    }));
    QSignalSpy backendCompleted(harness.backend.get(),
                                &AdapterBackend::operationFinished);
    BackendRequest request;
    request.kind = OperationKind::Disconnect;
    request.adapterAddress = adapterAddress();
    request.deviceAddress = QStringLiteral("AA:BB:CC:33:44:55");
    request.callerId = QStringLiteral(":1.10");
    harness.backend->submit(601, request);
    std::optional<BackendOperationOutcome> outcome;
    QVERIFY(QTest::qWaitFor([&] {
        for (const QList<QVariant> &arguments : backendCompleted) {
            if (arguments.value(1).toULongLong() == 601) {
                outcome = arguments.value(2).value<BackendOperationOutcome>();
                return true;
            }
        }
        return false;
    }));
    QVERIFY(outcome.has_value());
    QCOMPARE(outcome->status, BackendOperationStatus::Rejected);
    QCOMPARE(outcome->reasonCode, QStringLiteral("not-connected"));

    // A raw failure reply from the platform.
    FakeBluez::DeviceEntity *failing = harness.fake->device(devicePath);
    failing->connected = true;
    failing->disconnectError = QStringLiteral("org.bluez.Error.Failed");
    harness.fake->emitDeviceProperties(devicePath, {{QStringLiteral("Connected"), true}});
    QVERIFY(harness.waitUntil(
        [&harness] { return harness.model->snapshot().devices.constFirst().connected; }));
    const OperationSubmission failedDisconnect = harness.model->submit(
        {.kind = OperationKind::Disconnect, .target = deviceHandle},
        QStringLiteral(":1.10"));
    QVERIFY(failedDisconnect.pending);
    const std::optional<OperationResult> failed =
        harness.awaitResult(completed, failedDisconnect.operationId);
    QVERIFY(failed.has_value());
    QCOMPARE(failed->status, OperationStatus::Failed);
    QCOMPARE(failed->reasonCode, QStringLiteral("bluez-error"));
}

void BluezAdapterOperationsTests::leaseDiesWithAdapterPowerOff()
{
    BluezHarness harness(7106);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), adapterAddress(), QStringLiteral("Internal"), true);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle handle = harness.snapshotAdapter().handle;

    const OperationSubmission acquire = harness.model->submit(
        {.kind = OperationKind::AcquireDiscovery, .target = handle},
        QStringLiteral(":1.10"));
    QVERIFY(acquire.pending);
    QCOMPARE(harness.awaitResult(completed, acquire.operationId)->status,
             OperationStatus::Succeeded);
    QVERIFY(harness.snapshotAdapter().discovering);

    harness.fake->setAdapterPowered(adapterPath, false);
    QVERIFY(harness.waitUntil(
        [&harness] { return !harness.snapshotAdapter().powered; }));
    QVERIFY(!harness.snapshotAdapter().discovering);
    QCOMPARE(harness.fake->stopDiscoveryCalls, 0);

    // The lease died with the session: releasing it reports no-lease.
    const OperationSubmission release = harness.model->submit(
        {.kind = OperationKind::ReleaseDiscovery,
         .target = harness.snapshotAdapter().handle},
        QStringLiteral(":1.10"));
    QVERIFY(release.pending);
    const std::optional<OperationResult> missingLease =
        harness.awaitResult(completed, release.operationId);
    QVERIFY(missingLease.has_value());
    QCOMPARE(missingLease->status, OperationStatus::Rejected);
    QCOMPARE(missingLease->reasonCode, QStringLiteral("no-lease"));

    // Powering back on never resurrects discovery.
    harness.fake->setAdapterPowered(adapterPath, true);
    QVERIFY(harness.waitUntil([&harness] { return harness.snapshotAdapter().powered; }));
    QVERIFY(!harness.snapshotAdapter().discovering);
}

void BluezAdapterOperationsTests::ownerVanishedReleasesLeases()
{
    BluezHarness harness(7107);
    QVERIFY(harness.ready());
    (void)harness.fake->addAdapter(QStringLiteral("hci0"), adapterAddress(),
                             QStringLiteral("Internal"), true);
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);
    const Handle handle = harness.snapshotAdapter().handle;

    for (const char *caller : {":1.10", ":1.11"}) {
        const OperationSubmission acquire = harness.model->submit(
            {.kind = OperationKind::AcquireDiscovery, .target = handle},
            QLatin1String(caller));
        QVERIFY(acquire.pending);
        QCOMPARE(harness.awaitResult(completed, acquire.operationId)->status,
                 OperationStatus::Succeeded);
    }
    QCOMPARE(harness.fake->startDiscoveryCalls, 1);
    QVERIFY(harness.snapshotAdapter().discovering);

    harness.model->ownerVanished(QStringLiteral(":1.10"));
    QVERIFY(harness.snapshotAdapter().discovering);
    QCOMPARE(harness.fake->stopDiscoveryCalls, 0);

    harness.model->ownerVanished(QStringLiteral(":1.11"));
    QVERIFY(harness.waitUntil(
        [&harness] { return !harness.snapshotAdapter().discovering; }));
    QCOMPARE(harness.fake->stopDiscoveryCalls, 1);
}

void BluezAdapterOperationsTests::ownerLossDuringDeferredConnectIsUncertain()
{
    BluezHarness harness(7108);
    QVERIFY(harness.ready());
    const QString adapterPath = harness.fake->addAdapter(
        QStringLiteral("hci0"), adapterAddress(), QStringLiteral("Internal"), true);
    const QString devicePath = harness.fake->addDevice(
        adapterPath, QStringLiteral("AA:BB:CC:33:44:55"), QStringLiteral("Keyboard"));
    FakeBluez::DeviceEntity *device = harness.fake->device(devicePath);
    device->paired = true;
    device->deferConnect = true;
    QVERIFY(harness.fake->takeOwnership());
    harness.model->start();
    QVERIFY(harness.waitReady());
    QSignalSpy completed(harness.model.get(), &BluetoothModel::operationCompleted);

    const OperationSubmission connect = harness.model->submit(
        {.kind = OperationKind::Connect,
         .target = harness.model->snapshot().devices.constFirst().handle},
        QStringLiteral(":1.10"));
    QVERIFY(connect.pending);
    QVERIFY(harness.waitUntil([&harness] {
        return harness.fake->connectCalls >= 1;
    }));

    // The reply never arrives because the authority that owed it is gone.
    harness.fake->dropOwnership();
    const std::optional<OperationResult> uncertain =
        harness.awaitResult(completed, connect.operationId);
    QVERIFY(uncertain.has_value());
    QCOMPARE(uncertain->status, OperationStatus::Uncertain);
    QCOMPARE(uncertain->reasonCode, QStringLiteral("authority-replaced"));
    QVERIFY(harness.waitUnavailable());

    // The late reply from the dead owner must not deliver a second result.
    harness.fake->replyDeferredConnects(QStringLiteral("org.bluez.Error.Failed"));
    QTest::qWait(150);
    int delivered = 0;
    for (const QList<QVariant> &arguments : completed) {
        if (arguments.value(0).toULongLong() == connect.operationId) {
            ++delivered;
        }
    }
    QCOMPARE(delivered, 1);
}

QTEST_GUILESS_MAIN(BluezAdapterOperationsTests)
#include "tst_bluez_adapter_operations.moc"
