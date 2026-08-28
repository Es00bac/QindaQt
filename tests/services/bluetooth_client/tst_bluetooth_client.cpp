// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_bluetooth_transport.h"

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QtTest>

using namespace QindaQt::Bluetooth;
using namespace QindaQt::Tests;

namespace
{

QString kOwner()
{
    return QStringLiteral(":1.42");
}

} // namespace

class BluetoothClientTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void discoversOwnerAndPublishesValidatedSnapshot();
    void rejectsMalformedAndContradictorySnapshots();
    void coalescesInvalidationsWhileFetching();
    void ownerReplacementClearsSnapshotAndMarksUncertain();
    void timeoutMakesOperationUncertainAndRefetches();
    void rejectsStaleAndMalformedOperationReplies();
    void rejectsRequestsThatFailPreflight();
    void queuesExactlyOnceCompletion();
    void fetchFailureRevokesMutationAuthority();
    void stopCancelsAndMarksTransportBackedUncertain();

private:
    static void deliverReady(FakeBluetoothTransport &transport, const quint64 requestId,
                             const Snapshot &snapshot)
    {
        transport.emitSnapshotReply(kOwner(), requestId, true, snapshot);
    }
};

void BluetoothClientTests::discoversOwnerAndPublishesValidatedSnapshot()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    QSignalSpy snapshots(&client, &BluetoothClient::snapshotChanged);
    client.start();
    QCOMPARE(client.state(), ClientState::Starting);
    QCOMPARE(transport.startCalls, 1);

    transport.setOwner(kOwner());
    QCOMPARE(client.state(), ClientState::Starting);
    QCOMPARE(transport.fetches.size(), 1);
    deliverReady(transport, transport.fetches.constFirst().requestId,
                 bluetoothClientSnapshot());

    QCOMPARE(client.state(), ClientState::Ready);
    QVERIFY(client.hasSnapshot());
    QCOMPARE(snapshots.count(), 1);
    QVERIFY(client.owner() == kOwner());
    const Snapshot published = client.snapshot();
    QCOMPARE(published.epoch, quint64(61));
    QVERIFY(validateSnapshot(published).accepted);

    client.stop();
    QCOMPARE(client.state(), ClientState::Stopped);
    QVERIFY(!client.hasSnapshot());
    QCOMPARE(transport.stopCalls, 1);
}

void BluetoothClientTests::rejectsMalformedAndContradictorySnapshots()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    client.start();
    transport.setOwner(kOwner());
    const quint64 requestId = transport.fetches.constFirst().requestId;

    Snapshot malformed = bluetoothClientSnapshot();
    malformed.adapters[0].address = QStringLiteral("bad-address");
    deliverReady(transport, requestId, malformed);
    QCOMPARE(client.state(), ClientState::Unavailable);
    QCOMPARE(client.reasonCode(), QStringLiteral("malformed-snapshot"));
    QVERIFY(!client.hasSnapshot());

    // A new fetch eventually succeeds after the malformed one.
    transport.emitInvalidated(kOwner(), 61, 6);
    const quint64 retryId = transport.fetches.last().requestId;
    deliverReady(transport, retryId, bluetoothClientSnapshot());
    QCOMPARE(client.state(), ClientState::Ready);

    // Equal revision with different content is a contradiction and revokes
    // the retained snapshot.
    Snapshot contradictory = bluetoothClientSnapshot(61, client.snapshot().revision);
    contradictory.adapters[0].name = QStringLiteral("Other adapter");
    transport.emitInvalidated(kOwner(), 61, contradictory.revision);
    const quint64 contradictionId = transport.fetches.last().requestId;
    deliverReady(transport, contradictionId, contradictory);
    QCOMPARE(client.state(), ClientState::Unavailable);
    QCOMPARE(client.reasonCode(), QStringLiteral("malformed-snapshot"));
    QVERIFY(!client.hasSnapshot());

    // After revocation there is no retained lineage to contradict: the next
    // protocol-valid snapshot restores authority, even at a lower revision.
    transport.emitInvalidated(kOwner(), 61, 2);
    const quint64 regressId = transport.fetches.last().requestId;
    deliverReady(transport, regressId, bluetoothClientSnapshot(61, 1));
    QCOMPARE(client.state(), ClientState::Ready);
    QCOMPARE(client.snapshot().revision, quint64(1));
}

