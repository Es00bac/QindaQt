// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_audio_transport.h"

#include <qindaqt/services/audio_client/audio_client.h>
#include <qindaqt/services/audio_client/retry_backoff.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

namespace
{

constexpr auto kOwner = ":1.40";

// QTimer::timeout carries a QPrivateSignal, so it cannot be emitted from
// outside; the meta-object call is the supported way to fire a timer in a test
// without sleeping through its interval.
void fireRetry(QTimer *timer)
{
    QVERIFY2(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection),
             "the retry timer has no timeout signal to fire");
}

// A started client holding a console snapshot from a ready owner.
struct ReadyClient {
    FakeAudioTransport transport;
    AudioClient client{&transport};

    ReadyClient()
    {
        client.start();
        transport.announceOwner(QString::fromLatin1(kOwner));
        transport.reply(transport.fetches.last(), consoleClientSnapshot());
    }
};

} // namespace

// AGENT-CONTRACT (ADR-0394): the meter stream is subscribed only while a
// surface holds a lease, and a client that cannot get a snapshot backs off
// instead of retrying five times a second forever.
class AudioClientLevelsTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void subscriptionFollowsTheLeaseCount();
    void framesAreAppliedOnlyWhileLeased();
    void releasingTheLastLeaseIdlesEveryMeter();
    void leasesSurviveStopAndStart();
    void raiiLeaseReleasesOnceAndMoves();
    void backoffScheduleDoublesToItsCeiling();
    void snapshotRetriesBackOffAndAHealthyAnswerResets();
    void aNewOwnerStartsTheBackoffOver();
};

void AudioClientLevelsTests::initTestCase()
{
    // Declared by audio_console.h; registered so QSignalSpy can store frames.
    qRegisterMetaType<QList<QindaQt::Audio::LevelReading>>();
}

void AudioClientLevelsTests::subscriptionFollowsTheLeaseCount()
{
    ReadyClient ready;
    // Nobody is looking at a meter: the stream is not requested at all.
    QVERIFY(ready.transport.levelsRequests.isEmpty());
    QVERIFY(!ready.transport.levelsSubscribed);
    QCOMPARE(ready.client.levelsLeaseCount(), 0);

    ready.client.retainLevels();
    QCOMPARE(ready.transport.levelsRequests, QList<bool>{true});
    ready.client.retainLevels();
    QCOMPARE(ready.client.levelsLeaseCount(), 2);
    QCOMPARE(ready.transport.levelsRequests, QList<bool>{true}); // counted, not repeated

    ready.client.releaseLevels();
    QCOMPARE(ready.client.levelsLeaseCount(), 1);
    QVERIFY(ready.transport.levelsSubscribed);
    ready.client.releaseLevels();
    QCOMPARE(ready.client.levelsLeaseCount(), 0);
    QCOMPARE(ready.transport.levelsRequests, (QList<bool>{true, false}));

    // An unmatched release is ignored; it can neither go negative nor send
    // a second "off".
    ready.client.releaseLevels();
    QCOMPARE(ready.client.levelsLeaseCount(), 0);
    QCOMPARE(ready.transport.levelsRequests, (QList<bool>{true, false}));

    // A later lease subscribes again.
    ready.client.retainLevels();
    QCOMPARE(ready.transport.levelsRequests, (QList<bool>{true, false, true}));
}

