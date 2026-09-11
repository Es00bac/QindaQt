// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_audio_transport.h"

#include <qindaqt/services/audio_client/audio_client.h>

#include <QtTest>

#include <limits>
#include <memory>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

namespace
{

enum class LocalScenario {
    Unavailable,
    StaleDefault,
    InvalidVolume,
    UnsupportedMute,
    IncompatibleMove,
    ChannelCountMismatch,
    ChannelBadLevel,
    ChannelUnsupportedCapability,
    CreateBadName,
    CreateBadChannels,
    CreateUnsupportedCapability,
    RemoveNonVirtual,
    RemoveStale,
};

quint64 invokeLocalScenario(AudioClient &client, const LocalScenario scenario)
{
    switch (scenario) {
    case LocalScenario::Unavailable:
        return client.setDefault({.epoch = 11, .serial = 10});
    case LocalScenario::StaleDefault:
        return client.setDefault({.epoch = 11, .serial = 999});
    case LocalScenario::InvalidVolume:
        return client.setVolume({.epoch = 11, .serial = 10},
                                std::numeric_limits<double>::quiet_NaN());
    case LocalScenario::UnsupportedMute:
        return client.setMute({.epoch = 11, .serial = 10}, true);
    case LocalScenario::IncompatibleMove:
        return client.moveStream({.epoch = 11, .serial = 30},
                                 {.epoch = 11, .serial = 20});
    case LocalScenario::ChannelCountMismatch:
        return client.setChannelVolumes({.epoch = 11, .serial = 10}, {0.1});
    case LocalScenario::ChannelBadLevel:
        return client.setChannelVolumes({.epoch = 11, .serial = 10}, {0.1, 1.5});
    case LocalScenario::ChannelUnsupportedCapability:
        return client.setChannelVolumes({.epoch = 11, .serial = 30}, {0.1, 0.2});
    case LocalScenario::CreateBadName:
        return client.createVirtualDevice(DeviceKind::Output, QString(), 2);
    case LocalScenario::CreateBadChannels:
        return client.createVirtualDevice(DeviceKind::Output,
                                          QStringLiteral("Studio Bus"), 3);
    case LocalScenario::CreateUnsupportedCapability:
        return client.createVirtualDevice(DeviceKind::Output,
                                          QStringLiteral("Studio Bus"), 8);
    case LocalScenario::RemoveNonVirtual:
        return client.removeVirtualDevice({.epoch = 11, .serial = 10});
    case LocalScenario::RemoveStale:
        return client.removeVirtualDevice({.epoch = 11, .serial = 999});
    }
    return 0;
}

} // namespace

Q_DECLARE_METATYPE(LocalScenario)

class AudioClientTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bindsExactOwnerAndAcceptsLineage();
    void coalescesInvalidationsAndRejectsRegression();
    void duplicateAndContradictorySnapshotLineage();
    void ownerReplacementMakesOperationUncertain();
    void timeoutNeverReplaysOperation();
    void transportResultsAreQueued_data();
    void transportResultsAreQueued();
    void synchronousTransportResultIsStillQueued_data();
    void synchronousTransportResultIsStillQueued();
    void malformedOperationResultBecomesUncertain();
    void oldEpochSuccessBecomesUncertain();
    void localCompletionsAreQueued_data();
    void localCompletionsAreQueued();
    void channelVolumeOperationReachesTransport();
    void virtualDeviceOperationsReachTransport();
    void busyCompletionIsQueued();
    void stopCompletionAndCancellationAreLifetimeSafe();
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

