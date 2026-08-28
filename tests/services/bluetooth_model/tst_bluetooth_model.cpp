// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_adapter_backend.h"

#include <qindaqt/services/bluetooth_model/bluetooth_model.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtTest>

#include <limits>

using namespace QindaQt::Bluetooth;
using namespace QindaQt::Tests;

namespace
{

constexpr quint64 kTestEpoch = 5001;

Handle adapterHandle(const Snapshot &snapshot)
{
    return snapshot.adapters.constFirst().handle;
}

Handle deviceHandle(const Snapshot &snapshot)
{
    return snapshot.devices.constFirst().handle;
}

} // namespace

class BluetoothModelTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesValidatedSnapshots();
    void emptyInventoryIsUnavailable();
    void appliesTypedOperationsWithInitiatingLineage();
    void rejectsStaleHandlesAndPolicyViolations();
    void rejectsMalformedAndMissingCallers();
    void boundsDiscoveryLeases();
    void releasesLeasesWhenOwnerVanishes();
    void stopMakesPendingUncertain();
    void restartAdvancesEpochAndInvalidatesHandles();
    void malformedBackendFailsClosed();
    void rejectsStoppedAndSupersededBackendValues();
    void malformedBackendOutcomesBecomeProtocolValidFailures();
};

void BluetoothModelTests::publishesValidatedSnapshots()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    QSignalSpy snapshots(&model, &BluetoothModel::snapshotChanged);
    QSignalSpy invalidations(&model, &BluetoothModel::invalidated);
    model.start();
    QCOMPARE(backend.startCalls, 1);

    BackendInventory inventory = bluetoothInventory();
    inventory.leases = {{.callerId = QStringLiteral(":1.7"),
                         .adapterAddress = QStringLiteral("AA:BB:CC:00:11:22"),
                         .refcount = 1}};
    inventory.adapters[0].discovering = true;
    backend.publish(inventory);

    QCOMPARE(snapshots.count(), 1);
    QCOMPARE(invalidations.count(), 1);
    const Snapshot snapshot = model.snapshot();
    QVERIFY(validateSnapshot(snapshot).accepted);
    QCOMPARE(snapshot.availability, Availability::Ready);
    QCOMPARE(snapshot.epoch, kTestEpoch);
    QCOMPARE(snapshot.revision, quint64(2));
    QCOMPARE(snapshot.adapters.size(), 1);
    QCOMPARE(snapshot.devices.size(), 1);
    QCOMPARE(snapshot.adapters[0].discovering, true);

    // Republishing identical content must not advance the revision.
    backend.publish(inventory);
    QCOMPARE(snapshots.count(), 1);

    model.stop();
    QCOMPARE(backend.stopCalls, 1);
}

void BluetoothModelTests::emptyInventoryIsUnavailable()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    model.start();
    backend.publish(BackendInventory{});
    const Snapshot snapshot = model.snapshot();
    QCOMPARE(snapshot.availability, Availability::Unavailable);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("no-adapter"));
    QCOMPARE(snapshot.capabilities, Capabilities{});
    QVERIFY(snapshot.adapters.isEmpty());
    QVERIFY(snapshot.devices.isEmpty());
    const OperationSubmission submission = model.submit(
        {.kind = OperationKind::Connect, .target = {kTestEpoch, 700}, .powered = false},
        QStringLiteral(":1.7"));
    QVERIFY(!submission.pending);
    QCOMPARE(submission.immediateResult.status, OperationStatus::Rejected);
    QCOMPARE(submission.immediateResult.reasonCode, QStringLiteral("unavailable"));
}