void AudioClientLevelsTests::framesAreAppliedOnlyWhileLeased()
{
    ReadyClient ready;
    QSignalSpy frames(&ready.client, &AudioClient::levelsChanged);
    const QString owner = QString::fromLatin1(kOwner);

    // Without a lease a frame (one already queued, say) is not decoded or
    // folded into the snapshot.
    ready.transport.sendLevels(owner, {meterReading(QStringLiteral("strip.hardware.1"), -12, -20)});
    QCOMPARE(frames.size(), 0);
    QVERIFY(!ready.client.snapshot().console.strips.at(0).level.known);

    ready.client.retainLevels();
    ready.transport.sendLevels(owner, {meterReading(QStringLiteral("strip.hardware.1"), -12, -20)});
    QCOMPARE(frames.size(), 1);
    QVERIFY(ready.client.snapshot().console.strips.at(0).level.known);
    QCOMPARE(ready.client.snapshot().console.strips.at(0).level.peakDb, -12.0);
    // A reading for an id the console does not have is dropped, a frame from
    // another owner is dropped: the lease changes neither rule.
    ready.transport.sendLevels(owner, {meterReading(QStringLiteral("no.such.strip"), -1, -2)});
    ready.transport.sendLevels(QStringLiteral(":1.99"),
                               {meterReading(QStringLiteral("strip.hardware.2"), -3, -4)});
    QCOMPARE(frames.size(), 1);
}

void AudioClientLevelsTests::releasingTheLastLeaseIdlesEveryMeter()
{
    ReadyClient ready;
    QSignalSpy frames(&ready.client, &AudioClient::levelsChanged);
    const QString owner = QString::fromLatin1(kOwner);
    ready.client.retainLevels();
    ready.transport.sendLevels(owner, {meterReading(QStringLiteral("strip.hardware.1"), -6, -12),
                                       meterReading(QStringLiteral("strip.hardware.2"), -9, -15)});
    QCOMPARE(frames.size(), 1);

    // Two surfaces; the first leaves and the meters stay live for the second.
    ready.client.retainLevels();
    ready.client.releaseLevels();
    QCOMPARE(frames.size(), 1);
    QVERIFY(ready.client.snapshot().console.strips.at(1).level.known);

    // The last one leaves: one empty batch, the same signal an upstream
    // "metering stopped" gives, and the retained snapshot goes idle so a later
    // lease cannot start from a frozen reading.
    ready.client.releaseLevels();
    QCOMPARE(frames.size(), 2);
    const auto batch = frames.last().at(0).value<QList<LevelReading>>();
    QVERIFY(batch.isEmpty());
    for (const Strip &strip : ready.client.snapshot().console.strips) {
        QVERIFY(!strip.level.known);
    }
    // A frame still in flight after the release is not applied.
    ready.transport.sendLevels(owner, {meterReading(QStringLiteral("strip.hardware.1"), -6, -12)});
    QCOMPARE(frames.size(), 2);
    QVERIFY(!ready.client.snapshot().console.strips.at(0).level.known);
}

void AudioClientLevelsTests::leasesSurviveStopAndStart()
{
    ReadyClient ready;
    ready.client.retainLevels();
    QCOMPARE(ready.transport.levelsRequests, QList<bool>{true});
    // stop()/start() recreate the owner binding; the transport keeps the
    // standing want and re-applies it to the next owner, so the count must not
    // be reset or re-announced by the client.
    ready.client.stop();
    QCOMPARE(ready.client.levelsLeaseCount(), 1);
    ready.client.start();
    ready.transport.announceOwner(QStringLiteral(":1.41"));
    ready.transport.reply(ready.transport.fetches.last(), consoleClientSnapshot(12, 1));
    QCOMPARE(ready.client.levelsLeaseCount(), 1);
    QCOMPARE(ready.transport.levelsRequests, QList<bool>{true});
    ready.client.releaseLevels();
    QCOMPARE(ready.transport.levelsRequests, (QList<bool>{true, false}));
}

void AudioClientLevelsTests::raiiLeaseReleasesOnceAndMoves()
{
    ReadyClient ready;
    {
        LevelsLease first(&ready.client);
        QVERIFY(first.held());
        QCOMPARE(ready.client.levelsLeaseCount(), 1);
        LevelsLease moved(std::move(first));
        QVERIFY(!first.held());
        QVERIFY(moved.held());
        QCOMPARE(ready.client.levelsLeaseCount(), 1);
        LevelsLease second(&ready.client);
        QCOMPARE(ready.client.levelsLeaseCount(), 2);
        second = std::move(moved); // releases second's own lease, takes moved's
        QCOMPARE(ready.client.levelsLeaseCount(), 1);
        second.release();
        second.release();
        QCOMPARE(ready.client.levelsLeaseCount(), 0);
    }
    QCOMPARE(ready.transport.levelsRequests, (QList<bool>{true, false}));
    const LevelsLease detached(nullptr);
    QVERIFY(!detached.held());
}

