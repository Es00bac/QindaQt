// SPDX-License-Identifier: GPL-3.0-or-later
// The pure half of the OBS console bridge (ADR-0208): the projection from an
// Audio1 snapshot to OBS sources, and the sync plan against what OBS holds.
#include "support.h"

#include "qindaqt/obs_bridge/console_sources.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::ObsBridge;

namespace {

ExistingSource existingOf(const DesiredSource &source)
{
    return {source.consoleId, source.kind, source.sourceName, source.captureKind,
            source.captureDevice};
}

QList<ExistingSource> existingOf(const QList<DesiredSource> &sources)
{
    QList<ExistingSource> existing;
    for (const DesiredSource &source : sources) {
        existing.append(existingOf(source));
    }
    return existing;
}

} // namespace

class ConsoleSourcesTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tokensRoundTrip();
    void projectsBusesAndStripsInConsoleOrder();
    void namesStayUniqueAndCodesFallBack();
    void plansCreationsForAnEmptyObs();
    void plansNothingWhenObsMatches();
    void plansRenamesRetargetsAndRemovals();
    void plansDuplicateAndKindMismatchRepairs();
};

void ConsoleSourcesTests::tokensRoundTrip()
{
    for (const auto kind : {CaptureKind::None, CaptureKind::Input, CaptureKind::Monitor}) {
        QCOMPARE(captureKindFromToken(captureKindToken(kind)), std::optional(kind));
    }
    QVERIFY(!captureKindFromToken(QStringLiteral("loopback")).has_value());
    QCOMPARE(sourceKindId(SourceKind::Bus), QStringLiteral("qindaqt_console_bus"));
    QCOMPARE(sourceKindId(SourceKind::Strip), QStringLiteral("qindaqt_console_strip"));
    QCOMPARE(sourceKindFromId(QStringLiteral("qindaqt_console_strip")),
             std::optional(SourceKind::Strip));
    QVERIFY(!sourceKindFromId(QStringLiteral("pulse_input_capture")).has_value());
    QCOMPARE(consoleNodeName(QStringLiteral("bus.b1")), QStringLiteral("qindaqt.console.bus.b1"));
}