void BluetoothClientTests::coalescesInvalidationsWhileFetching()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    client.start();
    transport.setOwner(kOwner());
    QCOMPARE(transport.fetches.size(), 1);

    transport.emitInvalidated(kOwner(), 61, 6);
    transport.emitInvalidated(kOwner(), 61, 7);
    QCOMPARE(transport.fetches.size(), 1);

    const Snapshot updated = bluetoothClientSnapshot(61, 7);
    deliverReady(transport, transport.fetches.constFirst().requestId, updated);
    // The coalesced second invalidation triggers exactly one follow-up fetch.
    QCOMPARE(transport.fetches.size(), 2);
    deliverReady(transport, transport.fetches.last().requestId, updated);
    QCOMPARE(transport.fetches.size(), 2);
    QCOMPARE(client.snapshot().revision, quint64(7));

    // A stale invalidation that matches current lineage does not refetch.
    transport.emitInvalidated(kOwner(), 61, 5);
    QCOMPARE(transport.fetches.size(), 2);
}

void BluetoothClientTests::ownerReplacementClearsSnapshotAndMarksUncertain()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    QSignalSpy completions(&client, &BluetoothClient::operationCompleted);
    client.start();
    transport.setOwner(kOwner());
    deliverReady(transport, transport.fetches.constFirst().requestId,
                 bluetoothClientSnapshot());

    const quint64 requestId = client.connectDevice({.epoch = 61, .serial = 700});
    QVERIFY(requestId != 0);
    QCOMPARE(transport.submissions.size(), 1);

    // The exact owner is replaced by a different unique name.
    const QString replacement = QStringLiteral(":1.84");
    transport.setOwner(replacement);
    // AGENT-GUARD under test: the owner-replaced uncertainty is queued, so it
    // appears on the next event loop turn, never synchronously.
    QTRY_COMPARE(completions.count(), 1);
    const OperationResult uncertain = completions.takeFirst()[1].value<OperationResult>();
    QCOMPARE(uncertain.status, OperationStatus::Uncertain);
    QCOMPARE(uncertain.reasonCode, QStringLiteral("owner-replaced"));
    QCOMPARE(uncertain.initiatingEpoch, quint64(61));
    QVERIFY(!client.hasSnapshot());
    QVERIFY(!client.operationPending());
    QCOMPARE(transport.fetches.size(), 2);
    QCOMPARE(transport.fetches.last().owner, replacement);

    // A delayed reply from the old owner is refused by owner binding.
    const OperationResult stale{
        OperationKind::Connect,
        OperationStatus::Succeeded,
        61,
        5,
        61,
        5,
        QStringLiteral("connected"),
        {},
        true};
    transport.emitOperationReply(kOwner(), requestId, true, stale);
    QCOMPARE(completions.count(), 0);
}

void BluetoothClientTests::timeoutMakesOperationUncertainAndRefetches()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    QSignalSpy completions(&client, &BluetoothClient::operationCompleted);
    client.setRequestTimeout(50);
    client.start();
    transport.setOwner(kOwner());
    deliverReady(transport, transport.fetches.constFirst().requestId,
                 bluetoothClientSnapshot());

    const quint64 requestId = client.disconnectDevice({.epoch = 61, .serial = 700});
    QVERIFY(client.operationPending());
    QTRY_COMPARE_WITH_TIMEOUT(completions.count(), 1, 5000);
    const QList<QVariant> arguments = completions.takeFirst();
    const OperationResult result = arguments[1].value<OperationResult>();
    QCOMPARE(arguments[0].toULongLong(), requestId);
    QCOMPARE(result.status, OperationStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("operation-timeout"));
    QVERIFY(!client.operationPending());
    // The timeout schedules a snapshot refetch to restore certainty.
    QTRY_COMPARE(transport.fetches.size(), 2);

    // The late service reply can no longer complete anything.
    const OperationResult late{OperationKind::Disconnect,
                               OperationStatus::Succeeded,
                               61,
                               5,
                               61,
                               5,
                               QStringLiteral("disconnected"),
                               {},
                               true};
    transport.emitOperationReply(kOwner(), requestId, true, late);
    QCOMPARE(completions.count(), 0);
}