void BluetoothModelTests::appliesTypedOperationsWithInitiatingLineage()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    QSignalSpy completed(&model, &BluetoothModel::operationCompleted);
    model.start();
    backend.publish(bluetoothInventory());
    const Snapshot snapshot = model.snapshot();
    const quint64 revisionAtSubmit = snapshot.revision;

    const OperationRequest request{.kind = OperationKind::Connect,
                                   .target = deviceHandle(snapshot),
                                   .powered = false};
    const OperationSubmission submission =
        model.submit(request, QStringLiteral(":1.7"));
    QVERIFY(submission.pending);
    QCOMPARE(backend.operations.size(), 1);
    QCOMPARE(backend.operations[0].request.kind, OperationKind::Connect);
    QCOMPARE(backend.operations[0].request.deviceAddress,
             QStringLiteral("AA:BB:CC:33:44:55"));
    QCOMPARE(backend.operations[0].request.callerId, QStringLiteral(":1.7"));

    backend.publish(bluetoothInventory());
    backend.finish(submission.operationId,
                   {.status = BackendOperationStatus::Succeeded,
                    .reasonCode = QStringLiteral("connected"),
                    .diagnostic = {}});
    QCOMPARE(completed.count(), 1);
    const OperationResult result = completed.takeFirst()[1].value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Succeeded);
    QCOMPARE(result.kind, OperationKind::Connect);
    // AGENT-GUARD under test: the initiating lineage survives an intervening
    // snapshot publication; observed carries the newer revision.
    QCOMPARE(result.initiatingEpoch, kTestEpoch);
    QCOMPARE(result.initiatingRevision, revisionAtSubmit);
    QCOMPARE(result.observedEpoch, kTestEpoch);
    QVERIFY(result.observedRevision > revisionAtSubmit);
    QVERIFY(validateOperationResult(result).accepted);
}

void BluetoothModelTests::rejectsStaleHandlesAndPolicyViolations()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    model.start();
    backend.publish(bluetoothInventory());
    const Snapshot snapshot = model.snapshot();

    struct Case {
        OperationRequest request;
        QString reason;
    };
    const QList<Case> cases = {
        {{OperationKind::Connect, {kTestEpoch + 1, deviceHandle(snapshot).serial}, false},
         QStringLiteral("stale-handle")},
        {{OperationKind::Connect, {kTestEpoch, 424242}, false},
         QStringLiteral("stale-handle")},
        {{OperationKind::SetAdapterPower, {}, false}, QStringLiteral("stale-handle")},
        {{OperationKind::AcquireDiscovery, deviceHandle(snapshot), false},
         QStringLiteral("stale-handle")},
    };
    for (const Case &entry : cases) {
        const OperationSubmission submission =
            model.submit(entry.request, QStringLiteral(":1.7"));
        QVERIFY2(!submission.pending, qPrintable(entry.reason));
        QCOMPARE(submission.immediateResult.status, OperationStatus::Rejected);
        QCOMPARE(submission.immediateResult.reasonCode, entry.reason);
        QCOMPARE(submission.immediateResult.initiatingEpoch, kTestEpoch);
    }
    QVERIFY(backend.operations.isEmpty());
}

void BluetoothModelTests::rejectsMalformedAndMissingCallers()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    model.start();
    backend.publish(bluetoothInventory());
    const Snapshot snapshot = model.snapshot();

    for (const QString &caller : {QString{}, QStringLiteral("bad caller"),
                                  QString(kMaxCallerIdUtf8Bytes + 1, QLatin1Char('x'))}) {
        const OperationSubmission submission =
            model.submit({OperationKind::Connect, deviceHandle(snapshot), false}, caller);
        QVERIFY(!submission.pending);
        QCOMPARE(submission.immediateResult.reasonCode, QStringLiteral("malformed-caller"));
    }
    QVERIFY(backend.operations.isEmpty());
}

