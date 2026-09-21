// SPDX-License-Identifier: GPL-3.0-or-later

// VoiceClient: owner attribution, snapshot ordering, and single-intent
// accounting. The rules that matter here are the ones that decide whether a
// microphone gets opened twice, so most rows are about what the client
// refuses rather than what it forwards.

#include "support/fake_voice_transport.h"

#include <qindaqt/services/voice_client/voice_client.h>

#include <QtTest/QtTest>

using namespace QindaQt::Services::Voice;

namespace {

const QString kOwner = QStringLiteral(":1.42");
const QString kOtherOwner = QStringLiteral(":1.43");

Snapshot snapshotAt(quint64 revision, SessionState state = SessionState::Idle)
{
    Snapshot snapshot;
    snapshot.revision = revision;
    snapshot.state = state;
    snapshot.enabled = true;
    snapshot.capabilities = kKnownCapabilities;
    snapshot.providerId = QStringLiteral("elevenlabs");
    snapshot.providerLabel = QStringLiteral("ElevenLabs");
    snapshot.languageCode = QStringLiteral("en");
    snapshot.reasonCode = QStringLiteral("ok");
    snapshot.lastText = QStringLiteral("hello");
    snapshot.providers = {ProviderDescriptor{.id = QStringLiteral("elevenlabs"),
                                             .label = QStringLiteral("ElevenLabs"),
                                             .available = true}};
    return snapshot;
}

OperationResult resultFor(const OperationRequest &request, OperationStatus status,
                          quint64 observedRevision)
{
    return OperationResult{.kind = request.kind,
                           .status = status,
                           .requestId = request.requestId,
                           .initiatingRevision = request.expectedRevision,
                           .observedRevision = observedRevision,
                           .reasonCode = QStringLiteral("ok")};
}

} // namespace

class VoiceClientTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void becomesReadyOnAValidSnapshot();
    void reportsUnavailableWithoutAnOwner();
    void rejectsAMalformedSnapshot();
    void rejectsARegressedRevisionFromTheSameOwner();
    void acceptsARepeatedRevisionOnlyWhenIdentical();
    void doesNotRefetchForARevisionItAlreadyHolds();
    void refetchesForAnUnseenRevision();
    void ignoresAnInvalidationFromAnotherOwner();
    void refusesAnIntentWithoutASnapshot();
    void refusesAnUnadvertisedIntent();
    void forwardsAnAdmittedIntentOnce();
    void reportsASecondIntentAsBusy();
    void reportsAForeignResultAsUncertain();
    void reportsALostOwnerAsUncertain();
    void neverReplaysAnUncertainIntent();
    void levelIsOnlyHonouredDuringCapture();
    void levelIsClamped();
    void stoppingResolvesAPendingIntent();

private:
    void becomeReady(quint64 revision = 1,
                     SessionState state = SessionState::Idle);

    FakeVoiceTransport *m_transport = nullptr;
    VoiceClient *m_client = nullptr;
};

void VoiceClientTest::init()
{
    m_transport = new FakeVoiceTransport;
    m_client = new VoiceClient(m_transport);
    m_client->setRequestTimeout(50);
}

void VoiceClientTest::cleanup()
{
    delete m_client;
    m_client = nullptr;
    delete m_transport;
    m_transport = nullptr;
}

void VoiceClientTest::becomeReady(const quint64 revision, const SessionState state)
{
    m_client->start();
    m_transport->becomeOwner(kOwner);
    QCOMPARE(m_transport->fetches.size(), 1);
    m_transport->answerSnapshot(snapshotAt(revision, state));
    QCOMPARE(m_client->state(), ClientState::Ready);
}

void VoiceClientTest::becomesReadyOnAValidSnapshot()
{
    QSignalSpy states(m_client, &VoiceClient::stateChanged);
    QSignalSpy snapshots(m_client, &VoiceClient::snapshotChanged);
    becomeReady();
    QVERIFY(m_transport->started);
    QCOMPARE(snapshots.size(), 1);
    QVERIFY(states.size() >= 2);
    QCOMPARE(m_client->snapshot().revision, quint64(1));
}