void BluetoothClientTests::rejectsStaleAndMalformedOperationReplies()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    QSignalSpy completions(&client, &BluetoothClient::operationCompleted);
    client.start();
    transport.setOwner(kOwner());
    deliverReady(transport, transport.fetches.constFirst().requestId,
                 bluetoothClientSnapshot());

    const quint64 requestId = client.connectDevice({.epoch = 61, .serial = 700});
    transport.emitOperationReply(kOwner(), requestId + 100, true,
                                 {OperationKind::Connect,
                                  OperationStatus::Succeeded,
                                  61,
                                  5,
                                  61,
                                  5,
                                  QStringLiteral("connected"),
                                  {},
                                  true});
    QCOMPARE(completions.count(), 0);
    QVERIFY(client.operationPending());

    // A mismatched initiating lineage is converted to Uncertain.
    transport.emitOperationReply(kOwner(), requestId, true,
                                 {OperationKind::Connect,
                                  OperationStatus::Succeeded,
                                  61,
                                  99,
                                  61,
                                  99,
                                  QStringLiteral("connected"),
                                  {},
                                  true});
    QTRY_COMPARE(completions.count(), 1);
    const OperationResult result = completions.takeFirst()[1].value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("malformed-result"));
    QVERIFY(!client.operationPending());
    QTRY_COMPARE(transport.fetches.size(), 2);
}

void BluetoothClientTests::rejectsRequestsThatFailPreflight()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    QSignalSpy completions(&client, &BluetoothClient::operationCompleted);
    client.start();
    transport.setOwner(kOwner());
    deliverReady(transport, transport.fetches.constFirst().requestId,
                 bluetoothClientSnapshot());

    const quint64 staleEpochId = client.connectDevice({.epoch = 60, .serial = 700});
    QVERIFY(staleEpochId != 0);
    QTRY_COMPARE(completions.count(), 1);
    QCOMPARE(completions.takeFirst()[1].value<OperationResult>().reasonCode,
             QStringLiteral("stale-handle"));

    const quint64 staleSerialId = client.disconnectDevice({.epoch = 61, .serial = 424242});
    QVERIFY(staleSerialId != 0);
    QTRY_COMPARE(completions.count(), 1);
    QCOMPARE(completions.takeFirst()[1].value<OperationResult>().reasonCode,
             QStringLiteral("stale-handle"));

    Snapshot off = bluetoothClientSnapshot(61, 6);
    off.adapters[0].powered = false;
    off.devices.clear();
    transport.emitInvalidated(kOwner(), 61, 6);
    deliverReady(transport, transport.fetches.last().requestId, off);
    const quint64 offId = client.acquireDiscovery({.epoch = 61, .serial = 400});
    QVERIFY(offId != 0);
    QTRY_COMPARE(completions.count(), 1);
    QCOMPARE(completions.takeFirst()[1].value<OperationResult>().reasonCode,
             QStringLiteral("adapter-off"));

    // No request reached the transport for any rejected call.
    QCOMPARE(transport.submissions.size(), 0);
}

