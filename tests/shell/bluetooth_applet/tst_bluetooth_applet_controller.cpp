// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_applet_controller.h"

#include "support/fake_bluetooth_transport.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell::BluetoothApplet;
using namespace QindaQt::Tests;

namespace
{

const QString kOwner = QStringLiteral(":1.42");

void publishReady(Bluetooth::BluetoothClient &client,
                  FakeBluetoothTransport &transport,
                  const Bluetooth::Snapshot &snapshot = bluetoothClientSnapshot())
{
    client.start();
    transport.setOwner(kOwner);
    QVERIFY(!transport.fetches.isEmpty());
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, snapshot);
    QCOMPARE(client.state(), Bluetooth::ClientState::Ready);
}

Bluetooth::OperationResult resultFor(
    const FakeBluetoothTransport::RecordedSubmission &submission,
    const Bluetooth::OperationStatus status,
    const QString &reason,
    const quint64 observedRevision = 6)
{
    return {.kind = submission.request.kind,
            .status = status,
            .initiatingEpoch = 61,
            .initiatingRevision = 5,
            .observedEpoch = 61,
            .observedRevision = observedRevision,
            .reasonCode = reason,
            .diagnostic = {},
            .wireValid = true};
}

} // namespace

class BluetoothAppletControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectsOnlyOpaqueBoundedRows();
    void grantsGateReadAndControlIndependently();
    void serializesAndPinsPublicOperations();
    void closeReleasesDiscoveryAfterPendingAcquire();
    void malformedReleaseNoLeaseRetainsLease();
    void successWaitsForSnapshotConvergence();
    void ownerReplacementClearsTruthLeaseAndRequestWithoutReplay();
};

void BluetoothAppletControllerTests::projectsOnlyOpaqueBoundedRows()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    publishReady(client, transport);

    QCOMPARE(controller.phase(), QStringLiteral("ready"));
    QCOMPARE(controller.serviceEpoch(), quint64(61));
    QCOMPARE(controller.serviceRevision(), quint64(5));
    QCOMPARE(controller.adapterRows().size(), 1);
    QCOMPARE(controller.deviceRows().size(), 1);
    const QVariantMap adapter = controller.adapterRows().constFirst().toMap();
    const QVariantMap device = controller.deviceRows().constFirst().toMap();
    QCOMPARE(adapter.value(QStringLiteral("id")).toString(),
             QStringLiteral("adapter-61-400"));
    QCOMPARE(device.value(QStringLiteral("id")).toString(),
             QStringLiteral("device-61-700"));
    QVERIFY(!adapter.value(QStringLiteral("accessibleName")).toString().isEmpty());
    QVERIFY(!adapter.value(QStringLiteral("accessibleDescription")).toString().isEmpty());
    QVERIFY(!device.value(QStringLiteral("accessibleName")).toString().isEmpty());
    QVERIFY(!device.value(QStringLiteral("accessibleDescription")).toString().isEmpty());
    QVERIFY(!adapter.values().contains(QStringLiteral("AA:BB:CC:00:11:22")));
    QVERIFY(!device.values().contains(QStringLiteral("AA:BB:CC:33:44:55")));
}

void BluetoothAppletControllerTests::grantsGateReadAndControlIndependently()
{
    FakeBluetoothTransport deniedTransport;
    Bluetooth::BluetoothClient deniedClient(&deniedTransport);
    BluetoothAppletController denied(&deniedClient, false, true);
    publishReady(deniedClient, deniedTransport);
    QCOMPARE(denied.phase(), QStringLiteral("unavailable"));
    QVERIFY(denied.adapterRows().isEmpty());
    QVERIFY(!denied.requestAdapterPower(QStringLiteral("adapter-61-400"), false));
    QVERIFY(deniedTransport.submissions.isEmpty());

    FakeBluetoothTransport readOnlyTransport;
    Bluetooth::BluetoothClient readOnlyClient(&readOnlyTransport);
    BluetoothAppletController readOnly(&readOnlyClient, true, false);
    publishReady(readOnlyClient, readOnlyTransport);
    const QVariantMap adapter = readOnly.adapterRows().constFirst().toMap();
    const QVariantMap device = readOnly.deviceRows().constFirst().toMap();
    QVERIFY(!adapter.value(QStringLiteral("canSetPowered")).toBool());
    QVERIFY(!adapter.value(QStringLiteral("canAcquireDiscovery")).toBool());
    QVERIFY(!device.value(QStringLiteral("canDisconnect")).toBool());
    QVERIFY(!readOnly.requestDiscovery(QStringLiteral("adapter-61-400"), true));
    QVERIFY(readOnlyTransport.submissions.isEmpty());
}

