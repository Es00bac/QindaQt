// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0173: the mixing console's own state. These rows pin the behaviour that
// makes it a console rather than a volume mixer - a rectangular routing matrix
// with per-send gain, solo that silences without tearing the graph down, and a
// configuration that survives the devices behind it disappearing.

#include <qindaqt/services/audio_service/console_model.h>

#include <qindaqt/services/audio_protocol/audio_gain.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtTest>

using namespace QindaQt::Audio;

namespace {

OperationRequest strip(OperationKind kind, const QString &id)
{
    OperationRequest request;
    request.kind = kind;
    request.consoleId = id;
    return request;
}

} // namespace

class ConsoleModelTests final : public QObject
{
    Q_OBJECT
private slots:
    void theDefaultLayoutIsTheReferenceConsole();
    void everyPublishedConsolePassesTheAdmissionGate();
    void faderMuteSoloMonoAndPanApply();
    void outOfRangeValuesAreRejectedAndChangeNothing();
    void unknownStripsAndBusesAreRejected();
    void routingCarriesPerSendGain();
    void soloSilencesWithoutTearingRoutingDown();
    void aDisabledSendKeepsItsGain();
    void meterBatchesApplyClearAndRejectStrangers();
    void persistenceRoundTripsTheUsersDecisions();
    void aCorruptDocumentLoadsWhatItCan();
};

void ConsoleModelTests::theDefaultLayoutIsTheReferenceConsole()
{
    const ConsoleModel model;
    const Console console = model.console();
    // 5 hardware + 3 virtual input strips, 5 physical + 3 virtual output buses.
    QCOMPARE(console.strips.size(), 8);
    QCOMPARE(console.buses.size(), 8);
    int hardware = 0;
    int virtualStrips = 0;
    for (const Strip &s : console.strips) {
        (s.kind == StripKind::HardwareInput ? hardware : virtualStrips)++;
    }
    QCOMPARE(hardware, 5);
    QCOMPARE(virtualStrips, 3);
    QCOMPARE(console.buses.at(0).label, QStringLiteral("A1"));
    QCOMPARE(console.buses.at(4).label, QStringLiteral("A5"));
    QCOMPARE(console.buses.at(5).label, QStringLiteral("B1"));
    QCOMPARE(console.buses.at(7).label, QStringLiteral("B3"));
    // The matrix is rectangular: every strip carries a cell for every bus, so a
    // client can index it without asking which cells exist.
    for (const Strip &s : console.strips) {
        QCOMPARE(s.sends.size(), console.buses.size());
    }
    QCOMPARE(console.soloActive, false);
}

void ConsoleModelTests::everyPublishedConsolePassesTheAdmissionGate()
{
    ConsoleModel model;
    QVERIFY(validateConsole(model.console()).accepted);
    auto request = strip(OperationKind::SetStripSolo, QStringLiteral("strip.hw.1"));
    request.enabled = true;
    QString reason;
    QVERIFY(model.apply(request, &reason));
    // soloActive is derived, so it can never disagree with the strips and trip
    // the gate's consistency check.
    QVERIFY(model.console().soloActive);
    QVERIFY2(validateConsole(model.console()).accepted,
             qPrintable(validateConsole(model.console()).reasonCode));
}

void ConsoleModelTests::faderMuteSoloMonoAndPanApply()
{
    ConsoleModel model;
    QString reason;
    auto gain = strip(OperationKind::SetStripGain, QStringLiteral("strip.hw.2"));
    gain.gainDb = -6.0;
    QVERIFY(model.apply(gain, &reason));
    auto mute = strip(OperationKind::SetStripMute, QStringLiteral("strip.hw.2"));
    mute.muted = true;
    QVERIFY(model.apply(mute, &reason));
    auto mono = strip(OperationKind::SetStripMono, QStringLiteral("strip.hw.2"));
    mono.enabled = true;
    QVERIFY(model.apply(mono, &reason));
    auto pan = strip(OperationKind::SetStripPan, QStringLiteral("strip.hw.2"));
    pan.pan = -0.5;
    QVERIFY(model.apply(pan, &reason));

    // The console is a value; bind it, not a reference into a temporary.
    const Console console = model.console();
    const Strip &s = console.strips.at(1);
    QCOMPARE(s.gainDb, -6.0);
    QVERIFY(s.muted);
    QVERIFY(s.mono);
    QCOMPARE(s.pan, -0.5);

    auto busGain = strip(OperationKind::SetBusGain, QStringLiteral("bus.a1"));
    busGain.gainDb = 3.0;
    QVERIFY(model.apply(busGain, &reason));
    QCOMPARE(model.console().buses.at(0).gainDb, 3.0);
}

