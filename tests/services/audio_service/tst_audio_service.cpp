// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_audio_backend.h"

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

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
    void rejectsStoppedSupersededAndRegressedBackendValues();
    void malformedBackendOutcomesBecomeProtocolValidFailures();
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

void AudioServiceTests::rejectsStoppedSupersededAndRegressedBackendValues()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    QSignalSpy snapshots(&coordinator, &AudioOperationCoordinator::snapshotChanged);
    QSignalSpy completed(&coordinator, &AudioOperationCoordinator::operationCompleted);

    coordinator.start();
    const quint64 firstGeneration = backend.generation;
    backend.publish(audioSnapshot(7, 3));
    QCOMPARE(snapshots.count(), 1);
    const auto firstSubmission = coordinator.submit(
        {.kind = OperationKind::SetMute,
         .primary = {.epoch = 7, .serial = 10},
         .secondary = {},
         .volume = 0.0,
         .muted = true});
    QVERIFY(firstSubmission.pending);
    coordinator.stop();
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed[0][1].value<OperationResult>().reasonCode,
             QStringLiteral("service-stopped"));

    backend.publishForGeneration(firstGeneration, audioSnapshot(7, 4));
    backend.finishForGeneration(
        firstGeneration, firstSubmission.operationId,
        {.status = BackendOperationStatus::Succeeded,
         .reasonCode = QStringLiteral("late-stopped"),
         .diagnostic = {}});
    QCOMPARE(snapshots.count(), 1);
    QCOMPARE(completed.count(), 1);
    QCOMPARE(coordinator.snapshot(), audioSnapshot(7, 3));

    coordinator.start();
    const quint64 secondGeneration = backend.generation;
    QVERIFY(secondGeneration != firstGeneration);
    QCOMPARE(snapshots.count(), 2);
    QCOMPARE(coordinator.snapshot().availability, Availability::Starting);
    QVERIFY(coordinator.snapshot().outputs.isEmpty());
    backend.publishForGeneration(firstGeneration, audioSnapshot(9, 1));
    backend.publishForGeneration(secondGeneration, audioSnapshot(7, 99));
    QCOMPARE(snapshots.count(), 2);

    const Snapshot secondRun = audioSnapshot(8, 3);
    backend.publishForGeneration(secondGeneration, secondRun);
    QCOMPARE(snapshots.count(), 3);
    QCOMPARE(coordinator.snapshot(), secondRun);
    backend.publishForGeneration(secondGeneration, secondRun);
    Snapshot equalRevisionContradiction = secondRun;
    equalRevisionContradiction.outputs[0].volume = 0.8;
    backend.publishForGeneration(secondGeneration, equalRevisionContradiction);
    backend.publishForGeneration(secondGeneration, audioSnapshot(8, 2));
    backend.publishForGeneration(secondGeneration, audioSnapshot(7, 100));
    QCOMPARE(snapshots.count(), 3);
    QCOMPARE(coordinator.snapshot(), secondRun);

    const auto secondSubmission = coordinator.submit(
        {.kind = OperationKind::SetMute,
         .primary = {.epoch = 8, .serial = 10},
         .secondary = {},
         .volume = 0.0,
         .muted = false});
    QVERIFY(secondSubmission.pending);
    backend.finishForGeneration(
        firstGeneration, secondSubmission.operationId,
        {.status = BackendOperationStatus::Succeeded,
         .reasonCode = QStringLiteral("late-generation"),
         .diagnostic = {}});
    QCOMPARE(completed.count(), 1);
    backend.finishForGeneration(
        secondGeneration, secondSubmission.operationId,
        {.status = BackendOperationStatus::Succeeded,
         .reasonCode = QStringLiteral("ok"),
         .diagnostic = {}});
    QCOMPARE(completed.count(), 2);
    QCOMPARE(completed[1][1].value<OperationResult>().status,
             OperationStatus::Succeeded);
}

void AudioServiceTests::malformedBackendOutcomesBecomeProtocolValidFailures()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    QSignalSpy completed(&coordinator, &AudioOperationCoordinator::operationCompleted);
    coordinator.start();
    backend.publish(audioSnapshot());

    const QList<BackendOperationOutcome> malformed{
        {.status = BackendOperationStatus::Failed,
         .reasonCode = QString(kMaxReasonCodeUtf8Bytes + 1, QLatin1Char('x')),
         .diagnostic = {}},
        {.status = BackendOperationStatus::Failed,
         .reasonCode = QStringLiteral("bad") + QChar::Null + QStringLiteral("reason"),
         .diagnostic = {}},
        {.status = BackendOperationStatus::Failed,
         .reasonCode = QStringLiteral("bad_reason"),
         .diagnostic = {}},
        {.status = BackendOperationStatus::Failed,
         .reasonCode = QStringLiteral("backend-failed"),
         .diagnostic = QString(QChar(0x0001))},
        {.status = BackendOperationStatus::Failed,
         .reasonCode = QStringLiteral("backend-failed"),
         .diagnostic = QString(kMaxDiagnosticUtf8Bytes + 1, QLatin1Char('x'))},
        {.status = static_cast<BackendOperationStatus>(99),
         .reasonCode = QStringLiteral("invented-status"),
         .diagnostic = QStringLiteral("unsafe classification")},
    };

    for (const BackendOperationOutcome &outcome : malformed) {
        const auto submission = coordinator.submit(
            {.kind = OperationKind::SetMute,
             .primary = {.epoch = 7, .serial = 10},
             .secondary = {},
             .volume = 0.0,
             .muted = true});
        QVERIFY(submission.pending);
        backend.finish(submission.operationId, outcome);
        const OperationResult result = completed.constLast()[1].value<OperationResult>();
        QCOMPARE(result.status, OperationStatus::Failed);
        QCOMPARE(result.reasonCode, QStringLiteral("backend-malformed"));
        QVERIFY(result.diagnostic.isEmpty());
        QVERIFY(validateOperationResult(result).accepted);
    }
    QCOMPARE(completed.count(), malformed.size());
}

QTEST_GUILESS_MAIN(AudioServiceTests)
#include "tst_audio_service.moc"
