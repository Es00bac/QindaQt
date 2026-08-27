// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_audio_transport.h"

#include <qindaqt/services/audio_client/audio_client.h>

#include <QtTest>

#include <limits>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

class AudioClientTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bindsExactOwnerAndAcceptsLineage();
    void coalescesInvalidationsAndRejectsRegression();
    void ownerReplacementMakesOperationUncertain();
    void timeoutNeverReplaysOperation();
    void malformedOperationResultBecomesUncertain();
    void rejectsMalformedStaleAndUnsupportedLocally();
};

void AudioClientTests::bindsExactOwnerAndAcceptsLineage()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy snapshots(&client, &AudioClient::snapshotChanged);
    client.start();
    QCOMPARE(transport.startCalls, 1);
    QCOMPARE(client.state(), ClientState::Starting);
    transport.announceOwner(QStringLiteral(":1.20"));
    QCOMPARE(transport.fetches.size(), 1);
    transport.reply(transport.fetches[0], clientSnapshot());
    QCOMPARE(client.state(), ClientState::Ready);
    QCOMPARE(client.owner(), QStringLiteral(":1.20"));
    QCOMPARE(client.snapshot(), clientSnapshot());
    QCOMPARE(snapshots.count(), 1);

    const FakeAudioTransport::Fetch stale{.owner = QStringLiteral(":1.19"),
                                          .requestId = transport.fetches[0].requestId};
    transport.reply(stale, clientSnapshot(99, 99));
    QCOMPARE(client.snapshot().epoch, quint64(11));
}

void AudioClientTests::coalescesInvalidationsAndRejectsRegression()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    client.start();
    transport.announceOwner(QStringLiteral(":1.21"));
    transport.reply(transport.fetches[0], clientSnapshot(11, 5));
    transport.invalidate(QStringLiteral(":1.21"), 11, 6);
    transport.invalidate(QStringLiteral(":1.21"), 11, 7);
    transport.invalidate(QStringLiteral(":1.21"), 11, 8);
    QCOMPARE(transport.fetches.size(), 2);
    transport.reply(transport.fetches[1], clientSnapshot(11, 4));
    QCOMPARE(client.state(), ClientState::Unavailable);
    QTRY_VERIFY(transport.fetches.size() >= 3);
    QCOMPARE(client.snapshot().revision, quint64(5));
}

void AudioClientTests::ownerReplacementMakesOperationUncertain()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.30"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 requestId = client.setMute({.epoch = 11, .serial = 10}, true);
    QCOMPARE(transport.operations.size(), 1);
    const auto oldOperation = transport.operations[0];

    transport.announceOwner(QStringLiteral(":1.31"));
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), requestId);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(transport.fetches.size(), 2);
    transport.finish(oldOperation, successfulResult(oldOperation, 3));
    QCOMPARE(completed.count(), 1);
    transport.reply(transport.fetches[1], clientSnapshot(12, 1));
    QCOMPARE(client.snapshot().epoch, quint64(12));
}

void AudioClientTests::timeoutNeverReplaysOperation()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    client.setRequestTimeout(15);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.40"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 requestId = client.setDefault({.epoch = 11, .serial = 10});
    QVERIFY(requestId != 0);
    QCOMPARE(transport.operations.size(), 1);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(completed[0][1].value<OperationResult>().reasonCode,
             QStringLiteral("operation-timeout"));
    QCOMPARE(transport.operations.size(), 1);
    QVERIFY(transport.fetches.size() >= 2);
}

void AudioClientTests::malformedOperationResultBecomesUncertain()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.50"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 requestId = client.setVolume({.epoch = 11, .serial = 10}, 0.4);
    QVERIFY(requestId != 0);
    OperationResult malformed = successfulResult(transport.operations[0], 3);
    malformed.initiatingRevision = 1;
    transport.finish(transport.operations[0], malformed);
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(completed[0][1].value<OperationResult>().reasonCode,
             QStringLiteral("malformed-result"));
    QCOMPARE(transport.operations.size(), 1);
}

void AudioClientTests::rejectsMalformedStaleAndUnsupportedLocally()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.60"));
    Snapshot snapshot = clientSnapshot();
    snapshot.outputs[0].canSetMute = false;
    transport.reply(transport.fetches[0], snapshot);

    QVERIFY(client.setMute({.epoch = 11, .serial = 999}, true) != 0);
    QCOMPARE(completed.constLast()[1].value<OperationResult>().reasonCode,
             QStringLiteral("stale-handle"));
    QVERIFY(client.setVolume({.epoch = 11, .serial = 10},
                             std::numeric_limits<double>::quiet_NaN())
            != 0);
    QCOMPARE(completed.constLast()[1].value<OperationResult>().reasonCode,
             QStringLiteral("invalid-volume"));
    QVERIFY(client.setMute({.epoch = 11, .serial = 10}, true) != 0);
    QCOMPARE(completed.constLast()[1].value<OperationResult>().status,
             OperationStatus::Unsupported);
    QVERIFY(transport.operations.isEmpty());
}

QTEST_GUILESS_MAIN(AudioClientTests)
#include "tst_audio_client.moc"
