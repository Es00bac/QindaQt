// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_power_transport.h"

#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtTest>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

const QString kOwner = QStringLiteral(":1.42");

void driveToReady(PowerClient &client, FakePowerTransport &transport,
                  const Snapshot &snapshot = powerClientSnapshot())
{
    client.start();
    transport.announceOwner(kOwner);
    QVERIFY(!transport.fetches.isEmpty());
    transport.reply(transport.fetches.constLast(), snapshot);
}

} // namespace

class PowerClientTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startDiscoversOwnerAndPublishesValidatedSnapshot();
    void invalidSnapshotIsNeverPublished();
    void regressedLineageIsRejectedAsMalformed();
    void invalidationCoalescesWhileFetchInFlight();
    void ownerReplacementDiscardsSnapshotAndCompletesUncertain();
    void operationTimeoutIsUncertainExactlyOnceAndNeverReplayed();
    void localRejectionsNeverCrossTheTransport();
    void busyOperationIsRejectedLocally();
    void succeededOperationPublishesResultAndRefetches();
    void malformedReplyIsUncertainAndRefetches();
    void stopCompletesInFlightOperationAsClientStopped();
    void snapshotTimeoutTransitionsToUnavailable();
    void requestIdReturnedBeforeCompletion();
};

void PowerClientTests::startDiscoversOwnerAndPublishesValidatedSnapshot()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    QSignalSpy states(&client, &PowerClient::stateChanged);
    QSignalSpy snapshots(&client, &PowerClient::snapshotChanged);

    client.start();
    QCOMPARE(client.state(), PowerClientState::Starting);
    QCOMPARE(transport.startCalls, 1);

    transport.announceOwner(kOwner);
    QCOMPARE(client.owner(), kOwner);
    QCOMPARE(client.state(), PowerClientState::Starting);
    QCOMPARE(transport.fetches.size(), 1);

    transport.reply(transport.fetches.constFirst(), powerClientSnapshot());
    QCOMPARE(client.state(), PowerClientState::Ready);
    QVERIFY(client.hasSnapshot());
    QCOMPARE(client.snapshot().epoch, quint64(11));
    QCOMPARE(snapshots.size(), 1);
    QVERIFY(states.size() >= 2);
    client.stop();
    QCOMPARE(client.state(), PowerClientState::Stopped);
    QCOMPARE(transport.stopCalls, 1);
}

void PowerClientTests::invalidSnapshotIsNeverPublished()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    const Snapshot accepted = client.snapshot();
    const qsizetype initialFetches = transport.fetches.size();

    Snapshot hostile = powerClientSnapshot(11, accepted.revision + 1);
    hostile.supplies.first().percentage = 1'000.0; // out of range
    transport.invalidate(kOwner, 11, accepted.revision + 1);
    transport.reply(transport.fetches.constLast(), hostile);

    QCOMPARE(client.state(), PowerClientState::Unavailable);
    QCOMPARE(client.reasonCode(), QStringLiteral("malformed-snapshot"));
    QCOMPARE(client.snapshot(), accepted);

    // The retry timer republishes valid truth under the same owner. The
    // invalidation already consumed one fetch; the retry adds one more.
    QTRY_COMPARE(transport.fetches.size(), initialFetches + 2);
    transport.reply(transport.fetches.constLast(),
                    powerClientSnapshot(11, accepted.revision + 2));
    QCOMPARE(client.state(), PowerClientState::Ready);
    client.stop();
}

void PowerClientTests::regressedLineageIsRejectedAsMalformed()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    const Snapshot accepted = client.snapshot();
    qsizetype expectedFetches = transport.fetches.size();

    // A-B-A fence: the same owner cannot roll the epoch back.
    Snapshot aToA = powerClientSnapshot(10, 99);
    transport.invalidate(kOwner, 10, 99);
    transport.reply(transport.fetches.constLast(), aToA);
    QCOMPARE(client.state(), PowerClientState::Unavailable);
    QCOMPARE(client.reasonCode(), QStringLiteral("malformed-snapshot"));
    QCOMPARE(client.snapshot(), accepted);

    // Equal lineage with different content is a contradiction. The
    // invalidation consumed one fetch and the rejection scheduled a retry.
    Snapshot contradiction = accepted;
    contradiction.source.docked = true;
    expectedFetches += 2;
    QTRY_COMPARE(transport.fetches.size(), expectedFetches);
    transport.reply(transport.fetches.constLast(), contradiction);
    QCOMPARE(client.reasonCode(), QStringLiteral("malformed-snapshot"));

    // Exact duplicate at equal lineage is harmless.
    ++expectedFetches;
    QTRY_COMPARE(transport.fetches.size(), expectedFetches);
    transport.reply(transport.fetches.constLast(), accepted);
    QCOMPARE(client.state(), PowerClientState::Ready);
    client.stop();
}