void BluetoothModelTests::boundsDiscoveryLeases()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    model.start();
    BackendInventory inventory = bluetoothInventory();
    BackendLease lease{.callerId = QStringLiteral(":1.7"),
                       .adapterAddress = QStringLiteral("AA:BB:CC:00:11:22"),
                       .refcount = 1};
    for (int index = 0; index < kMaxDiscoveryLeasesPerAdapter; ++index) {
        BackendLease entry = lease;
        entry.callerId = QStringLiteral(":1.%1").arg(index);
        inventory.leases.push_back(entry);
    }
    backend.publish(inventory);
    const Snapshot snapshot = model.snapshot();

    const OperationSubmission blocked = model.submit(
        {OperationKind::AcquireDiscovery, adapterHandle(snapshot), false},
        QStringLiteral(":1.99"));
    QVERIFY(!blocked.pending);
    QCOMPARE(blocked.immediateResult.reasonCode, QStringLiteral("too-many-leases"));

    inventory.leases.clear();
    backend.publish(inventory);
    const OperationSubmission allowed = model.submit(
        {OperationKind::AcquireDiscovery, adapterHandle(snapshot), false},
        QStringLiteral(":1.99"));
    QVERIFY(allowed.pending);
    QCOMPARE(backend.operations.size(), 1);
    QCOMPARE(backend.operations[0].request.kind, OperationKind::AcquireDiscovery);

    // Discovery on an unpowered adapter fails closed.
    BackendInventory unpowered = bluetoothInventory();
    unpowered.adapters[0].powered = false;
    unpowered.devices.clear();
    backend.publish(unpowered);
    const Snapshot offSnapshot = model.snapshot();
    const OperationSubmission off = model.submit(
        {OperationKind::AcquireDiscovery, adapterHandle(offSnapshot), false},
        QStringLiteral(":1.7"));
    QVERIFY(!off.pending);
    QCOMPARE(off.immediateResult.reasonCode, QStringLiteral("adapter-off"));
}

void BluetoothModelTests::releasesLeasesWhenOwnerVanishes()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    model.start();
    model.ownerVanished(QStringLiteral(":1.7"));
    QCOMPARE(backend.releasedOwners.size(), 1);
    QCOMPARE(backend.releasedOwners.first(), QStringLiteral(":1.7"));

    // Malformed owner identities never reach the backend.
    model.ownerVanished(QString{});
    model.ownerVanished(QStringLiteral("bad caller"));
    QCOMPARE(backend.releasedOwners.size(), 1);
}

void BluetoothModelTests::stopMakesPendingUncertain()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    QSignalSpy completed(&model, &BluetoothModel::operationCompleted);
    model.start();
    backend.publish(bluetoothInventory());
    const Snapshot snapshot = model.snapshot();

    const OperationSubmission submission =
        model.submit({OperationKind::Disconnect, deviceHandle(snapshot), false},
                     QStringLiteral(":1.7"));
    QVERIFY(submission.pending);
    model.stop();
    QCOMPARE(completed.count(), 1);
    const OperationResult result = completed.takeFirst()[1].value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("model-stopped"));
    QCOMPARE(result.initiatingEpoch, kTestEpoch);
    QVERIFY(validateOperationResult(result).accepted);

    // A late backend completion for the retired operation is dropped.
    backend.finishForGeneration(backend.generation, submission.operationId,
                                {.status = BackendOperationStatus::Succeeded,
                                 .reasonCode = QStringLiteral("disconnected"),
                                 .diagnostic = {}});
    QCOMPARE(completed.count(), 0);
}

void BluetoothModelTests::restartAdvancesEpochAndInvalidatesHandles()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    model.start();
    backend.publish(bluetoothInventory());
    const quint64 firstEpoch = model.snapshot().epoch;

    model.stop();
    model.start();
    backend.publish(bluetoothInventory());

    const Snapshot snapshot = model.snapshot();
    QVERIFY(snapshot.epoch > firstEpoch);
    QVERIFY(validateSnapshot(snapshot).accepted);
    // A handle from the first epoch is stale after the restart.
    const OperationSubmission stale = model.submit(
        {OperationKind::Connect, {firstEpoch, deviceHandle(snapshot).serial}, false},
        QStringLiteral(":1.7"));
    QVERIFY(!stale.pending);
    QCOMPARE(stale.immediateResult.reasonCode, QStringLiteral("stale-handle"));

    // A second restart must again produce a strictly greater epoch.
    const quint64 secondEpoch = snapshot.epoch;
    model.stop();
    model.start();
    backend.publish(bluetoothInventory());
    QVERIFY(model.snapshot().epoch > secondEpoch);
}

