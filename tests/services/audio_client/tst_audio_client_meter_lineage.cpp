// SPDX-License-Identifier: GPL-3.0-or-later

// Meter readings and snapshot lineage (ADR-0174, Audio1 reference "Lineage and
// completion"). The service folds every Levels batch into the snapshot it
// serves without a new revision. During graph churn a second Changed arrives
// while a fetch is in flight, the client fetches again, and the service
// answers the SAME revision with moved meters. That refetch used to be judged
// a contradiction: the client went Unavailable/malformed-snapshot and backed
// off until the next revision, which is how the shell applet "stopped working
// when audio changed".

#include "support/fake_audio_transport.h"

#include <qindaqt/services/audio_client/audio_client.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

namespace
{

constexpr auto kOwner = ":1.50";

Level reading(const double peakDb, const double rmsDb)
{
    return meterReading(QStringLiteral("unused"), peakDb, rmsDb).level;
}

// A client holding revision 2, then revision 4 from one fetch while a second
// invalidation was coalesced behind it; the refetch is now in flight.
struct ChurnedClient {
    FakeAudioTransport transport;
    AudioClient client{&transport};
    Snapshot published = consoleClientSnapshot(11, 4);

    ChurnedClient()
    {
        const QString owner = QString::fromLatin1(kOwner);
        client.start();
        transport.announceOwner(owner);
        transport.reply(transport.fetches.last(), consoleClientSnapshot(11, 2));
        transport.invalidate(owner, 11, 3);
        transport.invalidate(owner, 11, 4); // coalesced behind the fetch in flight
        published.console.strips[0].level = reading(-12, -20);
        transport.reply(transport.fetches.last(), published);
    }
};

} // namespace

class AudioClientMeterLineageTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void aSameRevisionRefetchWithMovedMetersStaysReady();
    void readingsAppliedFromTheStreamAreNotAContradiction();
    void idledReadingsAfterTheLastReleaseAreNotAContradiction();
    void anyOtherSameRevisionDifferenceIsStillAContradiction();
};

void AudioClientMeterLineageTests::initTestCase()
{
    qRegisterMetaType<QList<LevelReading>>();
}

void AudioClientMeterLineageTests::aSameRevisionRefetchWithMovedMetersStaysReady()
{
    ChurnedClient churned;
    QSignalSpy snapshots(&churned.client, &AudioClient::snapshotChanged);
    QCOMPARE(churned.client.state(), ClientState::Ready);
    QCOMPARE(churned.transport.fetches.size(), 3);

    Snapshot moved = churned.published;
    moved.console.strips[0].level = reading(-9, -15);
    moved.console.strips[1].level = reading(-30, -40);
    churned.transport.reply(churned.transport.fetches.last(), moved);

    QCOMPARE(churned.client.state(), ClientState::Ready);
    QVERIFY(churned.client.reasonCode() != QStringLiteral("malformed-snapshot"));
    QVERIFY(!churned.client.retryTimerForTesting()->isActive());
    QCOMPARE(churned.client.snapshot().revision, quint64(4));
    // The same publication: nothing new to announce to the surfaces.
    QCOMPARE(snapshots.size(), 0);
}

// The client rewrites the same fields itself: frames while a lease is held,
// idle readings on the last release. Its retained copy then differs from what
// the service serves even when the service's readings never moved.
void AudioClientMeterLineageTests::readingsAppliedFromTheStreamAreNotAContradiction()
{
    ChurnedClient churned;
    churned.client.retainLevels();
    churned.transport.sendLevels(QString::fromLatin1(kOwner),
                                 {meterReading(QStringLiteral("strip.hardware.1"), -3, -6)});
    QCOMPARE(churned.client.snapshot().console.strips.at(0).level.peakDb, -3.0);

    churned.transport.reply(churned.transport.fetches.last(), churned.published);
    QCOMPARE(churned.client.state(), ClientState::Ready);
    // While a lease is held the stream stays the meter's source of truth.
    QCOMPARE(churned.client.snapshot().console.strips.at(0).level.peakDb, -3.0);
}

void AudioClientMeterLineageTests::idledReadingsAfterTheLastReleaseAreNotAContradiction()
{
    ChurnedClient churned;
    churned.client.retainLevels();
    churned.client.releaseLevels();
    QVERIFY(!churned.client.snapshot().console.strips.at(0).level.known);

    churned.transport.reply(churned.transport.fetches.last(), churned.published);
    QCOMPARE(churned.client.state(), ClientState::Ready);
    QVERIFY(!churned.client.snapshot().console.strips.at(0).level.known);
}

void AudioClientMeterLineageTests::anyOtherSameRevisionDifferenceIsStillAContradiction()
{
    ChurnedClient churned;
    QSignalSpy snapshots(&churned.client, &AudioClient::snapshotChanged);
    Snapshot contradiction = churned.published;
    contradiction.console.strips[0].level = reading(-9, -15);
    contradiction.console.strips[0].gainDb = -6.0;
    churned.transport.reply(churned.transport.fetches.last(), contradiction);

    QCOMPARE(churned.client.state(), ClientState::Unavailable);
    QCOMPARE(churned.client.reasonCode(), QStringLiteral("malformed-snapshot"));
    QCOMPARE(churned.client.snapshot().console.strips.at(0).gainDb, 0.0);
    QCOMPARE(snapshots.size(), 0);
}

QTEST_GUILESS_MAIN(AudioClientMeterLineageTests)
#include "tst_audio_client_meter_lineage.moc"