void PowerClientTests::invalidationCoalescesWhileFetchInFlight()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    client.start();
    transport.announceOwner(kOwner);
    QCOMPARE(transport.fetches.size(), 1);

    transport.invalidate(kOwner, 11, 5);
    transport.invalidate(kOwner, 11, 6);
    transport.invalidate(kOwner, 11, 7);
    // One fetch is in flight; later invalidations only mark a refetch need.
    QCOMPARE(transport.fetches.size(), 1);
    transport.reply(transport.fetches.constFirst(), powerClientSnapshot(11, 6));
    QTRY_COMPARE(transport.fetches.size(), 2);
    transport.reply(transport.fetches.constLast(), powerClientSnapshot(11, 7));
    QCOMPARE(client.snapshot().revision, quint64(7));
    client.stop();
}

void PowerClientTests::ownerReplacementDiscardsSnapshotAndCompletesUncertain()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    const Handle hold = clientHoldHandle(client.snapshot());
    QSignalSpy completed(&client, &PowerClient::operationCompleted);

    const quint64 requestId = client.releaseProfileHold(hold);
    QVERIFY(requestId != 0);
    QCOMPARE(transport.operations.size(), 1);

    const QString secondOwner = QStringLiteral(":1.99");
    transport.announceOwner(secondOwner);
    QCOMPARE(client.owner(), secondOwner);
    QVERIFY(!client.hasSnapshot());
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(0).toULongLong(), requestId);
    const OperationResult uncertain = completed.first().at(1).value<OperationResult>();
    QCOMPARE(uncertain.status, OperationStatus::Uncertain);
    QCOMPARE(uncertain.reasonCode, QStringLiteral("owner-replaced"));
    QCOMPARE(uncertain.initiatingEpoch, quint64(11));

    transport.reply(transport.fetches.constLast(), powerClientSnapshot(47, 1));
    QCOMPARE(client.snapshot().epoch, quint64(47));

    // A late reply addressed to the old owner is dropped silently.
    transport.finish(transport.operations.constFirst(),
                     powerClientResult(transport.operations.constFirst(),
                                       OperationStatus::Succeeded,
                                       QStringLiteral("applied")));
    QCOMPARE(completed.size(), 1);
    client.stop();
}

void PowerClientTests::operationTimeoutIsUncertainExactlyOnceAndNeverReplayed()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    const Handle device = clientKeyboardHandle(client.snapshot());
    QSignalSpy completed(&client, &PowerClient::operationCompleted);
    client.setRequestTimeout(50);

    const quint64 requestId = client.setKeyboardBrightness(device, 200);
    QVERIFY(client.operationPending());
    QVERIFY(requestId != 0);
    QTest::qWait(200);
    QVERIFY(!client.operationPending());
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(0).toULongLong(), requestId);
    const OperationResult timedOut = completed.first().at(1).value<OperationResult>();
    QCOMPARE(timedOut.status, OperationStatus::Uncertain);
    QCOMPARE(timedOut.reasonCode, QStringLiteral("operation-timeout"));

    // The timed-out operation is never replayed; the late upstream success is
    // dropped rather than trusted or re-sent.
    transport.finish(transport.operations.constFirst(),
                     powerClientResult(transport.operations.constFirst(),
                                       OperationStatus::Succeeded,
                                       QStringLiteral("applied")));
    QCOMPARE(completed.size(), 1);
    QCOMPARE(transport.operations.size(), 1);
    client.stop();
}

void PowerClientTests::localRejectionsNeverCrossTheTransport()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    QSignalSpy completed(&client, &PowerClient::operationCompleted);
    const Snapshot snapshot = client.snapshot();

    const quint64 requestId = client.setProfile(QStringLiteral("turbo"));
    QVERIFY(requestId != 0);
    transport.operationSubmitted = nullptr;
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.last().at(1).value<OperationResult>().status,
             OperationStatus::Rejected);
    QCOMPARE(completed.last().at(1).value<OperationResult>().reasonCode,
             QStringLiteral("unknown-profile"));

    Handle stale = clientHoldHandle(snapshot);
    stale.epoch -= 1;
    QVERIFY(client.releaseProfileHold(stale) != 0);
    QTRY_COMPARE(completed.size(), 2);
    QCOMPARE(completed.last().at(1).value<OperationResult>().reasonCode,
             QStringLiteral("stale-handle"));

    const quint64 oversize = client.setKeyboardBrightness(clientKeyboardHandle(snapshot),
                                                          999);
    QTRY_COMPARE(completed.size(), 3);
    QCOMPARE(completed.last().at(0).toULongLong(), oversize);
    QCOMPARE(completed.last().at(1).value<OperationResult>().status,
             OperationStatus::Unsupported);

    // None of the rejections reached the transport.
    QCOMPARE(transport.operations.size(), 0);
    client.stop();
}