void BluetoothModelTests::malformedBackendFailsClosed()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    QSignalSpy snapshots(&model, &BluetoothModel::snapshotChanged);
    model.start();
    backend.publish(bluetoothInventory());

    BackendInventory malformed = bluetoothInventory();
    malformed.adapters[0].address = QStringLiteral("not-an-address");
    backend.publish(malformed);

    const Snapshot snapshot = model.snapshot();
    QCOMPARE(snapshot.availability, Availability::Degraded);
    QCOMPARE(snapshot.reasonCode, QStringLiteral("backend-malformed"));
    QVERIFY(snapshot.adapters.isEmpty());
    QVERIFY(snapshot.devices.isEmpty());
    QVERIFY(validateSnapshot(snapshot).accepted);
    QCOMPARE(snapshots.count(), 2);

    // A malformed lease table also fails closed.
    BackendInventory badLeases = bluetoothInventory();
    badLeases.leases = {{.callerId = QString{},
                         .adapterAddress = QStringLiteral("AA:BB:CC:00:11:22"),
                         .refcount = 1}};
    backend.publish(badLeases);
    QCOMPARE(model.snapshot().reasonCode, QStringLiteral("backend-malformed"));
}

void BluetoothModelTests::rejectsStoppedAndSupersededBackendValues()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    model.start();
    const quint64 firstGeneration = backend.generation;
    model.stop();
    // Stopped-run values are dropped even when well formed.
    backend.publishForGeneration(firstGeneration, bluetoothInventory());
    QCOMPARE(model.snapshot().availability, Availability::Starting);

    model.start();
    backend.publish(bluetoothInventory());
    QCOMPARE(model.snapshot().availability, Availability::Ready);
    // Superseded-run values are dropped after a fresh start.
    backend.publishForGeneration(firstGeneration, BackendInventory{});
    QCOMPARE(model.snapshot().availability, Availability::Ready);
}

void BluetoothModelTests::malformedBackendOutcomesBecomeProtocolValidFailures()
{
    FakeAdapterBackend backend;
    BluetoothModel model(&backend, kTestEpoch);
    QSignalSpy completed(&model, &BluetoothModel::operationCompleted);
    model.start();
    backend.publish(bluetoothInventory());
    const Snapshot snapshot = model.snapshot();

    const OperationSubmission submission =
        model.submit({OperationKind::Connect, deviceHandle(snapshot), false},
                     QStringLiteral(":1.7"));
    QVERIFY(submission.pending);

    backend.finish(submission.operationId,
                   {.status = BackendOperationStatus::Succeeded,
                    .reasonCode = QStringLiteral("Not A Token"),
                    .diagnostic = {}});
    QCOMPARE(completed.count(), 1);
    OperationResult result = completed.takeFirst()[1].value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Failed);
    QCOMPARE(result.reasonCode, QStringLiteral("backend-malformed"));
    QVERIFY(validateOperationResult(result).accepted);

    const OperationSubmission second = model.submit(
        {OperationKind::Connect, deviceHandle(model.snapshot()), false},
        QStringLiteral(":1.7"));
    QVERIFY(second.pending);
    backend.finish(second.operationId,
                   {.status = static_cast<BackendOperationStatus>(99),
                    .reasonCode = QStringLiteral("connected"),
                    .diagnostic = {}});
    QCOMPARE(completed.count(), 1);
    result = completed.takeFirst()[1].value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Failed);
    QCOMPARE(result.reasonCode, QStringLiteral("backend-malformed"));
}

QTEST_GUILESS_MAIN(BluetoothModelTests)
#include "tst_bluetooth_model.moc"