void VoiceClientTest::reportsUnavailableWithoutAnOwner()
{
    m_client->start();
    m_transport->becomeOwner(QString());
    QCOMPARE(m_client->state(), ClientState::Unavailable);
    QCOMPARE(m_client->reasonCode(), QStringLiteral("service-unavailable"));
    QVERIFY(m_transport->fetches.isEmpty());
}

void VoiceClientTest::rejectsAMalformedSnapshot()
{
    m_client->start();
    m_transport->becomeOwner(kOwner);
    Snapshot broken = snapshotAt(1);
    broken.reasonCode = QStringLiteral("Not A Reason Code");
    m_transport->answerSnapshot(broken);
    QCOMPARE(m_client->state(), ClientState::Unavailable);
    QVERIFY(!m_client->hasSnapshot());
}

void VoiceClientTest::rejectsARegressedRevisionFromTheSameOwner()
{
    becomeReady(5);
    m_transport->invalidate(kOwner, 4);
    QCOMPARE(m_transport->fetches.size(), 1);
    m_transport->answerSnapshot(snapshotAt(4));
    QCOMPARE(m_client->state(), ClientState::Unavailable);
    QCOMPARE(m_client->reasonCode(), QStringLiteral("revision-contradiction"));
}

void VoiceClientTest::acceptsARepeatedRevisionOnlyWhenIdentical()
{
    becomeReady(3);
    QSignalSpy snapshots(m_client, &VoiceClient::snapshotChanged);

    m_transport->invalidate(kOwner, 9);
    m_transport->answerSnapshot(snapshotAt(3));
    QCOMPARE(m_client->state(), ClientState::Ready);
    // An identical repeat is not a change, so no consumer is woken.
    QCOMPARE(snapshots.size(), 0);

    m_transport->invalidate(kOwner, 9);
    m_transport->answerSnapshot(snapshotAt(3, SessionState::Listening));
    QCOMPARE(m_client->state(), ClientState::Unavailable);
}

void VoiceClientTest::doesNotRefetchForARevisionItAlreadyHolds()
{
    becomeReady(7);
    m_transport->invalidate(kOwner, 7);
    QVERIFY(m_transport->fetches.isEmpty());
}

void VoiceClientTest::refetchesForAnUnseenRevision()
{
    becomeReady(7);
    m_transport->invalidate(kOwner, 8);
    QCOMPARE(m_transport->fetches.size(), 1);
}

void VoiceClientTest::ignoresAnInvalidationFromAnotherOwner()
{
    becomeReady(7);
    m_transport->invalidate(kOtherOwner, 8);
    QVERIFY(m_transport->fetches.isEmpty());
}

void VoiceClientTest::refusesAnIntentWithoutASnapshot()
{
    QSignalSpy completions(m_client, &VoiceClient::operationCompleted);
    m_client->start();
    const quint64 requestId = m_client->startDictation();
    QVERIFY(requestId != 0);
    QVERIFY(completions.wait(200));
    const auto result = completions.at(0).at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Rejected);
    QCOMPARE(result.reasonCode, QStringLiteral("unavailable"));
    QVERIFY(m_transport->submissions.isEmpty());
}

void VoiceClientTest::refusesAnUnadvertisedIntent()
{
    m_client->start();
    m_transport->becomeOwner(kOwner);
    Snapshot limited = snapshotAt(1);
    limited.capabilities = CapabilityNone;
    m_transport->answerSnapshot(limited);
    QSignalSpy completions(m_client, &VoiceClient::operationCompleted);
    const quint64 requestId = m_client->undo();
    QVERIFY(requestId != 0);
    QVERIFY(completions.wait(200));
    const auto result = completions.at(0).at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Rejected);
    QCOMPARE(result.reasonCode, QStringLiteral("unsupported-capability"));
    QVERIFY(m_transport->submissions.isEmpty());
}