void AudioClientTests::duplicateAndContradictorySnapshotLineage()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy snapshots(&client, &AudioClient::snapshotChanged);
    client.start();
    transport.announceOwner(QStringLiteral(":1.22"));
    const Snapshot original = clientSnapshot(11, 5);
    transport.reply(transport.fetches[0], original);
    QCOMPARE(snapshots.count(), 1);

    transport.invalidate(QStringLiteral(":1.22"), 11, 6);
    QCOMPARE(transport.fetches.size(), 2);
    transport.reply(transport.fetches[1], original);
    QCOMPARE(client.state(), ClientState::Ready);
    QCOMPARE(snapshots.count(), 1);

    transport.invalidate(QStringLiteral(":1.22"), 11, 6);
    QCOMPARE(transport.fetches.size(), 3);
    Snapshot contradiction = original;
    contradiction.outputs[0].volume = 0.7;
    transport.reply(transport.fetches[2], contradiction);
    QCOMPARE(client.state(), ClientState::Unavailable);
    QCOMPARE(client.reasonCode(), QStringLiteral("malformed-snapshot"));
    QCOMPARE(client.snapshot(), original);
    QCOMPARE(snapshots.count(), 1);

    QTRY_COMPARE(transport.fetches.size(), 4);
    transport.reply(transport.fetches[3], clientSnapshot(10, 99));
    QCOMPARE(client.snapshot(), original);
    QCOMPARE(snapshots.count(), 1);
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
    QCOMPARE(completed.count(), 0);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), requestId);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(transport.fetches.size(), 2);
    transport.finish(oldOperation, successfulResult(oldOperation, 3));
    QCOMPARE(completed.count(), 1);
    transport.reply(transport.fetches[1], clientSnapshot(12, 1));
    QCOMPARE(client.snapshot().epoch, quint64(12));
}

void AudioClientTests::transportResultsAreQueued_data()
{
    QTest::addColumn<OperationStatus>("status");
    QTest::newRow("succeeded") << OperationStatus::Succeeded;
    QTest::newRow("rejected") << OperationStatus::Rejected;
    QTest::newRow("unsupported") << OperationStatus::Unsupported;
    QTest::newRow("failed") << OperationStatus::Failed;
    QTest::newRow("uncertain") << OperationStatus::Uncertain;
    QTest::newRow("busy") << OperationStatus::Busy;
}

void AudioClientTests::transportResultsAreQueued()
{
    QFETCH(OperationStatus, status);
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.45"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 requestId = client.setDefault({.epoch = 11, .serial = 10});
    OperationResult result = successfulResult(transport.operations[0], 3);
    result.status = status;
    result.reasonCode = QStringLiteral("backend-result");
    transport.finish(transport.operations[0], result);
    QCOMPARE(completed.count(), 0);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), requestId);
    QCOMPARE(completed[0][1].value<OperationResult>().status, status);
    QCoreApplication::processEvents();
    QCOMPARE(completed.count(), 1);
}

void AudioClientTests::synchronousTransportResultIsStillQueued_data()
{
    transportResultsAreQueued_data();
}

void AudioClientTests::synchronousTransportResultIsStillQueued()
{
    QFETCH(OperationStatus, status);
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.46"));
    transport.reply(transport.fetches[0], clientSnapshot());
    transport.operationSubmitted = [&transport, status](
                                       const FakeAudioTransport::Operation &op) {
        OperationResult result = successfulResult(op, 3);
        result.status = status;
        result.reasonCode = QStringLiteral("backend-result");
        transport.finish(op, result);
    };
    const quint64 requestId = client.setMute({.epoch = 11, .serial = 10}, true);
    QCOMPARE(completed.count(), 0);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), requestId);
    QCOMPARE(completed[0][1].value<OperationResult>().status, status);
    QCoreApplication::processEvents();
    QCOMPARE(completed.count(), 1);
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
    QCOMPARE(completed.count(), 0);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(completed[0][1].value<OperationResult>().reasonCode,
             QStringLiteral("malformed-result"));
    QCOMPARE(transport.operations.size(), 1);
}

void AudioClientTests::oldEpochSuccessBecomesUncertain()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.55"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 requestId = client.setVolume({.epoch = 11, .serial = 10}, 0.4);
    const auto operation = transport.operations[0];
    transport.invalidate(QStringLiteral(":1.55"), 12, 1);
    transport.reply(transport.fetches[1], clientSnapshot(12, 1));
    QCOMPARE(client.snapshot().epoch, quint64(12));
    QCOMPARE(completed.count(), 0);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), requestId);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(completed[0][1].value<OperationResult>().reasonCode,
             QStringLiteral("authority-replaced"));

    transport.finish(operation, successfulResult(operation, 3));
    QCoreApplication::processEvents();
    QCOMPARE(completed.count(), 1);
    QCOMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.fetches.size(), 2);
}

