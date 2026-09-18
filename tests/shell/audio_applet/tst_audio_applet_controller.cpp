// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/audio_applet_controller_fixture.h"

using namespace QindaQt::Shell::AudioApplet;
using namespace QindaQt::Shell::AudioApplet::Tests;


class AudioAppletControllerTests final : public ControllerFixture {
    Q_OBJECT

private slots:
    void init() { createController(); }
    void cleanup() { destroyController(); }

    void stoppedClientProjectsLoadingWithoutRows();
    void readySnapshotProjectsRowsDefaultsAndLabels();
    void invalidWireSnapshotFailsClosedThroughClient();
    void volumeRequestClampsBeforeDispatch();
    void nonFiniteVolumeIsRefusedWhileOutOfRangeClamps();
    void uncapableRowIsRefusedWithoutDispatch();
    void rejectedResultPublishesUnsupportedFeedback();
    void uncertainResultPublishesConfirmationFeedback();
    void successClearsPendingWithoutFeedback();
    void pendingPrunesWhenSerialVanishesAndLateReplyIsIgnored();
    void degradedSnapshotKeepsRowsWithReason();
    void unavailableSnapshotFailsClosedWithReason();
    void grantsGateReadAndControlIndependently();
    void ownerReplacementClearsTruthAndPendingWithoutReplay();

};






void AudioAppletControllerTests::stoppedClientProjectsLoadingWithoutRows()
{
    QCOMPARE(m_controller->phaseText(), QStringLiteral("loading"));
    QVERIFY(m_controller->deviceRows().isEmpty());
    QVERIFY(m_controller->streamRows().isEmpty());
    QVERIFY(!m_controller->feedbackPresent());
    QVERIFY(!m_controller->hasDefaultOutput());
    QVERIFY(!m_controller->hasDefaultInput());
}

void AudioAppletControllerTests::readySnapshotProjectsRowsDefaultsAndLabels()
{
    publishReadySnapshot();

    QCOMPARE(m_controller->deviceRows().size(), 3);
    QCOMPARE(m_controller->streamRows().size(), 1);
    QCOMPARE(m_controller->overflowDeviceCount(), 0);
    QCOMPARE(m_controller->overflowStreamCount(), 0);
    QVERIFY(m_controller->hasDefaultOutput());
    QCOMPARE(m_controller->defaultOutputLabel(),
             QStringLiteral("Built-in Speakers"));
    QVERIFY(m_controller->hasDefaultInput());
    QCOMPARE(m_controller->defaultInputLabel(),
             QStringLiteral("Webcam Microphone"));

    const auto defaultRow =
        m_controller->deviceRows().at(0).value<DeviceRow>();
    QCOMPARE(defaultRow.serial(), 1ULL);
    QVERIFY(defaultRow.isDefault());
    QVERIFY(defaultRow.canSetVolume());
    QVERIFY(defaultRow.volumeKnown());
    QVERIFY(!defaultRow.pending());
    QCOMPARE(defaultRow.volume(), 0.5);

    const auto limitedRow =
        m_controller->deviceRows().at(1).value<DeviceRow>();
    QVERIFY(!limitedRow.volumeKnown());
    QVERIFY(!limitedRow.canSetVolume());
    QVERIFY(limitedRow.canSetMute());

    const auto streamRow =
        m_controller->streamRows().at(0).value<StreamRow>();
    QCOMPARE(streamRow.label(), QStringLiteral("Music Player"));
    QVERIFY(streamRow.isPlayback());
    QVERIFY(streamRow.canSetVolume());
}

void AudioAppletControllerTests::invalidWireSnapshotFailsClosedThroughClient()
{
    m_client->start();
    m_transport->changeOwner(kOwner);
    QVERIFY(!m_transport->fetches.isEmpty());
    Snapshot snapshot = makeReadySnapshot();
    snapshot.wireValid = false;
    m_transport->deliverSnapshot(m_transport->fetches.constLast().requestId,
                                 true, snapshot);
    QCOMPARE(m_controller->phaseText(), QStringLiteral("unavailable"));
    QVERIFY(m_controller->deviceRows().isEmpty());
    QVERIFY(m_controller->streamRows().isEmpty());
    QVERIFY(!m_controller->phaseReasonText().isEmpty());
}

