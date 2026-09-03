// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/bluetooth_applet/bluetooth_request_state.h>

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell::BluetoothApplet;

namespace
{

Bluetooth::Snapshot readySnapshot()
{
    Bluetooth::Snapshot snapshot;
    snapshot.schemaVersion = Bluetooth::kSchemaVersion;
    snapshot.epoch = 71;
    snapshot.revision = 12;
    snapshot.availability = Bluetooth::Availability::Ready;
    snapshot.capabilities = Bluetooth::Capability::SetAdapterPower
        | Bluetooth::Capability::DiscoveryLease
        | Bluetooth::Capability::ConnectPaired
        | Bluetooth::Capability::DisconnectPaired;
    snapshot.reasonCode = QStringLiteral("ready");
    snapshot.adapters = {{.handle = {.epoch = 71, .serial = 4},
                          .address = QStringLiteral("AA:BB:CC:00:11:22"),
                          .name = QStringLiteral("Radio"),
                          .powered = true}};
    snapshot.devices = {
        {.handle = {.epoch = 71, .serial = 8},
         .adapterHandle = {.epoch = 71, .serial = 4},
         .address = QStringLiteral("AA:BB:CC:33:44:55"),
         .name = QStringLiteral("Connected keyboard"),
         .deviceClass = Bluetooth::DeviceClass::Keyboard,
         .paired = true,
         .connected = true},
        {.handle = {.epoch = 71, .serial = 9},
         .adapterHandle = {.epoch = 71, .serial = 4},
         .address = QStringLiteral("AA:BB:CC:33:44:66"),
         .name = QStringLiteral("Headphones"),
         .deviceClass = Bluetooth::DeviceClass::Headphones,
         .paired = true,
         .connected = false},
    };
    return snapshot;
}

Bluetooth::OperationResult resultFor(const RequestState &request,
                                     const Bluetooth::OperationStatus status,
                                     const QString &reason)
{
    return {.kind = request.operation.kind,
            .status = status,
            .initiatingEpoch = request.initiatingEpoch,
            .initiatingRevision = request.initiatingRevision,
            .observedEpoch = request.initiatingEpoch,
            .observedRevision = request.initiatingRevision + 1,
            .reasonCode = reason,
            .diagnostic = {},
            .wireValid = true};
}

} // namespace

class BluetoothRequestStateTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void admitsOnlyCurrentCapableOperations();
    void fencesCompletionByKindAndLineage();
    void classifiesFailureAndUncertaintyWithoutReplay();
    void ownerOrEpochReplacementEndsPending();
};

void BluetoothRequestStateTests::admitsOnlyCurrentCapableOperations()
{
    const Bluetooth::Snapshot snapshot = readySnapshot();
    const Bluetooth::Handle adapter{.epoch = 71, .serial = 4};
    const Bluetooth::Handle connected{.epoch = 71, .serial = 8};
    const Bluetooth::Handle disconnected{.epoch = 71, .serial = 9};

    QVERIFY(beginBluetoothRequest(snapshot,
                                  {.kind = Bluetooth::OperationKind::SetAdapterPower,
                                   .target = adapter,
                                   .powered = false}, true).pending());
    QVERIFY(beginBluetoothRequest(snapshot,
                                  {.kind = Bluetooth::OperationKind::AcquireDiscovery,
                                   .target = adapter}, true).pending());
    QVERIFY(beginBluetoothRequest(snapshot,
                                  {.kind = Bluetooth::OperationKind::Connect,
                                   .target = disconnected}, true).pending());
    QVERIFY(beginBluetoothRequest(snapshot,
                                  {.kind = Bluetooth::OperationKind::Disconnect,
                                   .target = connected}, true).pending());

    const RequestState denied = beginBluetoothRequest(
        snapshot, {.kind = Bluetooth::OperationKind::Connect,
                   .target = disconnected}, false);
    QCOMPARE(denied.phase, RequestPhase::Failed);
    QVERIFY(denied.feedback.contains(QStringLiteral("not allowed")));

    const RequestState duplicate = beginBluetoothRequest(
        snapshot, {.kind = Bluetooth::OperationKind::Connect,
                   .target = connected}, true);
    QCOMPARE(duplicate.phase, RequestPhase::Failed);

    const RequestState stale = beginBluetoothRequest(
        snapshot, {.kind = Bluetooth::OperationKind::Disconnect,
                   .target = {.epoch = 70, .serial = 8}}, true);
    QCOMPARE(stale.phase, RequestPhase::Failed);

    const RequestState release = beginBluetoothRequest(
        snapshot, {.kind = Bluetooth::OperationKind::ReleaseDiscovery,
                   .target = adapter}, true, adapter);
    QVERIFY(release.pending());
    const RequestState noLease = beginBluetoothRequest(
        snapshot, {.kind = Bluetooth::OperationKind::ReleaseDiscovery,
                   .target = adapter}, true);
    QCOMPARE(noLease.phase, RequestPhase::Failed);

    Bluetooth::Snapshot missingAdapter = snapshot;
    missingAdapter.adapters.clear();
    missingAdapter.devices.clear();
    const RequestState vanishedAdapter = beginBluetoothRequest(
        missingAdapter, {.kind = Bluetooth::OperationKind::ReleaseDiscovery,
                         .target = adapter}, true, adapter);
    QCOMPARE(vanishedAdapter.phase, RequestPhase::Failed);
}

