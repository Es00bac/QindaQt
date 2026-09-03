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
    const quint64 observedRevision = 6,
    const quint64 initiatingRevision = 5)
{
    return {.kind = submission.request.kind,
            .status = status,
            .initiatingEpoch = 61,
            .initiatingRevision = initiatingRevision,
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
    void failedCloseReleaseIsNotAutomaticallyReplayed_data();
    void failedCloseReleaseIsNotAutomaticallyReplayed();
    void malformedReleaseNoLeaseRetainsLease();
    void successWaitsForSnapshotConvergence();
    void ownerReplacementClearsTruthLeaseAndRequestWithoutReplay();
    void pairingConfirmationRoundTrip_data();
    void pairingConfirmationRoundTrip();
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
    QTRY_VERIFY(controller.discoveryLeaseHeld());
    QTRY_COMPARE(transport.fetches.size(), 2);
    Bluetooth::Snapshot acquired = bluetoothClientSnapshot(61, 6);
    acquired.adapters[0].discovering = true;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, acquired);
    QTRY_COMPARE(transport.submissions.size(), 2);
    const auto release = transport.submissions.constLast();
    QCOMPARE(release.request.kind, Bluetooth::OperationKind::ReleaseDiscovery);
    QCOMPARE(release.request.target, acquire.request.target);
    QVERIFY(controller.discoveryLeaseHeld());

    transport.emitOperationReply(
        kOwner, release.requestId, true,
        resultFor(release, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-released"), 7, 6));
    QTRY_VERIFY(!controller.discoveryLeaseHeld());
    QTRY_COMPARE(transport.fetches.size(), 3);
    Bluetooth::Snapshot released = bluetoothClientSnapshot(61, 7);
    released.adapters[0].discovering = false;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, released);
    QTRY_VERIFY(!controller.operationPending());
}

void BluetoothAppletControllerTests::failedCloseReleaseIsNotAutomaticallyReplayed_data()
{
    QTest::addColumn<Bluetooth::OperationStatus>("status");
    QTest::addColumn<QString>("reason");
    QTest::addColumn<QString>("feedbackFragment");
    QTest::addColumn<bool>("authoritativeEnd");

    QTest::newRow("failed-requires-explicit-retry")
        << Bluetooth::OperationStatus::Failed
        << QStringLiteral("release-failed") << QStringLiteral("failed") << false;
    QTest::newRow("uncertain-waits-for-truth")
        << Bluetooth::OperationStatus::Uncertain
        << QStringLiteral("state-unknown") << QStringLiteral("uncertain") << true;
}

void BluetoothAppletControllerTests::failedCloseReleaseIsNotAutomaticallyReplayed()
{
    QFETCH(Bluetooth::OperationStatus, status);
    QFETCH(QString, reason);
    QFETCH(QString, feedbackFragment);
    QFETCH(bool, authoritativeEnd);

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
    QTRY_COMPARE(transport.fetches.size(), 2);
    Bluetooth::Snapshot acquired = bluetoothClientSnapshot(61, 6);
    acquired.adapters[0].discovering = true;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, acquired);
    QTRY_VERIFY(!controller.operationPending());

    controller.setExpanded(false);
    QCOMPARE(transport.submissions.size(), 2);
    const auto release = transport.submissions.constLast();
    QCOMPARE(release.request.kind, Bluetooth::OperationKind::ReleaseDiscovery);
    transport.emitOperationReply(
        kOwner, release.requestId, true,
        resultFor(release, status, reason, 6, 6));

    QTRY_VERIFY(controller.feedbackPresent());
    QVERIFY(!controller.operationPending());
    QVERIFY(controller.discoveryLeaseHeld());
    QVERIFY(controller.feedback().contains(feedbackFragment));
    QCOMPARE(transport.submissions.size(), 2);
    QTRY_COMPARE(transport.fetches.size(), 3);

    Bluetooth::Snapshot observed = bluetoothClientSnapshot(61, 7);
    observed.adapters[0].discovering = !authoritativeEnd;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, observed);
    QTRY_COMPARE(controller.serviceRevision(), quint64(7));
    QCOMPARE(transport.submissions.size(), 2);
    QVERIFY(controller.feedback().contains(feedbackFragment));

    if (authoritativeEnd) {
        QVERIFY(!controller.discoveryLeaseHeld());
        return;
    }

    QVERIFY(controller.discoveryLeaseHeld());
    controller.setExpanded(true);
    controller.setExpanded(false);
    QCOMPARE(transport.submissions.size(), 3);
    const auto retry = transport.submissions.constLast();
    transport.emitOperationReply(
        kOwner, retry.requestId, true,
        resultFor(retry, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-released"), 8, 7));
    QTRY_VERIFY(!controller.discoveryLeaseHeld());
    QTRY_COMPARE(transport.fetches.size(), 4);
    Bluetooth::Snapshot released = bluetoothClientSnapshot(61, 8);
    released.adapters[0].discovering = false;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, released);
    QTRY_VERIFY(!controller.operationPending());
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
    QTRY_COMPARE(transport.fetches.size(), 2);
    Bluetooth::Snapshot acquired = bluetoothClientSnapshot(61, 6);
    acquired.adapters[0].discovering = true;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, acquired);
    QTRY_VERIFY(!controller.operationPending());

    QVERIFY(controller.requestDiscovery(QStringLiteral("adapter-61-400"), false));
    QCOMPARE(transport.submissions.size(), 2);
    const auto release = transport.submissions.constLast();
    Bluetooth::OperationResult malformed = resultFor(
        release, Bluetooth::OperationStatus::Failed, QStringLiteral("no-lease"),
        6, 6);
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
                  QStringLiteral("lease-released"), 7, 6));
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
                  QStringLiteral("policy-rejected"), 6, 6));
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
    QTRY_COMPARE(transport.fetches.size(), 2);
    Bluetooth::Snapshot acquired = bluetoothClientSnapshot(61, 6);
    acquired.adapters[0].discovering = true;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, acquired);
    QTRY_VERIFY(!controller.operationPending());

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

