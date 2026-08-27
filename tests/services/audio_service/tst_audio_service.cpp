// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_audio_backend.h"

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>

#include <QtTest>

#include <limits>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

class AudioServiceTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesValidatedSnapshots();
    void appliesTypedOperations();
    void rejectsStaleMalformedAndIncompatibleRequests();
    void authorityChangeMakesPendingUncertain();
    void malformedBackendFailsClosed();
};

void AudioServiceTests::publishesValidatedSnapshots()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    QSignalSpy snapshots(&coordinator, &AudioOperationCoordinator::snapshotChanged);
    QSignalSpy invalidations(&coordinator, &AudioOperationCoordinator::invalidated);
    coordinator.start();
    QCOMPARE(backend.startCalls, 1);
    backend.publish(audioSnapshot());
    QCOMPARE(snapshots.count(), 1);
    QCOMPARE(invalidations.count(), 1);
    QCOMPARE(coordinator.snapshot(), audioSnapshot());
    coordinator.stop();
    QCOMPARE(backend.stopCalls, 1);
}

void AudioServiceTests::appliesTypedOperations()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    QSignalSpy completed(&coordinator, &AudioOperationCoordinator::operationCompleted);
    coordinator.start();
    backend.publish(audioSnapshot());
    const OperationRequest request{.kind = OperationKind::SetVolume,
                                   .primary = {.epoch = 7, .serial = 10},
                                   .secondary = {},
                                   .volume = 0.2,
                                   .muted = false};
    const OperationSubmission submission = coordinator.submit(request);
    QVERIFY(submission.pending);
    QCOMPARE(backend.operations.size(), 1);
    QCOMPARE(backend.operations[0].request, request);
    backend.finish(submission.operationId,
                   {.status = BackendOperationStatus::Succeeded,
                    .reasonCode = QStringLiteral("ok"),
                    .diagnostic = {}});
    QCOMPARE(completed.count(), 1);
    const auto result = completed.takeFirst()[1].value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Succeeded);
    QCOMPARE(result.initiatingEpoch, quint64(7));
    QCOMPARE(result.initiatingRevision, quint64(3));
}

void AudioServiceTests::rejectsStaleMalformedAndIncompatibleRequests()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start();
    backend.publish(audioSnapshot());

    auto result = coordinator.submit({.kind = OperationKind::SetMute,
                                      .primary = {.epoch = 6, .serial = 10},
                                      .secondary = {},
                                      .volume = 0.0,
                                      .muted = true});
    QVERIFY(!result.pending);
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("stale-handle"));

    result = coordinator.submit({.kind = OperationKind::SetVolume,
                                 .primary = {.epoch = 7, .serial = 10},
                                 .secondary = {},
                                 .volume = std::numeric_limits<double>::infinity(),
                                 .muted = false});
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("invalid-volume"));

    result = coordinator.submit({.kind = OperationKind::MoveStream,
                                 .primary = {.epoch = 7, .serial = 30},
                                 .secondary = {.epoch = 7, .serial = 20},
                                 .volume = 0.0,
                                 .muted = false});
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("incompatible-target"));
    Snapshot unsupported = audioSnapshot(7, 4);
    unsupported.outputs[0].canSetMute = false;
    backend.publish(unsupported);
    result = coordinator.submit({.kind = OperationKind::SetMute,
                                 .primary = {.epoch = 7, .serial = 10},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = true});
    QCOMPARE(result.immediateResult.status, OperationStatus::Unsupported);
    QVERIFY(backend.operations.isEmpty());
}

void AudioServiceTests::authorityChangeMakesPendingUncertain()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    QSignalSpy completed(&coordinator, &AudioOperationCoordinator::operationCompleted);
    coordinator.start();
    backend.publish(audioSnapshot());
    const auto submission = coordinator.submit({.kind = OperationKind::SetMute,
                                                 .primary = {.epoch = 7, .serial = 10},
                                                 .secondary = {},
                                                 .volume = 0.0,
                                                 .muted = true});
    QVERIFY(submission.pending);
    backend.publish(audioSnapshot(8, 4));
    QCOMPARE(completed.count(), 1);
    const auto uncertain = completed.takeFirst()[1].value<OperationResult>();
    QCOMPARE(uncertain.status, OperationStatus::Uncertain);
    QCOMPARE(uncertain.reasonCode, QStringLiteral("authority-replaced"));
    backend.finish(submission.operationId,
                   {.status = BackendOperationStatus::Succeeded,
                    .reasonCode = QStringLiteral("late"),
                    .diagnostic = {}});
    QCOMPARE(completed.count(), 0);
}

void AudioServiceTests::malformedBackendFailsClosed()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start();
    Snapshot malformed = audioSnapshot();
    malformed.outputs[0].volume = 2.0;
    backend.publish(malformed);
    QCOMPARE(coordinator.snapshot().availability, Availability::Degraded);
    QCOMPARE(coordinator.snapshot().reasonCode, QStringLiteral("backend-malformed"));
    QVERIFY(coordinator.snapshot().outputs.isEmpty());
    QVERIFY(coordinator.snapshot().capabilities == Capabilities{});
}

QTEST_GUILESS_MAIN(AudioServiceTests)
#include "tst_audio_service.moc"