void ConsoleSourcesTests::projectsBusesAndStripsInConsoleOrder()
{
    const auto sources = desiredSources(consoleFixture());
    QCOMPARE(sources.size(), 7);

    const DesiredSource &a1 = sources[0];
    QCOMPARE(a1.consoleId, QStringLiteral("bus.a1"));
    QCOMPARE(a1.kind, SourceKind::Bus);
    QCOMPARE(a1.code, QStringLiteral("A1"));
    QCOMPARE(a1.sourceName, QStringLiteral("QindaQt Bus A1 — Speakers"));
    // A physical bus is heard through its target device's monitor.
    QCOMPARE(a1.captureKind, CaptureKind::Monitor);
    QCOMPARE(a1.captureDevice, QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo.monitor"));
    QCOMPARE(a1.gainDb, -3.0);

    const DesiredSource &a2 = sources[1];
    QCOMPARE(a2.sourceName, QStringLiteral("QindaQt Bus A2"));
    QCOMPARE(a2.captureKind, CaptureKind::None);
    QVERIFY(a2.captureDevice.isEmpty());

    const DesiredSource &b1 = sources[2];
    QCOMPARE(b1.sourceName, QStringLiteral("QindaQt Bus B1 — Chat"));
    // Applications, OBS included, record a virtual bus from its `.source`.
    QCOMPARE(b1.captureKind, CaptureKind::Input);
    QCOMPARE(b1.captureDevice, QStringLiteral("qindaqt.console.bus.b1.source"));

    // A label that only restates the code adds nothing to the name.
    QCOMPARE(sources[3].sourceName, QStringLiteral("QindaQt Bus B2"));

    const DesiredSource &virtual1 = sources[4];
    QCOMPARE(virtual1.kind, SourceKind::Strip);
    QCOMPARE(virtual1.code, QStringLiteral("Virtual 1"));
    QCOMPARE(virtual1.sourceName, QStringLiteral("QindaQt Strip Virtual 1 — Music"));
    QCOMPARE(virtual1.captureKind, CaptureKind::Monitor);
    QCOMPARE(virtual1.captureDevice, QStringLiteral("qindaqt.console.strip.virtual.1.monitor"));

    const DesiredSource &hardware1 = sources[5];
    QCOMPARE(hardware1.sourceName, QStringLiteral("QindaQt Strip Hardware 1 — Mic"));
    QCOMPARE(hardware1.captureKind, CaptureKind::Input);
    QCOMPARE(hardware1.captureDevice, QStringLiteral("alsa_input.usb-mic.mono-fallback"));
    QVERIFY(hardware1.muted);
    QCOMPARE(hardware1.gainDb, 2.5);

    QCOMPARE(sources[6].sourceName, QStringLiteral("QindaQt Strip Hardware 2"));
    QCOMPARE(sources[6].captureKind, CaptureKind::None);
}

void ConsoleSourcesTests::namesStayUniqueAndCodesFallBack()
{
    Audio::Snapshot snapshot;
    Audio::Bus named;
    named.id = QStringLiteral("bus.speakers");
    named.kind = Audio::BusKind::Physical;
    named.index = 0;
    named.label = QStringLiteral("Speakers");
    Audio::Bus coded;
    coded.id = QStringLiteral("bus.a1");
    coded.kind = Audio::BusKind::Physical;
    coded.index = 0;
    coded.label = QStringLiteral("Speakers");
    snapshot.console.buses = {named, coded};
    Audio::Strip usb;
    usb.id = QStringLiteral("strip.usb.mic");
    usb.kind = Audio::StripKind::HardwareInput;
    Audio::Strip blank;
    blank.kind = Audio::StripKind::VirtualInput;
    blank.index = 4;
    snapshot.console.strips = {usb, blank};

    const auto sources = desiredSources(snapshot);
    // The blank id is not a source at all; the rest keep distinct names.
    QCOMPARE(sources.size(), 3);
    QCOMPARE(sources[0].code, QStringLiteral("A1"));
    QCOMPARE(sources[0].sourceName, QStringLiteral("QindaQt Bus A1 — Speakers"));
    QCOMPARE(sources[1].sourceName, QStringLiteral("QindaQt Bus A1 — Speakers [bus.a1]"));
    QCOMPARE(sources[2].code, QStringLiteral("Usb Mic"));
    QCOMPARE(sources[2].sourceName, QStringLiteral("QindaQt Strip Usb Mic"));
}

void ConsoleSourcesTests::plansCreationsForAnEmptyObs()
{
    const auto desired = desiredSources(consoleFixture());
    const SyncPlan plan = planSync(desired, {});
    QCOMPARE(plan.creations, desired);
    QVERIFY(plan.removals.isEmpty());
    QVERIFY(plan.renames.isEmpty());
    QVERIFY(plan.retargets.isEmpty());
    QVERIFY(!plan.empty());
}

void ConsoleSourcesTests::plansNothingWhenObsMatches()
{
    const auto desired = desiredSources(consoleFixture());
    QVERIFY(planSync(desired, existingOf(desired)).empty());
}

void ConsoleSourcesTests::plansRenamesRetargetsAndRemovals()
{
    Audio::Snapshot snapshot = consoleFixture();
    const auto before = desiredSources(snapshot);
    const auto existing = existingOf(before);

    snapshot.console.buses[2].label = QStringLiteral("Game");
    snapshot.console.buses[0].targetSerial = 102;
    snapshot.console.buses.removeAt(3);
    snapshot.console.strips[2].sourceEpoch = 7;
    snapshot.console.strips[2].sourceSerial = 202;
    snapshot.console.strips[2].sourceKnown = true;
    const auto after = desiredSources(snapshot);

    const SyncPlan plan = planSync(after, existing);
    QCOMPARE(plan.removals, QList<QString>{QStringLiteral("bus.b2")});
    QCOMPARE(plan.renames.size(), 1);
    QCOMPARE(plan.renames[0].consoleId, QStringLiteral("bus.b1"));
    QCOMPARE(plan.renames[0].from, QStringLiteral("QindaQt Bus B1 — Chat"));
    QCOMPARE(plan.renames[0].to, QStringLiteral("QindaQt Bus B1 — Game"));
    QCOMPARE(plan.retargets.size(), 2);
    QCOMPARE(plan.retargets[0].consoleId, QStringLiteral("bus.a1"));
    QCOMPARE(plan.retargets[0].captureDevice,
             QStringLiteral("alsa_output.usb-dac.analog-stereo.monitor"));
    QCOMPARE(plan.retargets[1].consoleId, QStringLiteral("strip.hardware.2"));
    QCOMPARE(plan.retargets[1].captureKind, CaptureKind::Input);
    QVERIFY(plan.creations.isEmpty());
}

void ConsoleSourcesTests::plansDuplicateAndKindMismatchRepairs()
{
    const auto desired = desiredSources(consoleFixture());
    QList<ExistingSource> existing = existingOf(desired);
    // A second source claiming bus.b1 (a hand copy) is removed by name.
    ExistingSource duplicate = existing[2];
    duplicate.sourceName = QStringLiteral("QindaQt Bus B1 — Chat 2");
    existing.append(duplicate);
    // A strip that exists as a bus-typed source cannot be updated in place.
    existing[4].kind = SourceKind::Bus;

    const SyncPlan plan = planSync(desired, existing);
    QCOMPARE(plan.removals,
             QList<QString>({QStringLiteral("bus.b1#QindaQt Bus B1 — Chat 2"),
                             QStringLiteral("strip.virtual.1")}));
    QCOMPARE(plan.creations.size(), 1);
    QCOMPARE(plan.creations[0].consoleId, QStringLiteral("strip.virtual.1"));
    QVERIFY(plan.renames.isEmpty());
    QVERIFY(plan.retargets.isEmpty());
}

QTEST_GUILESS_MAIN(ConsoleSourcesTests)
#include "tst_console_sources.moc"