void BluetoothRequestStateTests::fencesCompletionByKindAndLineage()
{
    const RequestState request = beginBluetoothRequest(
        readySnapshot(), {.kind = Bluetooth::OperationKind::Connect,
                          .target = {.epoch = 71, .serial = 9}}, true);
    QVERIFY(request.pending());

    const RequestState succeeded = applyBluetoothResult(
        request, resultFor(request, Bluetooth::OperationStatus::Succeeded,
                           QStringLiteral("connected")));
    QCOMPARE(succeeded.phase, RequestPhase::Succeeded);

    Bluetooth::OperationResult wrongKind = resultFor(
        request, Bluetooth::OperationStatus::Succeeded,
        QStringLiteral("connected"));
    wrongKind.kind = Bluetooth::OperationKind::Disconnect;
    QCOMPARE(applyBluetoothResult(request, wrongKind).phase,
             RequestPhase::Uncertain);

    Bluetooth::OperationResult wrongRevision = resultFor(
        request, Bluetooth::OperationStatus::Succeeded,
        QStringLiteral("connected"));
    wrongRevision.initiatingRevision += 1;
    QCOMPARE(applyBluetoothResult(request, wrongRevision).phase,
             RequestPhase::Uncertain);
}

void BluetoothRequestStateTests::classifiesFailureAndUncertaintyWithoutReplay()
{
    const RequestState request = beginBluetoothRequest(
        readySnapshot(), {.kind = Bluetooth::OperationKind::Disconnect,
                          .target = {.epoch = 71, .serial = 8}}, true);
    const RequestState rejected = applyBluetoothResult(
        request, resultFor(request, Bluetooth::OperationStatus::Rejected,
                           QStringLiteral("policy-rejected")));
    QCOMPARE(rejected.phase, RequestPhase::Failed);
    QVERIFY(rejected.feedback.contains(QStringLiteral("rejected")));
    QCOMPARE(applyBluetoothResult(rejected,
                                  resultFor(request,
                                            Bluetooth::OperationStatus::Succeeded,
                                            QStringLiteral("disconnected"))),
             rejected);

    const RequestState uncertain = applyBluetoothResult(
        request, resultFor(request, Bluetooth::OperationStatus::Uncertain,
                           QStringLiteral("operation-timeout")));
    QCOMPARE(uncertain.phase, RequestPhase::Uncertain);
    QVERIFY(uncertain.feedback.contains(QStringLiteral("uncertain")));
}

void BluetoothRequestStateTests::ownerOrEpochReplacementEndsPending()
{
    const RequestState request = beginBluetoothRequest(
        readySnapshot(), {.kind = Bluetooth::OperationKind::AcquireDiscovery,
                          .target = {.epoch = 71, .serial = 4}}, true);
    QCOMPARE(observeBluetoothAuthority(request, true, 71), request);
    const RequestState noOwner = observeBluetoothAuthority(request, false, 0);
    QCOMPARE(noOwner.phase, RequestPhase::Uncertain);
    QVERIFY(noOwner.feedback.contains(QStringLiteral("authority changed")));
    QCOMPARE(observeBluetoothAuthority(request, true, 72).phase,
             RequestPhase::Uncertain);
}

QTEST_GUILESS_MAIN(BluetoothRequestStateTests)
#include "tst_bluetooth_request_state.moc"