void BluetoothClientTests::queuesExactlyOnceCompletion()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    QSignalSpy completions(&client, &BluetoothClient::operationCompleted);
    client.start();
    transport.setOwner(kOwner());
    deliverReady(transport, transport.fetches.constFirst().requestId,
                 bluetoothClientSnapshot());

    const quint64 firstId = client.connectDevice({.epoch = 61, .serial = 700});
    QVERIFY(client.operationPending());
    QCOMPARE(completions.count(), 0);

    // A second request while one is in flight completes locally as Busy,
    // through the same asynchronous queue as every other completion.
    const quint64 busyId = client.disconnectDevice({.epoch = 61, .serial = 700});
    QVERIFY(client.operationPending());
    QTRY_COMPARE(completions.count(), 1);
    QCOMPARE(completions[0][0].toULongLong(), busyId);
    const OperationResult busy = completions[0][1].value<OperationResult>();
    QCOMPARE(busy.status, OperationStatus::Busy);
    QCOMPARE(busy.reasonCode, QStringLiteral("operation-busy"));
    QVERIFY(client.operationPending());

    // The dispatched operation completes exactly once, asynchronously.
    transport.emitOperationReply(kOwner(), firstId, true,
                                 {OperationKind::Connect,
                                  OperationStatus::Succeeded,
                                  61,
                                  5,
                                  61,
                                  5,
                                  QStringLiteral("connected"),
                                  {},
                                  true});
    QTRY_COMPARE(completions.count(), 2);
    QCOMPARE(completions[1][0].toULongLong(), firstId);
    QCOMPARE(completions[1][1].value<OperationResult>().status,
             OperationStatus::Succeeded);
    QVERIFY(!client.operationPending());

    // Exactly once: no duplicate delivery ever arrives.
    QTest::qWait(50);
    QCOMPARE(completions.count(), 2);
}

void BluetoothClientTests::fetchFailureRevokesMutationAuthority()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    QSignalSpy completions(&client, &BluetoothClient::operationCompleted);
    client.start();
    transport.setOwner(kOwner());
    deliverReady(transport, transport.fetches.constFirst().requestId,
                 bluetoothClientSnapshot());
    QCOMPARE(client.state(), ClientState::Ready);

    const quint64 requestId = client.setAdapterPower({.epoch = 61, .serial = 400}, false);
    QVERIFY(client.operationPending());

    // The post-dispatch refetch fails: retained state is no longer provable.
    transport.emitInvalidated(kOwner(), 61, 6);
    const quint64 fetchId = transport.fetches.last().requestId;
    transport.emitSnapshotReply(kOwner(), fetchId, false, Snapshot{},
                                QStringLiteral("transport-timeout"));
    QCOMPARE(client.state(), ClientState::Unavailable);
    QVERIFY(!client.hasSnapshot());
    // The dispatched mutation completes as Uncertain, never as success.
    QTRY_COMPARE(completions.count(), 1);
    QCOMPARE(completions[0][0].toULongLong(), requestId);
    const OperationResult uncertain = completions[0][1].value<OperationResult>();
    QCOMPARE(uncertain.status, OperationStatus::Uncertain);
    QCOMPARE(uncertain.reasonCode, QStringLiteral("snapshot-unavailable"));
    QVERIFY(!client.operationPending());

    // And no new mutation may dispatch against the revoked authority.
    const quint64 connectId = client.connectDevice({.epoch = 61, .serial = 700});
    QVERIFY(connectId != 0);
    QTRY_COMPARE(completions.count(), 2);
    QCOMPARE(completions[1][1].value<OperationResult>().reasonCode,
             QStringLiteral("unavailable"));
}

void BluetoothClientTests::stopCancelsAndMarksTransportBackedUncertain()
{
    FakeBluetoothTransport transport;
    BluetoothClient client(&transport);
    QSignalSpy completions(&client, &BluetoothClient::operationCompleted);
    client.start();
    transport.setOwner(kOwner());
    deliverReady(transport, transport.fetches.constFirst().requestId,
                 bluetoothClientSnapshot());

    const quint64 requestId = client.connectDevice({.epoch = 61, .serial = 700});
    QVERIFY(client.operationPending());
    client.stop();
    QTRY_COMPARE(completions.count(), 1);
    const QList<QVariant> arguments = completions.takeFirst();
    const OperationResult result = arguments[1].value<OperationResult>();
    QCOMPARE(arguments[0].toULongLong(), requestId);
    QCOMPARE(result.status, OperationStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("client-stopped"));

    // A late accepted reply is dropped with the cancelled lifetime.
    transport.emitOperationReply(kOwner(), requestId, true,
                                 {OperationKind::Connect,
                                  OperationStatus::Succeeded,
                                  61,
                                  5,
                                  61,
                                  5,
                                  QStringLiteral("connected"),
                                  {},
                                  true});
    QCOMPARE(completions.count(), 0);
}

QTEST_GUILESS_MAIN(BluetoothClientTests)
#include "tst_bluetooth_client.moc"
