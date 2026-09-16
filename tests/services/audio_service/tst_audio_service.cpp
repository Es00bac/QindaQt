// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_audio_backend.h"

#include "../../../src/services/audio_service/src/console_endpoints_p.h"

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
    void admitsChannelVolumeOperations();
    void admitsVirtualDeviceOperations();
    void authorityChangeMakesPendingUncertain();
    void malformedBackendFailsClosed();
    void rejectsStoppedSupersededAndRegressedBackendValues();
    void malformedBackendOutcomesBecomeProtocolValidFailures();
    void consoleEndpointsFollowTheGraph();
    void virtualEndpointsAreDeclaredAndBoundByName();
    void meterReadingsStreamWithoutTouchingLineage();
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
    QCOMPARE(coordinator.snapshot(),
             publishedSnapshot(audioSnapshot(), coordinator.consoleModel().console()));
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

void AudioServiceTests::admitsChannelVolumeOperations()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    QSignalSpy completed(&coordinator, &AudioOperationCoordinator::operationCompleted);
    coordinator.start();
    backend.publish(audioSnapshot());

    auto result = coordinator.submit({.kind = OperationKind::SetChannelVolumes,
                                      .primary = {.epoch = 7, .serial = 10},
                                      .secondary = {},
                                      .volume = 0.0,
                                      .muted = false,
                                      .channelVolumes = {0.1, 0.9}});
    QVERIFY(result.pending);
    QCOMPARE(backend.operations.size(), 1);
    QCOMPARE(backend.operations[0].request.kind, OperationKind::SetChannelVolumes);
    QCOMPARE(backend.operations[0].request.channelVolumes, QVector<double>({0.1, 0.9}));
    backend.finish(result.operationId,
                   {.status = BackendOperationStatus::Succeeded,
                    .reasonCode = QStringLiteral("ok"),
                    .diagnostic = {}});
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed.constLast()[1].value<OperationResult>().status,
             OperationStatus::Succeeded);

    result = coordinator.submit({.kind = OperationKind::SetChannelVolumes,
                                 .primary = {.epoch = 7, .serial = 10},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = false,
                                 .channelVolumes = {0.5}});
    QVERIFY(!result.pending);
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("invalid-target"));
    QCOMPARE(result.immediateResult.status, OperationStatus::Rejected);

    result = coordinator.submit({.kind = OperationKind::SetChannelVolumes,
                                 .primary = {.epoch = 7, .serial = 10},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = false,
                                 .channelVolumes = {0.5, 1.5}});
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("invalid-volume"));

    Snapshot channelless = audioSnapshot(7, 5);
    channelless.outputs[0].channelVolumes.clear();
    channelless.outputs[0].channelMap.clear();
    backend.publish(channelless);
    result = coordinator.submit({.kind = OperationKind::SetChannelVolumes,
                                 .primary = {.epoch = 7, .serial = 10},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = false,
                                 .channelVolumes = {0.5, 0.5}});
    QVERIFY(!result.pending);
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("invalid-target"));
    QCOMPARE(backend.operations.size(), 1);
}