void ConsoleModelTests::outOfRangeValuesAreRejectedAndChangeNothing()
{
    ConsoleModel model;
    QString reason;
    auto tooLoud = strip(OperationKind::SetStripGain, QStringLiteral("strip.hw.1"));
    tooLoud.gainDb = kMaxGainDb + 1.0;
    QVERIFY(!model.apply(tooLoud, &reason));
    QCOMPARE(reason, QStringLiteral("gain-out-of-range"));
    QCOMPARE(model.console().strips.constFirst().gainDb, 0.0);

    auto nan = strip(OperationKind::SetStripGain, QStringLiteral("strip.hw.1"));
    nan.gainDb = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!model.apply(nan, &reason));
    QCOMPARE(model.console().strips.constFirst().gainDb, 0.0);

    auto wildPan = strip(OperationKind::SetStripPan, QStringLiteral("strip.hw.1"));
    wildPan.pan = 4.0;
    QVERIFY(!model.apply(wildPan, &reason));
    QCOMPARE(reason, QStringLiteral("pan-out-of-range"));
    QCOMPARE(model.console().strips.constFirst().pan, 0.0);
}

void ConsoleModelTests::unknownStripsAndBusesAreRejected()
{
    ConsoleModel model;
    QString reason;
    auto ghost = strip(OperationKind::SetStripGain, QStringLiteral("strip.nope"));
    ghost.gainDb = -3.0;
    QVERIFY(!model.apply(ghost, &reason));
    QCOMPARE(reason, QStringLiteral("unknown-strip"));

    auto send = strip(OperationKind::SetStripSend, QStringLiteral("strip.hw.1"));
    send.busIndex = 99;
    send.enabled = true;
    QVERIFY(!model.apply(send, &reason));
    QCOMPARE(reason, QStringLiteral("unknown-bus"));
}

void ConsoleModelTests::routingCarriesPerSendGain()
{
    ConsoleModel model;
    QString reason;
    QVERIFY(model.routing().isEmpty());

    // One strip to two buses at different levels: this is the whole point of a
    // matrix, as opposed to a single output selector.
    auto toA1 = strip(OperationKind::SetStripSend, QStringLiteral("strip.virtual.1"));
    toA1.busIndex = 0;
    toA1.enabled = true;
    toA1.gainDb = 0.0;
    QVERIFY(model.apply(toA1, &reason));
    auto toB1 = strip(OperationKind::SetStripSend, QStringLiteral("strip.virtual.1"));
    toB1.busIndex = 5;
    toB1.enabled = true;
    toB1.gainDb = -12.0;
    QVERIFY(model.apply(toB1, &reason));

    const QList<ConsoleModel::RoutingEdge> routing = model.routing();
    QCOMPARE(routing.size(), 2);
    QCOMPARE(routing.at(0).busId, QStringLiteral("bus.a1"));
    QCOMPARE(routing.at(0).gainDb, 0.0);
    QCOMPARE(routing.at(1).busId, QStringLiteral("bus.b1"));
    QCOMPARE(routing.at(1).gainDb, -12.0);
    QVERIFY(routing.at(0).audible);
}

void ConsoleModelTests::soloSilencesWithoutTearingRoutingDown()
{
    ConsoleModel model;
    QString reason;
    for (const QString &id : {QStringLiteral("strip.hw.1"), QStringLiteral("strip.hw.2")}) {
        auto send = strip(OperationKind::SetStripSend, id);
        send.busIndex = 0;
        send.enabled = true;
        QVERIFY(model.apply(send, &reason));
    }
    QCOMPARE(model.routing().size(), 2);

    auto solo = strip(OperationKind::SetStripSolo, QStringLiteral("strip.hw.2"));
    solo.enabled = true;
    QVERIFY(model.apply(solo, &reason));

    // AGENT-GUARD: both edges REMAIN. Solo silences, it does not unroute; a
    // console that tore the graph down here would take audible time to come
    // back and would lose the mix on unsolo.
    const QList<ConsoleModel::RoutingEdge> routing = model.routing();
    QCOMPARE(routing.size(), 2);
    QCOMPARE(routing.at(0).stripId, QStringLiteral("strip.hw.1"));
    QCOMPARE(routing.at(0).audible, false);
    QCOMPARE(routing.at(1).stripId, QStringLiteral("strip.hw.2"));
    QCOMPARE(routing.at(1).audible, true);

    // Muting the destination bus silences every edge into it.
    auto busMute = strip(OperationKind::SetBusMute, QStringLiteral("bus.a1"));
    busMute.muted = true;
    QVERIFY(model.apply(busMute, &reason));
    const QList<ConsoleModel::RoutingEdge> silenced = model.routing();
    for (const ConsoleModel::RoutingEdge &edge : silenced) {
        QCOMPARE(edge.audible, false);
    }
}

void ConsoleModelTests::aDisabledSendKeepsItsGain()
{
    ConsoleModel model;
    QString reason;
    auto on = strip(OperationKind::SetStripSend, QStringLiteral("strip.hw.1"));
    on.busIndex = 2;
    on.enabled = true;
    on.gainDb = -9.0;
    QVERIFY(model.apply(on, &reason));

    auto off = on;
    off.enabled = false;
    QVERIFY(model.apply(off, &reason));
    QVERIFY(model.routing().isEmpty());
    // The level the user dialled in is still there, so re-enabling restores the
    // mix instead of slamming the send to unity.
    QCOMPARE(model.console().strips.constFirst().sends.at(2).gainDb, -9.0);
}