void PowerClientTests::busyOperationIsRejectedLocally()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    QSignalSpy completed(&client, &PowerClient::operationCompleted);

    const quint64 first = client.setProfile(QStringLiteral("balanced"));
    const quint64 second = client.setProfile(QStringLiteral("power-saver"));
    QVERIFY(first != 0);
    QVERIFY(second != 0);
    QCOMPARE(transport.operations.size(), 1);
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(0).toULongLong(), second);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Busy);
    QVERIFY(client.operationPending());

    transport.finish(transport.operations.constFirst(),
                     powerClientResult(transport.operations.constFirst(),
                                       OperationStatus::Succeeded,
                                       QStringLiteral("applied")));
    QTRY_COMPARE(completed.size(), 2);
    QCOMPARE(completed.last().at(0).toULongLong(), first);
    QCOMPARE(completed.last().at(1).value<OperationResult>().status,
             OperationStatus::Succeeded);
    client.stop();
}

void PowerClientTests::succeededOperationPublishesResultAndRefetches()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    const Handle hold = clientHoldHandle(client.snapshot());
    QSignalSpy completed(&client, &PowerClient::operationCompleted);

    const quint64 requestId = client.releaseProfileHold(hold);
    QCOMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constFirst().request.kind,
             OperationKind::ReleaseProfileHold);
    QCOMPARE(transport.operations.constFirst().request.handle, hold);

    transport.finish(transport.operations.constFirst(),
                     powerClientResult(transport.operations.constFirst(),
                                       OperationStatus::Succeeded,
                                       QStringLiteral("released")));
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(0).toULongLong(), requestId);
    const OperationResult result = completed.first().at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Succeeded);
    QCOMPARE(result.reasonCode, QStringLiteral("released"));
    // Completion triggers a resnapshot of the mutated truth.
    QTRY_VERIFY(transport.fetches.size() >= 2);
    client.stop();
}

void PowerClientTests::malformedReplyIsUncertainAndRefetches()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    QSignalSpy completed(&client, &PowerClient::operationCompleted);

    const quint64 requestId = client.setProfile(QStringLiteral("balanced"));
    QVERIFY(requestId != 0);
    OperationResult hostile = powerClientResult(transport.operations.constFirst(),
                                                OperationStatus::Succeeded,
                                                QStringLiteral("applied"));
    hostile.initiatingEpoch = 99; // not the request's lineage
    transport.finish(transport.operations.constFirst(), hostile);
    QTRY_COMPARE(completed.size(), 1);
    const OperationResult uncertain = completed.first().at(1).value<OperationResult>();
    QCOMPARE(uncertain.status, OperationStatus::Uncertain);
    QCOMPARE(uncertain.reasonCode, QStringLiteral("malformed-result"));
    QTRY_VERIFY(transport.fetches.size() >= 2);
    client.stop();
}

void PowerClientTests::stopCompletesInFlightOperationAsClientStopped()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    const Handle device = clientKeyboardHandle(client.snapshot());
    QSignalSpy completed(&client, &PowerClient::operationCompleted);

    const quint64 requestId = client.setKeyboardBrightness(device, 42);
    QVERIFY(client.operationPending());
    QVERIFY(requestId != 0);
    client.stop();
    QTRY_COMPARE(completed.size(), 1);
    const OperationResult stopped = completed.first().at(1).value<OperationResult>();
    QCOMPARE(stopped.reasonCode, QStringLiteral("client-stopped"));
    QCOMPARE(stopped.status, OperationStatus::Uncertain);
    QVERIFY(requestId != 0);
}

void PowerClientTests::snapshotTimeoutTransitionsToUnavailable()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    client.setRequestTimeout(40);
    client.start();
    transport.announceOwner(kOwner);
    QTest::qWait(120);
    QCOMPARE(client.state(), PowerClientState::Unavailable);
    QCOMPARE(client.reasonCode(), QStringLiteral("snapshot-timeout"));
    // The retry timer keeps trying under the same owner.
    QTRY_VERIFY(transport.fetches.size() >= 2);
    client.stop();
}

void PowerClientTests::requestIdReturnedBeforeCompletion()
{
    FakePowerTransport transport;
    PowerClient client(&transport);
    driveToReady(client, transport);
    QSignalSpy completed(&client, &PowerClient::operationCompleted);

    transport.operationSubmitted = [&completed](const FakePowerTransport::Operation &) {
        // The caller cannot have observed a completion for a request whose ID
        // it has not yet received.
        QCOMPARE(completed.size(), 0);
    };
    const quint64 requestId = client.setProfile(QStringLiteral("balanced"));
    QCOMPARE(completed.size(), 0);
    QVERIFY(requestId != 0);
    client.stop();
    Q_UNUSED(requestId);
}

QTEST_GUILESS_MAIN(PowerClientTests)
#include "tst_power_client.moc"