void AudioServiceTests::admitsVirtualDeviceOperations()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    QSignalSpy snapshots(&coordinator, &AudioOperationCoordinator::snapshotChanged);
    QSignalSpy completed(&coordinator, &AudioOperationCoordinator::operationCompleted);
    coordinator.start();
    backend.publish(audioSnapshot());

    auto result = coordinator.submit({.kind = OperationKind::CreateVirtualDevice,
                                      .primary = {},
                                      .secondary = {},
                                      .volume = 0.0,
                                      .muted = false,
                                      .channelVolumes = {},
                                      .deviceKind = DeviceKind::Output,
                                      .displayName = QStringLiteral("Studio Bus"),
                                      .channels = 6});
    QVERIFY(result.pending);
    QCOMPARE(backend.operations.size(), 1);
    QCOMPARE(backend.operations[0].request.displayName, QStringLiteral("Studio Bus"));
    QCOMPARE(backend.operations[0].request.channels, quint32(6));
    fulfillVirtualDeviceCreation(backend, result.operationId, 7, 4, 12);
    QTRY_COMPARE(snapshots.count(), 2);
    QCOMPARE(coordinator.snapshot().outputs.size(), 3);
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed.constLast()[1].value<OperationResult>().status,
             OperationStatus::Succeeded);

    result = coordinator.submit({.kind = OperationKind::CreateVirtualDevice,
                                 .primary = {},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = false,
                                 .channelVolumes = {},
                                 .deviceKind = DeviceKind::Input,
                                 .displayName = QString(kMaxVirtualNameUtf8Bytes + 1,
                                                        QLatin1Char('x')),
                                 .channels = 2});
    QVERIFY(!result.pending);
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("invalid-name"));

    result = coordinator.submit({.kind = OperationKind::CreateVirtualDevice,
                                 .primary = {},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = false,
                                 .channelVolumes = {},
                                 .deviceKind = DeviceKind::Output,
                                 .displayName = QStringLiteral("Studio Bus"),
                                 .channels = 5});
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("invalid-channel-count"));

    result = coordinator.submit({.kind = OperationKind::CreateVirtualDevice,
                                 .primary = {},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = false,
                                 .channelVolumes = {},
                                 .deviceKind = DeviceKind::Output,
                                 .displayName = QStringLiteral("Studio Bus"),
                                 .channels = 8});
    QVERIFY(result.pending);

    // Removing a device the service does not manage is refused before the
    // backend can see it; the managed virtual device is dispatched.
    result = coordinator.submit({.kind = OperationKind::RemoveVirtualDevice,
                                 .primary = {.epoch = 7, .serial = 10},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = false});
    QVERIFY(!result.pending);
    QCOMPARE(result.immediateResult.reasonCode, QStringLiteral("invalid-target"));

    result = coordinator.submit({.kind = OperationKind::RemoveVirtualDevice,
                                 .primary = {.epoch = 7, .serial = 11},
                                 .secondary = {},
                                 .volume = 0.0,
                                 .muted = false});
    QVERIFY(result.pending);
    QCOMPARE(backend.operations.constLast().request.kind,
             OperationKind::RemoveVirtualDevice);
    QCOMPARE(backend.operations.constLast().request.primary.serial, quint64(11));
    QCOMPARE(backend.operations.size(), 3);
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
    // The graph capabilities are withdrawn, but the console is the user's own
    // configuration and survives a malformed graph payload - so its bits stay.
    QVERIFY(coordinator.snapshot().capabilities == consoleCapabilityBits());
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
    QCOMPARE(coordinator.snapshot(),
             publishedSnapshot(audioSnapshot(7, 3),
                               coordinator.consoleModel().console()));

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
    QCOMPARE(coordinator.snapshot(),
             publishedSnapshot(secondRun, coordinator.consoleModel().console()));
    backend.publishForGeneration(secondGeneration, secondRun);
    Snapshot equalRevisionContradiction = secondRun;
    equalRevisionContradiction.outputs[0].volume = 0.8;
    backend.publishForGeneration(secondGeneration, equalRevisionContradiction);
    backend.publishForGeneration(secondGeneration, audioSnapshot(8, 2));
    backend.publishForGeneration(secondGeneration, audioSnapshot(7, 100));
    QCOMPARE(snapshots.count(), 3);
    QCOMPARE(coordinator.snapshot(),
             publishedSnapshot(secondRun, coordinator.consoleModel().console()));

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

// ADR-0174. A console that is not attached to the graph draws faders wired to
// nothing: no routing is buildable and no meter has a node to read.
void AudioServiceTests::consoleEndpointsFollowTheGraph()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start();
    backend.publish(audioSnapshot());

    const Console console = coordinator.snapshot().console;
    const Strip &firstStrip = console.strips.at(0);
    QVERIFY(firstStrip.sourceKnown);
    // The DEFAULT input is claimed first: that is what the user means by "my
    // microphone", regardless of where it sorts by serial.
    QCOMPARE(firstStrip.sourceSerial, 20u);

    const Bus &firstBus = console.buses.at(0);
    QVERIFY(firstBus.targetKnown);
    QCOMPARE(firstBus.targetSerial, 10u);
    // The managed virtual output is the console's own endpoint and must not be
    // claimed as though it were hardware the user plugged in.
    for (const Bus &bus : console.buses) {
        QVERIFY(!bus.targetKnown || bus.targetSerial != 11u);
    }
    // There is exactly one input in the fixture, so every later hardware strip
    // stays unbound rather than sharing that one device.
    int boundStrips = 0;
    for (const Strip &strip : console.strips) {
        boundStrips += strip.sourceKnown ? 1 : 0;
    }
    QCOMPARE(boundStrips, 1);

    // Bound endpoints are exactly what gets declared for metering, and a bus is
    // read from its monitor while a hardware strip is read as a capture source.
    QCOMPARE(backend.metering.size(), 2);
    QCOMPARE(backend.metering.at(0).consoleId, firstStrip.id);
    QVERIFY(!backend.metering.at(0).captureSink);
    QCOMPARE(backend.metering.at(1).consoleId, firstBus.id);
    QVERIFY(backend.metering.at(1).captureSink);
}