void ConsoleModelTests::persistenceRoundTripsTheUsersDecisions()
{
    ConsoleModel model;
    QString reason;
    auto gain = strip(OperationKind::SetStripGain, QStringLiteral("strip.virtual.2"));
    gain.gainDb = -4.5;
    QVERIFY(model.apply(gain, &reason));
    auto send = strip(OperationKind::SetStripSend, QStringLiteral("strip.virtual.2"));
    send.busIndex = 6;
    send.enabled = true;
    send.gainDb = -2.0;
    QVERIFY(model.apply(send, &reason));
    auto busMute = strip(OperationKind::SetBusMute, QStringLiteral("bus.a3"));
    busMute.muted = true;
    QVERIFY(model.apply(busMute, &reason));

    ConsoleModel restored;
    restored.loadJson(model.toJson());
    QCOMPARE(restored.console().strips.at(6).gainDb, -4.5);
    QCOMPARE(restored.console().strips.at(6).sends.at(6).enabled, true);
    QCOMPARE(restored.console().strips.at(6).sends.at(6).gainDb, -2.0);
    QCOMPARE(restored.console().buses.at(2).muted, true);
    QCOMPARE(restored.routing().size(), 1);
}

void ConsoleModelTests::aCorruptDocumentLoadsWhatItCan()
{
    ConsoleModel model;
    // A console that refuses to start because one entry is corrupt is worse
    // than one that comes back with that entry at its default.
    const QJsonObject document{
        {QStringLiteral("strips"),
         QJsonArray{
             QJsonObject{{QStringLiteral("id"), QStringLiteral("strip.hw.1")},
                         {QStringLiteral("gainDb"), 1.0e9}},
             QJsonObject{{QStringLiteral("id"), QStringLiteral("strip.nonexistent")},
                         {QStringLiteral("gainDb"), -3.0}},
             QJsonObject{{QStringLiteral("id"), QStringLiteral("strip.hw.3")},
                         {QStringLiteral("gainDb"), -7.5}},
         }},
    };
    model.loadJson(document);
    // The absurd gain fell back to the default, the unknown strip was skipped,
    // and the valid one loaded.
    QCOMPARE(model.console().strips.at(0).gainDb, 0.0);
    QCOMPARE(model.console().strips.at(2).gainDb, -7.5);
    QVERIFY(validateConsole(model.console()).accepted);
}

// ADR-0174. Meters arrive as whole batches at frame rate, so the batch itself
// is the unit that has to behave: it applies to whatever it names, it rejects
// anything it should not have named, and an empty batch means metering stopped.
void ConsoleModelTests::meterBatchesApplyClearAndRejectStrangers()
{
    ConsoleModel model;
    const QString stripId = model.console().strips.at(0).id;
    const QString busId = model.console().buses.at(0).id;

    QVERIFY(model.publishLevels(
        {LevelReading{stripId, Level{.peakDb = -6.0, .rmsDb = -12.0, .known = true}},
         LevelReading{busId, Level{.peakDb = -2.0, .rmsDb = -9.0, .known = true}},
         // Names nothing this console publishes: a meter must never bring an
         // element into existence.
         LevelReading{QStringLiteral("strip.invented"),
                      Level{.peakDb = -1.0, .rmsDb = -1.0, .known = true}}}));
    Console console = model.console();
    QCOMPARE(console.strips.at(0).level.peakDb, -6.0);
    QCOMPARE(console.strips.at(0).level.rmsDb, -12.0);
    QVERIFY(console.strips.at(0).level.known);
    QCOMPARE(console.buses.at(0).level.peakDb, -2.0);
    QCOMPARE(console.strips.size(), model.console().strips.size());
    QVERIFY(validateConsole(console).accepted);

    // An identical batch changes nothing, which is what lets the service skip
    // resending a still console.
    QVERIFY(!model.publishLevels(
        {LevelReading{stripId, Level{.peakDb = -6.0, .rmsDb = -12.0, .known = true}},
         LevelReading{busId, Level{.peakDb = -2.0, .rmsDb = -9.0, .known = true}}}));

    // An RMS above its own peak is impossible and is refused outright rather
    // than clamped, so a malformed producer cannot move the meter at all.
    QVERIFY(!model.publishLevels(
        {LevelReading{stripId, Level{.peakDb = -20.0, .rmsDb = -3.0, .known = true}}}));
    QCOMPARE(model.console().strips.at(0).level.peakDb, -6.0);

    // Empty batch: metering stopped, so every meter goes back to unknown
    // instead of freezing on its last reading.
    QVERIFY(model.publishLevels({}));
    console = model.console();
    QVERIFY(!console.strips.at(0).level.known);
    QVERIFY(!console.buses.at(0).level.known);
    QCOMPARE(console.strips.at(0).level.peakDb, kSilentMeterDb);
    QVERIFY(!model.publishLevels({}));
}

QTEST_APPLESS_MAIN(ConsoleModelTests)
#include "tst_console_model.moc"