void VoiceClientTest::forwardsAnAdmittedIntentOnce()
{
    becomeReady(2);
    QSignalSpy completions(m_client, &VoiceClient::operationCompleted);
    const quint64 requestId = m_client->startDictation();
    QCOMPARE(m_transport->submissions.size(), 1);
    const auto submission = m_transport->submissions.at(0);
    QCOMPARE(submission.request.kind, OperationKind::StartDictation);
    QCOMPARE(submission.request.expectedRevision, quint64(2));
    QCOMPARE(submission.request.requestId, requestId);

    m_transport->answerOperation(
        resultFor(submission.request, OperationStatus::Succeeded, 3));
    QVERIFY(completions.wait(200));
    QCOMPARE(completions.at(0).at(1).value<OperationResult>().status,
             OperationStatus::Succeeded);
    // Completing an intent asks for fresh truth.
    QCOMPARE(m_transport->fetches.size(), 1);
}

void VoiceClientTest::reportsASecondIntentAsBusy()
{
    becomeReady(2);
    QSignalSpy completions(m_client, &VoiceClient::operationCompleted);
    QVERIFY(m_client->startDictation() != 0);
    QVERIFY(m_client->startCommand() != 0);
    QVERIFY(completions.wait(200));
    QCOMPARE(completions.size(), 1);
    QCOMPARE(completions.at(0).at(1).value<OperationResult>().status,
             OperationStatus::Busy);
    QCOMPARE(m_transport->submissions.size(), 1);
}

void VoiceClientTest::reportsAForeignResultAsUncertain()
{
    becomeReady(2);
    QSignalSpy completions(m_client, &VoiceClient::operationCompleted);
    QVERIFY(m_client->startDictation() != 0);
    OperationResult foreign =
        resultFor(m_transport->submissions.at(0).request, OperationStatus::Succeeded, 3);
    foreign.requestId += 1;
    m_transport->answerOperation(foreign);
    QVERIFY(completions.wait(200));
    QCOMPARE(completions.at(0).at(1).value<OperationResult>().status,
             OperationStatus::Uncertain);
}

void VoiceClientTest::reportsALostOwnerAsUncertain()
{
    becomeReady(2);
    QSignalSpy completions(m_client, &VoiceClient::operationCompleted);
    QVERIFY(m_client->startDictation() != 0);
    m_transport->becomeOwner(QString());
    QVERIFY(completions.wait(200));
    const auto result = completions.at(0).at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("owner-replaced"));
}

void VoiceClientTest::neverReplaysAnUncertainIntent()
{
    becomeReady(2);
    QSignalSpy completions(m_client, &VoiceClient::operationCompleted);
    QVERIFY(m_client->startDictation() != 0);
    m_transport->submissions.clear();
    // Let the operation timeout fire; a dictation that may already be live
    // must never be started a second time on the client's own initiative.
    QVERIFY(completions.wait(500));
    QCOMPARE(completions.at(0).at(1).value<OperationResult>().status,
             OperationStatus::Uncertain);
    QVERIFY(m_transport->submissions.isEmpty());
}

void VoiceClientTest::levelIsOnlyHonouredDuringCapture()
{
    becomeReady(1, SessionState::Listening);
    QSignalSpy levels(m_client, &VoiceClient::levelChanged);
    m_transport->reportLevel(kOwner, 60);
    QCOMPARE(m_client->levelPercent(), 60u);
    QCOMPARE(levels.size(), 1);

    m_transport->invalidate(kOwner, 2);
    m_transport->answerSnapshot(snapshotAt(2, SessionState::Idle));
    QCOMPARE(m_client->levelPercent(), 0u);

    // A meter that keeps moving after capture ends reads as a hot microphone.
    m_transport->reportLevel(kOwner, 80);
    QCOMPARE(m_client->levelPercent(), 0u);
}

void VoiceClientTest::levelIsClamped()
{
    becomeReady(1, SessionState::Listening);
    m_transport->reportLevel(kOwner, 5'000);
    QCOMPARE(m_client->levelPercent(), 100u);
}

void VoiceClientTest::stoppingResolvesAPendingIntent()
{
    becomeReady(2);
    QSignalSpy completions(m_client, &VoiceClient::operationCompleted);
    QVERIFY(m_client->startDictation() != 0);
    m_client->stop();
    QVERIFY(completions.wait(200));
    const auto result = completions.at(0).at(1).value<OperationResult>();
    QCOMPARE(result.status, OperationStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("client-stopped"));
    QCOMPARE(m_client->state(), ClientState::Stopped);
}

QTEST_MAIN(VoiceClientTest)
#include "tst_voice_client.moc"