void AudioClientTests::localCompletionsAreQueued_data()
{
    QTest::addColumn<LocalScenario>("scenario");
    QTest::addColumn<OperationStatus>("status");
    QTest::addColumn<QString>("reasonCode");
    QTest::newRow("unavailable") << LocalScenario::Unavailable
                                  << OperationStatus::Rejected
                                  << QStringLiteral("unavailable");
    QTest::newRow("set-default-stale") << LocalScenario::StaleDefault
                                        << OperationStatus::Rejected
                                        << QStringLiteral("stale-handle");
    QTest::newRow("set-volume-invalid") << LocalScenario::InvalidVolume
                                         << OperationStatus::Rejected
                                         << QStringLiteral("invalid-volume");
    QTest::newRow("set-mute-unsupported") << LocalScenario::UnsupportedMute
                                           << OperationStatus::Unsupported
                                           << QStringLiteral("unsupported");
    QTest::newRow("move-stream-incompatible") << LocalScenario::IncompatibleMove
                                               << OperationStatus::Rejected
                                               << QStringLiteral("incompatible-target");
    QTest::newRow("channel-count-mismatch") << LocalScenario::ChannelCountMismatch
                                            << OperationStatus::Rejected
                                            << QStringLiteral("invalid-target");
    QTest::newRow("channel-bad-level") << LocalScenario::ChannelBadLevel
                                       << OperationStatus::Rejected
                                       << QStringLiteral("invalid-volume");
    QTest::newRow("channel-unsupported-capability")
        << LocalScenario::ChannelUnsupportedCapability << OperationStatus::Unsupported
        << QStringLiteral("unsupported");
    QTest::newRow("create-bad-name") << LocalScenario::CreateBadName
                                     << OperationStatus::Rejected
                                     << QStringLiteral("invalid-name");
    QTest::newRow("create-bad-channels") << LocalScenario::CreateBadChannels
                                         << OperationStatus::Rejected
                                         << QStringLiteral("invalid-channel-count");
    QTest::newRow("create-unsupported-capability")
        << LocalScenario::CreateUnsupportedCapability << OperationStatus::Unsupported
        << QStringLiteral("unsupported");
    QTest::newRow("remove-non-virtual") << LocalScenario::RemoveNonVirtual
                                        << OperationStatus::Rejected
                                        << QStringLiteral("invalid-target");
    QTest::newRow("remove-stale") << LocalScenario::RemoveStale
                                  << OperationStatus::Rejected
                                  << QStringLiteral("stale-handle");
}

void AudioClientTests::localCompletionsAreQueued()
{
    QFETCH(LocalScenario, scenario);
    QFETCH(OperationStatus, status);
    QFETCH(QString, reasonCode);
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    if (scenario != LocalScenario::Unavailable) {
        transport.announceOwner(QStringLiteral(":1.60"));
        Snapshot snapshot = clientSnapshot();
        switch (scenario) {
        case LocalScenario::UnsupportedMute:
            snapshot.outputs[0].canSetMute = false;
            break;
        case LocalScenario::ChannelUnsupportedCapability:
            snapshot.capabilities &= ~Capabilities(Capability::SetChannelVolumes);
            break;
        case LocalScenario::CreateUnsupportedCapability:
            snapshot.capabilities &= ~Capabilities(Capability::ManageVirtualDevices);
            break;
        default:
            break;
        }
        transport.reply(transport.fetches[0], snapshot);
    }

    const quint64 requestId = invokeLocalScenario(client, scenario);
    QVERIFY(requestId != 0);
    QCOMPARE(completed.count(), 0);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), requestId);
    QCOMPARE(completed[0][1].value<OperationResult>().status, status);
    QCOMPARE(completed[0][1].value<OperationResult>().reasonCode, reasonCode);
    QCoreApplication::processEvents();
    QCOMPARE(completed.count(), 1);
    QVERIFY(transport.operations.isEmpty());
}

void AudioClientTests::channelVolumeOperationReachesTransport()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.64"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 requestId =
        client.setChannelVolumes({.epoch = 11, .serial = 10}, {0.15, 0.85});
    QVERIFY(requestId != 0);
    QCOMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations[0].request.kind, OperationKind::SetChannelVolumes);
    QCOMPARE(transport.operations[0].request.primary.serial, quint64(10));
    QCOMPARE(transport.operations[0].request.channelVolumes, QVector<double>({0.15, 0.85}));

    // A second mutation while the first is transport-backed stays serialized.
    const quint64 secondId =
        client.setChannelVolumes({.epoch = 11, .serial = 30}, {0.2, 0.4});
    QVERIFY(secondId != 0);
    QCOMPARE(transport.operations.size(), 1);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), secondId);
    QCOMPARE(completed[0][1].value<OperationResult>().status, OperationStatus::Busy);

    transport.finish(transport.operations[0], successfulResult(transport.operations[0], 3));
    QTRY_COMPARE(completed.count(), 2);
    QCOMPARE(completed[1][0].toULongLong(), requestId);
    QCOMPARE(completed[1][1].value<OperationResult>().status, OperationStatus::Succeeded);
}