// ADR-0175. A virtual strip is a sink applications play into and a virtual
// bus is a sink-plus-source other applications record from; the console
// declares them and binds to them by NAME, because that is the one identity
// that survives the daemon restarting.
void AudioServiceTests::virtualEndpointsAreDeclaredAndBoundByName()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start();
    // Declared from the moment the backend runs - before any snapshot - so the
    // sinks exist by the time the first snapshot could bind them.
    QCOMPARE(backend.endpointCalls, 1);
    int strips = 0;
    int buses = 0;
    for (const BackendConsoleEndpoint &endpoint : backend.endpoints) {
        QVERIFY(!endpoint.description.isEmpty());
        (endpoint.isBus ? buses : strips) += 1;
    }
    QCOMPARE(strips, 3);
    QCOMPARE(buses, 3);

    // Without the nodes in the graph the virtual endpoints stay unbound, and
    // nothing else is claimed in their place.
    backend.publish(audioSnapshot());
    for (const Strip &strip : coordinator.snapshot().console.strips) {
        if (strip.kind == StripKind::VirtualInput) {
            QVERIFY(!strip.sourceKnown);
        }
    }

    const QString virtualStrip = QStringLiteral("strip.virtual.1");
    const QString virtualBus = QStringLiteral("bus.b1");
    backend.publish(snapshotWithConsoleEndpoints(
        ConsoleEndpoints::stripSinkNodeName(virtualStrip),
        ConsoleEndpoints::busSinkNodeName(virtualBus), 7, 4));
    const Console console = coordinator.snapshot().console;
    const Strip *boundStrip = nullptr;
    for (const Strip &strip : console.strips) {
        if (strip.id == virtualStrip) {
            boundStrip = &strip;
        }
        // AGENT-GUARD: the console's own sinks are not hardware. A hardware
        // strip must never claim one, or it would meter a bus's own output.
        if (strip.kind == StripKind::HardwareInput && strip.sourceKnown) {
            QVERIFY(strip.sourceSerial != 50 && strip.sourceSerial != 51);
        }
    }
    QVERIFY(boundStrip != nullptr);
    QVERIFY(boundStrip->sourceKnown);
    QCOMPARE(boundStrip->sourceSerial, 50u);
    const Bus *boundBus = nullptr;
    for (const Bus &bus : console.buses) {
        if (bus.id == virtualBus) {
            boundBus = &bus;
        }
        if (bus.kind == BusKind::Physical && bus.targetKnown) {
            QVERIFY(bus.targetSerial != 50 && bus.targetSerial != 51);
        }
    }
    QVERIFY(boundBus != nullptr);
    QVERIFY(boundBus->targetKnown);
    QCOMPARE(boundBus->targetSerial, 51u);

    // A virtual strip's audio is on its sink's monitor, so its meter and any
    // send from it read the sink rather than a capture port.
    bool stripMetered = false;
    for (const BackendMeterTarget &target : backend.metering) {
        if (target.consoleId == virtualStrip) {
            stripMetered = true;
            QVERIFY(target.captureSink);
        }
    }
    QVERIFY(stripMetered);
}

void AudioServiceTests::meterReadingsStreamWithoutTouchingLineage()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start();
    backend.publish(audioSnapshot());
    QSignalSpy snapshots(&coordinator, &AudioOperationCoordinator::snapshotChanged);
    QSignalSpy invalidations(&coordinator, &AudioOperationCoordinator::invalidated);
    QSignalSpy levels(&coordinator, &AudioOperationCoordinator::levelsChanged);
    const quint64 revisionBefore = coordinator.snapshot().revision;
    const QString stripId = coordinator.snapshot().console.strips.at(0).id;

    backend.publishLevels(
        {LevelReading{stripId, Level{.peakDb = -4.0, .rmsDb = -11.0, .known = true}}});
    QCOMPARE(levels.count(), 1);
    // AGENT-GUARD: the whole point of the separate channel. Meters move twenty
    // times a second; if they advanced the revision, every client would refetch
    // the entire snapshot at meter rate and lineage would stop meaning anything.
    QCOMPARE(snapshots.count(), 0);
    QCOMPARE(invalidations.count(), 0);
    QCOMPARE(coordinator.snapshot().revision, revisionBefore);
    // They are still folded into the retained snapshot, so a client connecting
    // mid-stream sees live meters in its very first GetSnapshot.
    QCOMPARE(coordinator.snapshot().console.strips.at(0).level.peakDb, -4.0);

    // An identical batch is not news.
    backend.publishLevels(
        {LevelReading{stripId, Level{.peakDb = -4.0, .rmsDb = -11.0, .known = true}}});
    QCOMPARE(levels.count(), 1);

    // A superseded backend run cannot move a meter.
    backend.publishLevelsForGeneration(
        backend.generation + 1,
        {LevelReading{stripId, Level{.peakDb = 0.0, .rmsDb = 0.0, .known = true}}});
    QCOMPARE(levels.count(), 1);
    QCOMPARE(coordinator.snapshot().console.strips.at(0).level.peakDb, -4.0);

    coordinator.stop();
    backend.publishLevels(
        {LevelReading{stripId, Level{.peakDb = -1.0, .rmsDb = -1.0, .known = true}}});
    QCOMPARE(levels.count(), 1);
}

QTEST_GUILESS_MAIN(AudioServiceTests)
#include "tst_audio_service.moc"
