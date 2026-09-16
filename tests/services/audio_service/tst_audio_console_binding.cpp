// SPDX-License-Identifier: GPL-3.0-or-later

// The console's attachment to the graph: automatic binding (ADR-0174), the
// console's own virtual endpoints (ADR-0175), device pins (ADR-0178), and the
// meter channel that rides on the same bindings. Split from tst_audio_service
// so each file stays inside its source-shape budget.

#include "support/fake_audio_backend.h"

#include "../../../src/services/audio_service/src/console_endpoints_p.h"

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

class AudioConsoleBindingTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void consoleEndpointsFollowTheGraph();
    void virtualEndpointsAreDeclaredAndBoundByName();
    void aPinnedDeviceIsThatElementsAlone();
    void meterReadingsStreamWithoutTouchingLineage();
    void aRackIsDeclaredOnlyWhenActiveAndBound();
};

// ADR-0174. A console that is not attached to the graph draws faders wired to
// nothing: no routing is buildable and no meter has a node to read.
void AudioConsoleBindingTests::consoleEndpointsFollowTheGraph()
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
void AudioConsoleBindingTests::virtualEndpointsAreDeclaredAndBoundByName()
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
// ADR-0178. The user's pin wins over the automatic rule, an absent pinned
// device leaves the element unbound rather than handing it another one, a
// pinned device is never handed to a different element, and clearing the pin
// returns the element to automatic.
void AudioConsoleBindingTests::aPinnedDeviceIsThatElementsAlone()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start();
    Snapshot snapshot = audioSnapshot();
    // A second real input, so the automatic rule has something to hand out.
    snapshot.inputs.push_back({.handle = {.epoch = 7, .serial = 21},
                               .kind = DeviceKind::Input,
                               .name = QStringLiteral("Webcam"),
                               .description = {},
                               .volume = 0.5,
                               .volumeKnown = true,
                               .muted = false,
                               .muteKnown = true,
                               .isDefault = false,
                               .canSetVolume = true,
                               .canSetMute = true,
                               .channelVolumes = {0.5, 0.5},
                               .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
                               .virtualDevice = false,
                               .nodeName = QStringLiteral("alsa_input.usb-webcam")});
    backend.publish(snapshot);
    const Console before = coordinator.snapshot().console;
    // Automatic: strip 1 got the default input (20), strip 2 the webcam (21).
    QCOMPARE(before.strips.at(0).sourceSerial, 20u);
    QCOMPARE(before.strips.at(1).sourceSerial, 21u);
    const QString firstStrip = before.strips.at(0).id;
    const QString secondStrip = before.strips.at(1).id;

    // Pin the default microphone to strip 2. The pin must win: strip 2 takes
    // it, and strip 1 - automatic - must NOT still be sitting on it.
    OperationRequest pin;
    pin.kind = OperationKind::SetStripSource;
    pin.consoleId = secondStrip;
    pin.primary = Handle{7, 20};
    const OperationSubmission pinned = coordinator.submit(pin);
    QVERIFY(!pinned.pending);
    QCOMPARE(pinned.immediateResult.status, OperationStatus::Succeeded);
    Console console = coordinator.snapshot().console;
    QCOMPARE(console.strips.at(1).pinnedSource,
             QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"));
    QCOMPARE(console.strips.at(1).sourceSerial, 20u);
    QVERIFY(console.strips.at(0).sourceKnown);
    QCOMPARE(console.strips.at(0).sourceSerial, 21u);

    // The pinned device goes away: strip 2 stays UNBOUND and does not quietly
    // follow the webcam; strip 1 keeps the webcam.
    Snapshot without = snapshot;
    without.inputs.removeFirst();
    without.defaultInput = {.epoch = 7, .serial = 21};
    // The admission gate checks the default against the flags; a snapshot
    // whose default is not flagged is refused whole, console untouched.
    without.inputs[0].isDefault = true;
    // The pin above republished the console and advanced the revision; a
    // backend snapshot must move past it or it is dropped as contradictory.
    without.revision = 20;
    backend.publish(without);
    console = coordinator.snapshot().console;
    QVERIFY(!console.strips.at(1).sourceKnown);
    QCOMPARE(console.strips.at(1).pinnedSource,
             QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"));
    QCOMPARE(console.strips.at(0).sourceSerial, 21u);

    // Clearing the pin (an invalid handle) returns strip 2 to automatic.
    OperationRequest clear;
    clear.kind = OperationKind::SetStripSource;
    clear.consoleId = secondStrip;
    QCOMPARE(coordinator.submit(clear).immediateResult.status, OperationStatus::Succeeded);
    QVERIFY(coordinator.snapshot().console.strips.at(1).pinnedSource.isEmpty());

    // An output offered to a strip is a client error, not a pin.
    OperationRequest wrongKind;
    wrongKind.kind = OperationKind::SetStripSource;
    wrongKind.consoleId = firstStrip;
    wrongKind.primary = Handle{7, 21};
    Snapshot again = without;
    again.revision = 30;
    backend.publish(again);
    wrongKind.primary = Handle{7, 10};
    const OperationSubmission rejected = coordinator.submit(wrongKind);
    QCOMPARE(rejected.immediateResult.status, OperationStatus::Rejected);
    QCOMPARE(rejected.immediateResult.reasonCode, QStringLiteral("stale-handle"));

    // A bus pin survives the next publication - it used to be overwritten by
    // the automatic rule a moment after it was set.
    OperationRequest busPin;
    busPin.kind = OperationKind::SetBusTarget;
    busPin.consoleId = console.buses.at(1).id;
    busPin.primary = Handle{7, 10};
    QCOMPARE(coordinator.submit(busPin).immediateResult.status, OperationStatus::Succeeded);
    Snapshot later = again;
    later.revision = 40;
    backend.publish(later);
    console = coordinator.snapshot().console;
    QCOMPARE(console.buses.at(1).targetSerial, 10u);
    QCOMPARE(console.buses.at(1).pinnedTarget,
             QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"));
    // ...and the automatic bus 0 no longer holds that device.
    QVERIFY(!console.buses.at(0).targetKnown || console.buses.at(0).targetSerial != 10u);
}
void AudioConsoleBindingTests::meterReadingsStreamWithoutTouchingLineage()
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

// ADR-0179. A rack reaches the graph only when a block is on and the strip's
// device is present; switching every block off withdraws it.
void AudioConsoleBindingTests::aRackIsDeclaredOnlyWhenActiveAndBound()
{
    FakeAudioBackend backend;
    AudioOperationCoordinator coordinator(&backend);
    coordinator.start();
    backend.publish(audioSnapshot());
    QVERIFY(backend.processing.isEmpty());
    const Console console = coordinator.snapshot().console;
    const QString bound = console.strips.at(0).id;    // has the default input
    const QString unbound = console.strips.at(4).id;  // no fifth input in the fixture
    QVERIFY(console.strips.at(0).sourceKnown);
    QVERIFY(!console.strips.at(4).sourceKnown);

    OperationRequest rack;
    rack.kind = OperationKind::SetStripProcessing;
    rack.consoleId = unbound;
    rack.processing.compressor.enabled = true;
    QCOMPARE(coordinator.submit(rack).immediateResult.status, OperationStatus::Succeeded);
    // Enabled on a strip with no device: nothing to run it on, nothing declared.
    QVERIFY(backend.processing.isEmpty());

    rack.consoleId = bound;
    QCOMPARE(coordinator.submit(rack).immediateResult.status, OperationStatus::Succeeded);
    QCOMPARE(backend.processing.size(), 1);
    QCOMPARE(backend.processing.at(0).stripId, bound);
    QCOMPARE(backend.processing.at(0).source.serial, 20u);
    QVERIFY(backend.processing.at(0).processing.compressor.enabled);

    rack.processing.compressor.enabled = false;
    QCOMPARE(coordinator.submit(rack).immediateResult.status, OperationStatus::Succeeded);
    QVERIFY(backend.processing.isEmpty());
}

QTEST_GUILESS_MAIN(AudioConsoleBindingTests)
#include "tst_audio_console_binding.moc"