void AudioClientLevelsTests::backoffScheduleDoublesToItsCeiling()
{
    RetryBackoff backoff; // 0.2 s doubling to 10 s, the OBS bridge's schedule
    QCOMPARE(backoff.initial(), 200);
    QCOMPARE(backoff.maximum(), 10'000);
    QList<int> delays;
    for (int step = 0; step < 9; ++step) {
        delays.append(backoff.take());
    }
    QCOMPARE(delays, (QList<int>{200, 400, 800, 1600, 3200, 6400, 10000, 10000, 10000}));
    QCOMPARE(backoff.peek(), 10000);
    backoff.reset();
    QCOMPARE(backoff.take(), 200);
    QCOMPARE(backoff.take(), 400);

    // Degenerate bounds are clamped rather than looping or going negative.
    RetryBackoff odd(0, 5);
    QCOMPARE(odd.initial(), 1);
    QCOMPARE(odd.take(), 1);
    RetryBackoff tight(300, 100);
    QCOMPARE(tight.maximum(), 300);
    QCOMPARE(tight.take(), 300);
    QCOMPARE(tight.take(), 300);
}

void AudioClientLevelsTests::snapshotRetriesBackOffAndAHealthyAnswerResets()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    client.start();
    transport.announceOwner(QString::fromLatin1(kOwner));
    QTimer *retry = client.retryTimerForTesting();

    // The service keeps refusing: each rejected snapshot schedules the next
    // retry twice as far away, up to the ceiling. The test fires the retry
    // timer itself instead of sleeping through the schedule.
    QList<int> scheduled;
    for (int attempt = 0; attempt < 9; ++attempt) {
        transport.fail(transport.fetches.last(), QStringLiteral("transport-timeout"));
        QVERIFY(retry->isActive());
        scheduled.append(client.pendingRetryDelayMilliseconds());
        retry->stop();
        fireRetry(retry); // the retry fires: a new fetch is issued
    }
    QCOMPARE(scheduled, (QList<int>{200, 400, 800, 1600, 3200, 6400, 10000, 10000, 10000}));

    // A coherent answer resets the schedule.
    transport.reply(transport.fetches.last(), clientSnapshot());
    QCOMPARE(client.state(), ClientState::Ready);
    transport.invalidate(QString::fromLatin1(kOwner), 11, 99);
    transport.fail(transport.fetches.last(), QStringLiteral("transport-timeout"));
    QVERIFY(retry->isActive());
    QCOMPARE(client.pendingRetryDelayMilliseconds(), 200);
}

void AudioClientLevelsTests::aNewOwnerStartsTheBackoffOver()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    client.start();
    transport.announceOwner(QString::fromLatin1(kOwner));
    QTimer *retry = client.retryTimerForTesting();
    for (int attempt = 0; attempt < 4; ++attempt) {
        transport.fail(transport.fetches.last(), QStringLiteral("transport-timeout"));
        retry->stop();
        fireRetry(retry);
    }
    // The next retry would wait 3200 ms against this owner.
    transport.fail(transport.fetches.last(), QStringLiteral("transport-timeout"));
    QCOMPARE(client.pendingRetryDelayMilliseconds(), 3200);
    retry->stop();

    // A replacement service is a new authority: nothing learned about the old
    // one applies, so its first failure retries quickly.
    transport.announceOwner(QStringLiteral(":1.77"));
    transport.fail(transport.fetches.last(), QStringLiteral("transport-timeout"));
    QVERIFY(retry->isActive());
    QCOMPARE(client.pendingRetryDelayMilliseconds(), 200);
}

QTEST_GUILESS_MAIN(AudioClientLevelsTests)
#include "tst_audio_client_levels.moc"