void BluetoothAppletControllerTests::serializesAndPinsPublicOperations()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    publishReady(client, transport);

    QVERIFY(controller.requestDeviceConnection(QStringLiteral("device-61-700"),
                                                false));
    QCOMPARE(transport.submissions.size(), 1);
    const auto submission = transport.submissions.constFirst();
    QCOMPARE(submission.owner, kOwner);
    QCOMPARE(submission.request.kind, Bluetooth::OperationKind::Disconnect);
    QCOMPARE(submission.request.target,
             (Bluetooth::Handle{.epoch = 61, .serial = 700}));
    QVERIFY(controller.operationPending());

    QVERIFY(!controller.requestAdapterPower(QStringLiteral("adapter-61-400"),
                                             false));
    QCOMPARE(transport.submissions.size(), 1);
    transport.emitOperationReply(
        kOwner, submission.requestId, true,
        resultFor(submission, Bluetooth::OperationStatus::Rejected,
                  QStringLiteral("policy-rejected")));
    QTRY_VERIFY(!controller.operationPending());
    QVERIFY(controller.feedbackPresent());
    QVERIFY(controller.feedback().contains(QStringLiteral("rejected")));
    QCOMPARE(transport.submissions.size(), 1);
}

void BluetoothAppletControllerTests::closeReleasesDiscoveryAfterPendingAcquire()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    publishReady(client, transport);
    controller.setExpanded(true);

    QVERIFY(controller.requestDiscovery(QStringLiteral("adapter-61-400"), true));
    QCOMPARE(transport.submissions.size(), 1);
    const auto acquire = transport.submissions.constFirst();
    QCOMPARE(acquire.request.kind, Bluetooth::OperationKind::AcquireDiscovery);
    controller.setExpanded(false);
    QCOMPARE(transport.submissions.size(), 1);

    transport.emitOperationReply(
        kOwner, acquire.requestId, true,
        resultFor(acquire, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-acquired")));
    QTRY_COMPARE(transport.submissions.size(), 2);
    const auto release = transport.submissions.constLast();
    QCOMPARE(release.request.kind, Bluetooth::OperationKind::ReleaseDiscovery);
    QCOMPARE(release.request.target, acquire.request.target);
    QVERIFY(controller.discoveryLeaseHeld());

    transport.emitOperationReply(
        kOwner, release.requestId, true,
        resultFor(release, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-released"), 7));
    QTRY_VERIFY(!controller.operationPending());
    QVERIFY(!controller.discoveryLeaseHeld());
}

void BluetoothAppletControllerTests::malformedReleaseNoLeaseRetainsLease()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    publishReady(client, transport);
    controller.setExpanded(true);

    QVERIFY(controller.requestDiscovery(QStringLiteral("adapter-61-400"), true));
    const auto acquire = transport.submissions.constFirst();
    transport.emitOperationReply(
        kOwner, acquire.requestId, true,
        resultFor(acquire, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-acquired")));
    QTRY_VERIFY(controller.discoveryLeaseHeld());

    QVERIFY(controller.requestDiscovery(QStringLiteral("adapter-61-400"), false));
    QCOMPARE(transport.submissions.size(), 2);
    const auto release = transport.submissions.constLast();
    Bluetooth::OperationResult malformed = resultFor(
        release, Bluetooth::OperationStatus::Failed, QStringLiteral("no-lease"));
    malformed.wireValid = false;

    // Exercise the controller's final admission boundary directly: an invalid
    // collaborator result must not acquire lease-lifetime authority merely by
    // carrying a plausible reason string.
    Q_EMIT client.operationCompleted(release.requestId, malformed);

    QTRY_VERIFY(!controller.operationPending());
    QVERIFY(controller.discoveryLeaseHeld());
    QVERIFY(controller.feedback().contains(QStringLiteral("unreadable")));

    // Retire the client's matching internal request without changing the
    // controller result; the controller has already consumed this request ID.
    transport.emitOperationReply(
        kOwner, release.requestId, true,
        resultFor(release, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-released"), 7));
}

void BluetoothAppletControllerTests::successWaitsForSnapshotConvergence()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    publishReady(client, transport);

    QVERIFY(controller.requestDeviceConnection(QStringLiteral("device-61-700"),
                                                false));
    QCOMPARE(transport.submissions.size(), 1);
    const auto disconnect = transport.submissions.constFirst();
    transport.emitOperationReply(
        kOwner, disconnect.requestId, true,
        resultFor(disconnect, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("disconnected"), 6));

    QTRY_VERIFY(controller.operationPending());
    QTRY_COMPARE(transport.fetches.size(), 2);
    QVariantMap staleDevice = controller.deviceRows().constFirst().toMap();
    QVERIFY(!staleDevice.value(QStringLiteral("canDisconnect")).toBool());
    QVERIFY(!controller.requestDeviceConnection(QStringLiteral("device-61-700"),
                                                 false));
    QCOMPARE(transport.submissions.size(), 1);

    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, bluetoothClientSnapshot(61, 5));
    QVERIFY(controller.operationPending());
    QCOMPARE(transport.submissions.size(), 1);

    transport.emitInvalidated(kOwner, 61, 6);
    QCOMPARE(transport.fetches.size(), 3);
    Bluetooth::Snapshot converged = bluetoothClientSnapshot(61, 6);
    converged.devices[0].connected = false;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, converged);

    QTRY_VERIFY(!controller.operationPending());
    QCOMPARE(controller.serviceRevision(), quint64(6));
    const QVariantMap currentDevice = controller.deviceRows().constFirst().toMap();
    QVERIFY(currentDevice.value(QStringLiteral("canConnect")).toBool());
    QVERIFY(!currentDevice.value(QStringLiteral("canDisconnect")).toBool());

    QVERIFY(controller.requestDeviceConnection(QStringLiteral("device-61-700"),
                                                true));
    QCOMPARE(transport.submissions.size(), 2);
    const auto connect = transport.submissions.constLast();
    transport.emitOperationReply(
        kOwner, connect.requestId, true,
        resultFor(connect, Bluetooth::OperationStatus::Rejected,
                  QStringLiteral("policy-rejected"), 6));
    QTRY_VERIFY(!controller.operationPending());
}

void BluetoothAppletControllerTests::ownerReplacementClearsTruthLeaseAndRequestWithoutReplay()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    publishReady(client, transport);
    controller.setExpanded(true);

    QVERIFY(controller.requestDiscovery(QStringLiteral("adapter-61-400"), true));
    const auto acquire = transport.submissions.constFirst();
    transport.emitOperationReply(
        kOwner, acquire.requestId, true,
        resultFor(acquire, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-acquired")));
    QTRY_VERIFY(controller.discoveryLeaseHeld());

    QVERIFY(controller.requestDeviceConnection(QStringLiteral("device-61-700"),
                                                false));
    QCOMPARE(transport.submissions.size(), 2);
    transport.setOwner(QStringLiteral(":1.99"));
    QVERIFY(!controller.operationPending());
    QVERIFY(!controller.discoveryLeaseHeld());
    QCOMPARE(controller.phase(), QStringLiteral("loading"));
    QVERIFY(controller.adapterRows().isEmpty());
    QVERIFY(controller.deviceRows().isEmpty());
    QVERIFY(controller.feedback().contains(QStringLiteral("authority changed")));

    transport.emitSnapshotReply(QStringLiteral(":1.99"),
                                transport.fetches.constLast().requestId,
                                true, bluetoothClientSnapshot(62, 1));
    QCOMPARE(controller.phase(), QStringLiteral("ready"));
    QCoreApplication::processEvents();
    QCOMPARE(transport.submissions.size(), 2);
}

QTEST_GUILESS_MAIN(BluetoothAppletControllerTests)
#include "tst_bluetooth_applet_controller.moc"