void AudioClientTests::virtualDeviceOperationsReachTransport()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.65"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 createdId = client.createVirtualDevice(DeviceKind::Output,
                                                         QStringLiteral("Studio Bus"), 6);
    QVERIFY(createdId != 0);
    QCOMPARE(transport.operations.size(), 1);
    const auto createRequest = transport.operations[0].request;
    QCOMPARE(createRequest.kind, OperationKind::CreateVirtualDevice);
    QCOMPARE(createRequest.deviceKind, DeviceKind::Output);
    QCOMPARE(createRequest.displayName, QStringLiteral("Studio Bus"));
    QCOMPARE(createRequest.channels, quint32(6));
    transport.finish(transport.operations[0], successfulResult(transport.operations[0], 3));
    QTRY_COMPARE(completed.count(), 1);

    const quint64 removedId = client.removeVirtualDevice({.epoch = 11, .serial = 11});
    QVERIFY(removedId != 0);
    QCOMPARE(transport.operations.size(), 2);
    QCOMPARE(transport.operations[1].request.kind, OperationKind::RemoveVirtualDevice);
    QCOMPARE(transport.operations[1].request.primary.serial, quint64(11));
    transport.finish(transport.operations[1], successfulResult(transport.operations[1], 3));
    QTRY_COMPARE(completed.count(), 2);
    QCOMPARE(completed[1][1].value<OperationResult>().status, OperationStatus::Succeeded);
}

void AudioClientTests::busyCompletionIsQueued()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.61"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 pendingId = client.setDefault({.epoch = 11, .serial = 10});
    const quint64 busyId = client.setMute({.epoch = 11, .serial = 10}, true);
    QCOMPARE(completed.count(), 0);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), busyId);
    QCOMPARE(completed[0][1].value<OperationResult>().status, OperationStatus::Busy);
    transport.finish(transport.operations[0],
                     successfulResult(transport.operations[0], 3));
    QCOMPARE(completed.count(), 1);
    QTRY_COMPARE(completed.count(), 2);
    QCOMPARE(completed[1][0].toULongLong(), pendingId);
}

void AudioClientTests::stopCompletionAndCancellationAreLifetimeSafe()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.62"));
    transport.reply(transport.fetches[0], clientSnapshot());
    const quint64 pendingId = client.setDefault({.epoch = 11, .serial = 10});
    client.stop();
    QCOMPARE(completed.count(), 0);
    client.stop();
    QCOMPARE(completed.count(), 0);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(completed[0][0].toULongLong(), pendingId);
    QCOMPARE(completed[0][1].value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(completed[0][1].value<OperationResult>().reasonCode,
             QStringLiteral("client-stopped"));

    FakeAudioTransport cancelledTransport;
    auto cancelledClient = std::make_unique<AudioClient>(&cancelledTransport);
    QSignalSpy cancelled(cancelledClient.get(), &AudioClient::operationCompleted);
    cancelledClient->start();
    const quint64 rejectedId =
        cancelledClient->setDefault({.epoch = 11, .serial = 10});
    QVERIFY(rejectedId != 0);
    QCOMPARE(cancelled.count(), 0);
    cancelledClient->stop();
    QCoreApplication::processEvents();
    QCOMPARE(cancelled.count(), 0);

    cancelledClient->start();
    cancelledTransport.announceOwner(QStringLiteral(":1.63"));
    cancelledTransport.reply(cancelledTransport.fetches.constLast(), clientSnapshot());
    QVERIFY(cancelledClient->setMute({.epoch = 11, .serial = 10}, true) != 0);
    cancelledClient->stop();
    QCOMPARE(cancelled.count(), 0);
    cancelledClient.reset();
    QCoreApplication::processEvents();
    QCOMPARE(cancelled.count(), 0);
}

QTEST_GUILESS_MAIN(AudioClientTests)
#include "tst_audio_client.moc"
