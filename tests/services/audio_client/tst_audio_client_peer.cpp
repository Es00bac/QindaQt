// SPDX-License-Identifier: GPL-3.0-or-later

// VBAN name-based reply lineage is distinct from handle-targeted Audio1
// mutations: the service may have accepted a newer snapshot revision before
// applying a peer setting, but the same owner/epoch still bounds authority.
#include "support/fake_audio_transport.h"

#include <qindaqt/services/audio_client/audio_client.h>

#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Tests;

class AudioClientPeerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void peerReplyLineage_data();
    void peerReplyLineage();
    void peerOldOwnerCannotComplete();
};

void AudioClientPeerTests::peerReplyLineage_data()
{
    QTest::addColumn<OperationKind>("kind");
    QTest::addColumn<quint64>("initiatingEpoch");
    QTest::addColumn<quint64>("initiatingRevision");
    QTest::addColumn<quint64>("observedRevision");
    QTest::addColumn<OperationStatus>("wireStatus");
    QTest::addColumn<OperationStatus>("expectedStatus");
    QTest::newRow("upsert-newer-service-revision")
        << OperationKind::UpsertVbanStream << quint64(11) << quint64(3)
        << quint64(4) << OperationStatus::Succeeded << OperationStatus::Succeeded;
    QTest::newRow("delete-newer-service-revision")
        << OperationKind::DeleteVbanStream << quint64(11) << quint64(3)
        << quint64(4) << OperationStatus::Succeeded << OperationStatus::Succeeded;
    QTest::newRow("enable-newer-service-revision")
        << OperationKind::SetVbanEnabled << quint64(11) << quint64(3)
        << quint64(4) << OperationStatus::Succeeded << OperationStatus::Succeeded;
    QTest::newRow("enable-regressing-initiation")
        << OperationKind::SetVbanEnabled << quint64(11) << quint64(1)
        << quint64(4) << OperationStatus::Succeeded << OperationStatus::Uncertain;
    QTest::newRow("upsert-regressing-initiation")
        << OperationKind::UpsertVbanStream << quint64(11) << quint64(1)
        << quint64(4) << OperationStatus::Succeeded << OperationStatus::Uncertain;
    QTest::newRow("upsert-regressing-observation")
        << OperationKind::UpsertVbanStream << quint64(11) << quint64(4)
        << quint64(3) << OperationStatus::Succeeded << OperationStatus::Uncertain;
    QTest::newRow("upsert-foreign-epoch")
        << OperationKind::UpsertVbanStream << quint64(10) << quint64(3)
        << quint64(4) << OperationStatus::Succeeded << OperationStatus::Uncertain;
    QTest::newRow("upsert-malformed-status")
        << OperationKind::UpsertVbanStream << quint64(11) << quint64(3)
        << quint64(4) << static_cast<OperationStatus>(99) << OperationStatus::Uncertain;
    QTest::newRow("handle-operation-stays-exact")
        << OperationKind::SetMute << quint64(11) << quint64(3)
        << quint64(4) << OperationStatus::Succeeded << OperationStatus::Uncertain;
}

void AudioClientPeerTests::peerReplyLineage()
{
    QFETCH(OperationKind, kind);
    QFETCH(quint64, initiatingEpoch);
    QFETCH(quint64, initiatingRevision);
    QFETCH(quint64, observedRevision);
    QFETCH(OperationStatus, wireStatus);
    QFETCH(OperationStatus, expectedStatus);
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.90"));
    Snapshot snapshot = clientSnapshot();
    snapshot.capabilities |= Capability::Console | Capability::ManageVbanStreams;
    transport.reply(transport.fetches[0], snapshot);
    QCOMPARE(client.state(), ClientState::Ready);
    quint64 requestId = 0;
    if (kind == OperationKind::UpsertVbanStream) {
        VbanStream definition;
        definition.name = QStringLiteral("Desk");
        definition.outgoing = false;
        definition.host = QStringLiteral("192.0.2.10");
        definition.outputNodeName = QStringLiteral("alsa_output.test");
        definition.port = 6980;
        requestId = client.upsertVbanStream(definition);
    } else if (kind == OperationKind::DeleteVbanStream) {
        requestId = client.deleteVbanStream(QStringLiteral("Desk"));
    } else if (kind == OperationKind::SetVbanEnabled) {
        requestId = client.setVbanEnabled(QStringLiteral("Desk"), true);
    } else {
        requestId = client.setMute({.epoch = 11, .serial = 10}, true);
    }
    QVERIFY(requestId != 0);
    QCOMPARE(transport.operations.size(), 1);
    const auto operation = transport.operations.constFirst();
    OperationResult result = successfulResult(operation, observedRevision);
    result.initiatingEpoch = initiatingEpoch;
    result.observedEpoch = initiatingEpoch;
    result.initiatingRevision = initiatingRevision;
    result.status = wireStatus;
    transport.finish(operation, result);
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.constFirst().constFirst().toULongLong(), requestId);
    QCOMPARE(completed.constFirst().at(1).value<OperationResult>().status, expectedStatus);
}

void AudioClientPeerTests::peerOldOwnerCannotComplete()
{
    FakeAudioTransport transport;
    AudioClient client(&transport);
    QSignalSpy completed(&client, &AudioClient::operationCompleted);
    client.start();
    transport.announceOwner(QStringLiteral(":1.90"));
    Snapshot snapshot = clientSnapshot();
    snapshot.capabilities |= Capability::Console | Capability::ManageVbanStreams;
    transport.reply(transport.fetches[0], snapshot);
    const quint64 requestId = client.deleteVbanStream(QStringLiteral("Desk"));
    QCOMPARE(transport.operations.size(), 1);
    const auto oldOperation = transport.operations.constFirst();
    transport.announceOwner(QStringLiteral(":1.91"));
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.constFirst().constFirst().toULongLong(), requestId);
    QCOMPARE(completed.constFirst().at(1).value<OperationResult>().status,
             OperationStatus::Uncertain);
    transport.finish(oldOperation, successfulResult(oldOperation, 4));
    QCoreApplication::processEvents();
    QCOMPARE(completed.size(), 1);
}

QTEST_MAIN(AudioClientPeerTests)
#include "tst_audio_client_peer.moc"