void AudioAppletControllerTests::volumeRequestClampsBeforeDispatch()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestVolume(1, false, 1.7));
    QCOMPARE(m_transport->submissions.size(), 1);
    const auto submission = m_transport->submissions.constFirst();
    QCOMPARE(submission.request.kind, OperationKind::SetVolume);
    QCOMPARE(submission.request.primary.serial, 1ULL);
    QCOMPARE(submission.request.primary.epoch, kEpoch);
    QCOMPARE(submission.request.volume, 1.0);
    QCOMPARE(countPendingDeviceRows(), 1);

    m_transport->deliverOperation(
        submission.requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::nonFiniteVolumeIsRefusedWhileOutOfRangeClamps()
{
    publishReadySnapshot();

    // A non-finite level is domain-invalid: refused locally, never dispatched,
    // and always explained by feedback.
    QSignalSpy feedbackSpy(m_controller,
                           &AudioAppletController::feedbackChanged);
    QVERIFY(!m_controller->requestVolume(
        1, false, std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(m_transport->submissions.size(), 0);
    QCOMPARE(feedbackSpy.size(), 1);
    QVERIFY(m_controller->feedbackPresent());
    QCOMPARE(countPendingDeviceRows(), 0);

    // A finite out-of-range level is clamped into range and dispatched.
    m_controller->clearFeedback();
    QVERIFY(m_controller->requestVolume(1, false, 42.0));
    QCOMPARE(m_transport->submissions.size(), 1);
    QCOMPARE(m_transport->submissions.constFirst().request.volume, 1.0);
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::uncapableRowIsRefusedWithoutDispatch()
{
    publishReadySnapshot();

    QVERIFY(!m_controller->requestVolume(2, false, 0.3));
    QCOMPARE(m_transport->submissions.size(), 0);
    QCOMPARE(m_controller->feedback(),
             AudioAppletController::tr(
                 "This device does not support volume changes."));
    QCOMPARE(countPendingDeviceRows(), 0);

    // The same row still allows a mute request through.
    QVERIFY(m_controller->requestMute(2, false, true));
    QCOMPARE(m_transport->submissions.size(), 1);
    QCOMPARE(m_transport->submissions.constFirst().request.kind,
             OperationKind::SetMute);
    QCOMPARE(m_transport->submissions.constFirst().request.muted, true);
    QCOMPARE(countPendingDeviceRows(), 1);

    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingDeviceRows(), 0);
}

// ADR-0191. This replaces `secondRequestWhilePendingIsRefused`, whose
// contract was the defect: a pointer drag dispatched its first move, the row
// went pending, the slider disabled itself on that pending flag, and the
// second move was refused with "A change for this item is already in
// progress." The drag therefore produced exactly one step and died.
void AudioAppletControllerTests::rejectedResultPublishesUnsupportedFeedback()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestMute(3, false, true));
    m_transport->deliverOperation(
        m_transport->submissions.constLast().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Unsupported,
                   QStringLiteral("unsupported")));
    QTRY_COMPARE(m_controller->feedback(),
                 AudioAppletController::tr(
                     "This device does not support that change."));
    QCOMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::uncertainResultPublishesConfirmationFeedback()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestMute(1, false, true));
    // A transport loss is classified uncertain by the client and must reach
    // the user as an unconfirmed change, never as silent success.
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, false,
        makeResult(OperationKind::SetMute, OperationStatus::Failed, {}),
        QStringLiteral("operation-timeout"));
    QTRY_COMPARE(
        m_controller->feedback(),
        AudioAppletController::tr("The %1 change could not be confirmed. "
                                  "Shown state will refresh from the audio "
                                  "service.")
            .arg(AudioAppletController::tr("mute")));
    QCOMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::successClearsPendingWithoutFeedback()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestVolume(4, true, 0.9));
    const auto submission = m_transport->submissions.constFirst();
    QCOMPARE(submission.request.kind, OperationKind::SetVolume);
    QCOMPARE(submission.request.primary.serial, 4ULL);
    QCOMPARE(submission.request.volume, 0.9);
    QCOMPARE(countPendingStreamRows(), 1);
    QCOMPARE(countPendingDeviceRows(), 0);
    QVERIFY(!m_controller->feedbackPresent());

    m_transport->deliverOperation(
        submission.requestId, true,
        makeResult(OperationKind::SetVolume, OperationStatus::Succeeded, {}));
    QTRY_COMPARE(countPendingStreamRows(), 0);
    QVERIFY(!m_controller->feedbackPresent());
}

void AudioAppletControllerTests::
    pendingPrunesWhenSerialVanishesAndLateReplyIsIgnored()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestMute(2, false, true));
    QCOMPARE(countPendingDeviceRows(), 1);

    // A same-owner revision drops serial 2 from the graph. Reprojection must
    // clear the stale pending flag without waiting for the operation.
    m_transport->invalidate(kEpoch, kRevision + 1);
    Snapshot replacement = makeReadySnapshot();
    replacement.revision = kRevision + 1;
    replacement.inputs.removeAt(0);
    deliverSnapshotAfterRefetch(replacement);
    QCOMPARE(m_controller->deviceRows().size(), 2);
    QCOMPARE(countPendingDeviceRows(), 0);

    // The still-dispatched mute completes after its serial vanished. The late
    // completion is ignored: no feedback and no state change.
    m_transport->deliverOperation(
        m_transport->submissions.constFirst().requestId, true,
        makeResult(OperationKind::SetMute, OperationStatus::Succeeded, {}));
    QTest::qWait(50);
    QVERIFY(!m_controller->feedbackPresent());
    QCOMPARE(countPendingDeviceRows(), 0);
}

