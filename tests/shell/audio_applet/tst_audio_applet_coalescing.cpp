// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0191, the controller half: one request in flight per object, at most one
// queued value per object and kind with the latest winning, a queued mute ahead
// of a queued volume, queued intent dropped with its serial, and the value a
// control asked for held until the service answers for it.

#include "support/audio_applet_controller_fixture.h"

using namespace QindaQt::Shell::AudioApplet;
using namespace QindaQt::Shell::AudioApplet::Tests;

class AudioAppletCoalescingTests final : public ControllerFixture {
    Q_OBJECT

private slots:
    void init() { createController(); }
    void cleanup() { destroyController(); }

    void aDragCoalescesToTheLatestValueWithOneRequestInFlight();
    void aQueuedMuteGoesBeforeAQueuedVolume();
    void queuedIntentIsDroppedWithItsSerial();
    void aRefusedChangeReturnsTheControlToServiceTruth();
};

void AudioAppletCoalescingTests::aDragCoalescesToTheLatestValueWithOneRequestInFlight()
{
    publishReadySnapshot();

    // The first move of a drag dispatches immediately.
    QVERIFY(m_controller->requestVolume(1, false, 0.20));
    QCOMPARE(m_transport->submissions.size(), 1);
    QCOMPARE(m_transport->submissions.constLast().request.volume, 0.20);

    // Every later move is accepted and replaces the queued value. None of
    // them dispatch while the first is still in flight, and none of them
    // produce refusal feedback.
    for (const double level : {0.30, 0.40, 0.50, 0.60}) {
        QVERIFY2(m_controller->requestVolume(1, false, level),
                 qPrintable(QStringLiteral("move to %1 was refused").arg(level)));
    }
    QCOMPARE(m_transport->submissions.size(), 1);
    QVERIFY2(!m_controller->feedbackPresent(),
             qPrintable(m_controller->feedback()));
    QCOMPARE(countPendingDeviceRows(), 1);

    // The completion dispatches the value the finger was last on — not 0.30,
    // and not four queued requests.
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(m_transport->submissions.size(), 2);
    QCOMPARE(m_transport->submissions.constLast().request.volume, 0.60);
    QCOMPARE(countPendingDeviceRows(), 1);

    // With the queue empty, the next completion leaves nothing pending and
    // dispatches nothing further.
    m_transport->deliverOperation(
        m_transport->submissions.constLast().requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingDeviceRows(), 0);
    QCOMPARE(m_transport->submissions.size(), 2);
    QVERIFY(!m_controller->feedbackPresent());
}

// A user who reaches for mute during a drag wants silence, not silence after
// the volume lands. Volume and mute queue separately and mute goes first.

void AudioAppletCoalescingTests::aQueuedMuteGoesBeforeAQueuedVolume()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestVolume(1, false, 0.20));
    QCOMPARE(m_transport->submissions.size(), 1);
    QVERIFY(m_controller->requestVolume(1, false, 0.70));
    QVERIFY(m_controller->requestMute(1, false, true));
    QCOMPARE(m_transport->submissions.size(), 1);

    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(m_transport->submissions.size(), 2);
    QCOMPARE(m_transport->submissions.constLast().request.kind,
             OperationKind::SetMute);

    // The queued volume is not lost, only deferred behind the mute.
    m_transport->deliverOperation(
        m_transport->submissions.constLast().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(m_transport->submissions.size(), 3);
    QCOMPARE(m_transport->submissions.constLast().request.kind,
             OperationKind::SetVolume);
    QCOMPARE(m_transport->submissions.constLast().request.volume, 0.70);
}

// AGENT-GUARD: a value queued against an object that leaves the graph must go
// with it. Dispatching it later would apply a stale intent to whatever object
// reuses the serial.

void AudioAppletCoalescingTests::queuedIntentIsDroppedWithItsSerial()
{
    publishReadySnapshot();

    // Serial 2 is the limited input: mute-capable, volume-incapable. One mute
    // goes in flight and a second one queues behind it.
    QVERIFY(m_controller->requestMute(2, false, true));
    QVERIFY(m_controller->requestMute(2, false, false));
    QCOMPARE(m_transport->submissions.size(), 1);
    QCOMPARE(countPendingDeviceRows(), 1);

    // Serial 2 leaves the graph while the second value is still queued. The
    // snapshot has to stay wire-valid for the controller to see the change at
    // all: dropping the default output or a stream's target instead would be
    // rejected whole, leaving no rows and a vacuous assertion below.
    m_transport->invalidate(kEpoch, kRevision + 1);
    Snapshot without = makeReadySnapshot();
    without.revision = kRevision + 1;
    without.inputs.removeAt(0);
    deliverSnapshotAfterRefetch(without);
    QCOMPARE(m_controller->phaseText(), QStringLiteral("ready"));
    QCOMPARE(m_controller->deviceRows().size(), 2);
    QCOMPARE(countPendingDeviceRows(), 0);

    // The serial comes back. Whatever now answers to it is a different object,
    // so the value queued against the old one must be gone.
    m_transport->invalidate(kEpoch, kRevision + 2);
    Snapshot reused = makeReadySnapshot();
    reused.revision = kRevision + 2;
    reused.inputs[0].description = QStringLiteral("A different microphone");
    deliverSnapshotAfterRefetch(reused);
    QCOMPARE(m_controller->deviceRows().size(), 3);
    QCOMPARE(countPendingDeviceRows(), 0);

    // The original request finally completes. Its serial was pruned, so the
    // controller ignores it and drains nothing; the client is now free to
    // submit again.
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Succeeded, {}));
    QTest::qWait(20);
    QCOMPARE(m_transport->submissions.size(), 1);

    // The user mutes the new object once. That request's completion is the
    // only path that drains a queue for this serial: a retained entry would
    // dispatch here and unmute the device the user just muted.
    QVERIFY(m_controller->requestMute(2, false, true));
    QCOMPARE(m_transport->submissions.size(), 2);
    m_transport->deliverOperation(
        m_transport->submissions.constLast().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Succeeded, {}));
    QTest::qWait(50);
    QCOMPARE(m_transport->submissions.size(), 2);
    QCOMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletCoalescingTests::aRefusedChangeReturnsTheControlToServiceTruth()
{
    publishReadySnapshot();

    // The control shows what the user asked for while the request is out.
    QVERIFY(m_controller->requestVolume(1, false, 0.90));
    QCOMPARE(deviceRowFor(1).volume(), 0.90);
    QVERIFY(deviceRowFor(1).volumeIsRequested());

    // The service refuses, and publishes nothing: a refusal usually changes
    // no state, so no newer revision may ever arrive for this object. The
    // control must go back to service truth here rather than stay parked on a
    // value that will never be true.
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Rejected,
                   QStringLiteral("unsupported")));
    QTest::qWait(20);
    QVERIFY(m_controller->feedbackPresent());
    QVERIFY(!deviceRowFor(1).volumeIsRequested());
    QCOMPARE(deviceRowFor(1).volume(), 0.5);
    QCOMPARE(countPendingDeviceRows(), 0);
}

QTEST_MAIN(AudioAppletCoalescingTests)
#include "tst_audio_applet_coalescing.moc"