void BluetoothAppletControllerTests::pairingConfirmationRoundTrip_data()
{
    QTest::addColumn<bool>("accepted");
    QTest::newRow("confirm") << true;
    QTest::newRow("cancel") << false;
}

void BluetoothAppletControllerTests::pairingConfirmationRoundTrip()
{
    QFETCH(bool, accepted);
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    Bluetooth::Snapshot prompt = bluetoothClientSnapshot();
    prompt.pairingPrompt = {
        .promptId = 103,
        .kind = Bluetooth::PairingPromptKind::ConfirmPasskey,
        .device = prompt.devices.constFirst().handle,
        .detail = QStringLiteral("123456"),
        .serviceUuid = {},
        .entered = 0,
    };
    publishReady(client, transport, prompt);

    QVERIFY(controller.pairingPromptVisible());
    QVERIFY(controller.pairingConfirmationAvailable());
    QVERIFY(controller.pairingPromptText().contains(QStringLiteral("123456")));
    QVERIFY(accepted ? controller.confirmPrompt() : controller.cancelPrompt());
    QVERIFY(controller.pairingReplyPending());
    QCOMPARE(transport.submissions.size(), 1);
    const auto response = transport.submissions.constFirst();
    QCOMPARE(response.request.kind, Bluetooth::OperationKind::ReplyConfirmation);
    QCOMPARE(response.request.accepted, accepted);
    transport.emitOperationReply(
        kOwner, response.requestId, true,
        resultFor(response, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("prompt-replied")));
    QTRY_VERIFY(!controller.pairingReplyPending());
    QTRY_COMPARE(transport.fetches.size(), 2);
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, bluetoothClientSnapshot(61, 6));
    QTRY_VERIFY(!controller.pairingPromptVisible());
}

QTEST_GUILESS_MAIN(BluetoothAppletControllerTests)
#include "tst_bluetooth_applet_controller.moc"