void AudioAppletControllerTests::degradedSnapshotKeepsRowsWithReason()
{
    publishReadySnapshot();

    Snapshot degraded = withEpoch(makeReadySnapshot(), kEpoch + 1);
    degraded.availability = Availability::Degraded;
    degraded.reasonCode = QStringLiteral("backend-malformed");
    m_transport->invalidate(kEpoch + 1, 1);
    deliverSnapshotAfterRefetch(degraded);

    QCOMPARE(m_controller->phaseText(), QStringLiteral("degraded"));
    QVERIFY(!m_controller->phaseReasonText().isEmpty());
    QCOMPARE(m_controller->deviceRows().size(), 3);
    QCOMPARE(m_controller->streamRows().size(), 1);
    QVERIFY(!m_controller->feedbackPresent());
}

void AudioAppletControllerTests::unavailableSnapshotFailsClosedWithReason()
{
    publishReadySnapshot();

    Snapshot unavailable = withEpoch(makeReadySnapshot(), kEpoch + 1);
    unavailable.availability = Availability::Unavailable;
    unavailable.reasonCode = QStringLiteral("unavailable");
    m_transport->invalidate(kEpoch + 1, 1);
    deliverSnapshotAfterRefetch(unavailable);

    QCOMPARE(m_controller->phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(
        m_controller->phaseReasonText(),
        AudioAppletController::tr("The audio service is not available right now."));
    QVERIFY(m_controller->deviceRows().isEmpty());
    QVERIFY(m_controller->streamRows().isEmpty());
}

void AudioAppletControllerTests::grantsGateReadAndControlIndependently()
{
    // Read denial suppresses observation entirely: even a ready client behind
    // the facade must present as policy-unavailable, never as live truth.
    AudioAppletController denied(m_client, false, true);
    m_client->start();
    m_transport->changeOwner(kOwner);
    m_transport->deliverSnapshot(m_transport->fetches.constLast().requestId,
                                 true, makeReadySnapshot());
    QCOMPARE(denied.phaseText(), QStringLiteral("unavailable"));
    QCOMPARE(denied.phaseReasonText(),
             AudioAppletController::tr(
                 "Audio access was not granted to this applet."));
    QVERIFY(denied.deviceRows().isEmpty());
    QVERIFY(!denied.requestMute(1, false, true));
    QCOMPARE(m_transport->submissions.size(), 0);

    // Control denial keeps bounded rows visible but refuses every mutation
    // before dispatch; observation still works.
    delete m_controller;
    m_controller =
        new AudioAppletController(m_client, true, false, this);
    QCOMPARE(m_controller->phaseText(), QStringLiteral("ready"));
    QCOMPARE(m_controller->deviceRows().size(), 3);
    QVERIFY(!m_controller->isControlGranted());
    QVERIFY(!m_controller->requestVolume(1, false, 0.2));
    QCOMPARE(m_transport->submissions.size(), 0);
    QCOMPARE(m_controller->feedback(),
             AudioAppletController::tr(
                 "Audio controls are not allowed for this applet."));
    QCOMPARE(countPendingDeviceRows(), 0);

    // Control without read is not control: the same refusal path applies.
    delete m_controller;
    m_controller = new AudioAppletController(m_client, false, false, this);
    QVERIFY(!m_controller->isControlGranted());
    QVERIFY(!m_controller->requestMute(1, false, true));
    QCOMPARE(m_transport->submissions.size(), 0);
}

void AudioAppletControllerTests::
    ownerReplacementClearsTruthAndPendingWithoutReplay()
{
    publishReadySnapshot();

    QVERIFY(m_controller->requestMute(1, false, true));
    QCOMPARE(countPendingDeviceRows(), 1);

    // An exact-owner replacement invalidates the old lineage: rows must
    // clear immediately, the in-flight request resolves as uncertain, and
    // nothing replays against the next owner.
    const quint64 staleRequestId = m_transport->fetches.constLast().requestId;
    m_transport->changeOwner(QStringLiteral(":1.99"));
    QVERIFY(m_controller->deviceRows().isEmpty());
    QVERIFY(m_controller->streamRows().isEmpty());
    QTRY_COMPARE(countPendingDeviceRows(), 0);
    QCOMPARE(m_controller->phaseText(), QStringLiteral("loading"));
    QTRY_COMPARE(m_controller->feedback(),
                 AudioAppletController::tr("The %1 change could not be confirmed. "
                                           "Shown state will refresh from the audio "
                                           "service.")
                     .arg(AudioAppletController::tr("mute")));

    // A stale reply addressed to the old owner changes nothing: AudioClient
    // drops it, truth stays cleared, and the phase does not revive.
    m_transport->deliverSnapshotAs(kOwner, staleRequestId, true,
                                   withEpoch(makeReadySnapshot(), kEpoch));
    QCOMPARE(m_controller->phaseText(), QStringLiteral("loading"));
    QVERIFY(m_controller->deviceRows().isEmpty());

    // The new owner must be answered before truth returns.
    m_transport->deliverSnapshotAs(QStringLiteral(":1.99"),
                                   m_transport->fetches.constLast().requestId,
                                   true, withEpoch(makeReadySnapshot(), kEpoch + 1));
    QCOMPARE(m_controller->phaseText(), QStringLiteral("ready"));
    QCOMPARE(m_controller->deviceRows().size(), 3);
}

QTEST_GUILESS_MAIN(AudioAppletControllerTests)

#include "tst_audio_applet_controller.moc"
