// SPDX-License-Identifier: LGPL-3.0-or-later
#include "support/notification_presentation_client_test_support.h"

#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services;
using namespace QindaQt::Tests::NotificationPresentationClientSupport;

class NotificationPresentationClientTests final : public QObject {
    Q_OBJECT

private slots:
    void authenticatesAndAcceptsBoundedSnapshots();
    void coalescesInvalidationsAndFollowsAnInFlightChange();
    void rejectsLateOwnerRepliesAndRegressingRevisions();
    void timesOutAndRetriesWithoutParallelRequests();
    void accessDenialForcesReauthentication();
    void validatesAndTracksOperations();
    void stopReleasesTheAuthenticatedPresenter();
    void rejectsInvalidTimingAndTransportStartup();
};

void NotificationPresentationClientTests::authenticatesAndAcceptsBoundedSnapshots()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    QVERIFY(client.start());
    QCOMPARE(client.state(), NotificationPresentationClient::ClientState::Unavailable);

    transport.announceOwner(QStringLiteral(":1.10"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    QCOMPARE(transport.requests[0].type, RequestType::Register);
    QCOMPARE(transport.observedAccessToken, QString(64, QLatin1Char('a')));
    transport.reply(transport.requests[0],
                    snapshotWire(QStringLiteral("11111111-1111-1111-1111-111111111111"), 1));

    QTRY_COMPARE_WITH_TIMEOUT(client.state(),
                              NotificationPresentationClient::ClientState::Ready,
                              100);
    QVERIFY(client.snapshot().has_value());
    QCOMPARE(client.snapshot()->notifications[0].summary,
             QStringLiteral("Build complete"));
}

void NotificationPresentationClientTests::
    coalescesInvalidationsAndFollowsAnInFlightChange()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.11");
    const QString epoch = QStringLiteral("11111111-1111-1111-1111-111111111111");
    transport.announceOwner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], snapshotWire(epoch, 1));

    transport.invalidate(owner, epoch, 2);
    transport.invalidate(owner, epoch, 3);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    QCOMPARE(transport.requests[1].type, RequestType::Snapshot);
    transport.invalidate(owner, epoch, 4);
    QTest::qWait(5);
    QCOMPARE(transport.requests.size(), 2);
    transport.reply(transport.requests[1], snapshotWire(epoch, 3));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(transport.requests[2], snapshotWire(epoch, 4));
    QTRY_COMPARE_WITH_TIMEOUT(client.snapshot()->revision, quint64(4), 100);
}

void NotificationPresentationClientTests::
    rejectsLateOwnerRepliesAndRegressingRevisions()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    QVERIFY(client.start());
    transport.announceOwner(QStringLiteral(":1.20"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    const RecordedRequest abandoned = transport.requests[0];
    transport.announceOwner(QStringLiteral(":1.21"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    transport.reply(abandoned,
                    snapshotWire(QStringLiteral("22222222-2222-2222-2222-222222222222"), 99));
    QVERIFY(!client.snapshot().has_value());

    const QString epoch = QStringLiteral("33333333-3333-3333-3333-333333333333");
    transport.reply(transport.requests[1], snapshotWire(epoch, 5));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(),
                              NotificationPresentationClient::ClientState::Ready,
                              100);
    transport.invalidate(QStringLiteral(":1.21"), epoch, 6);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    transport.reply(transport.requests[2], snapshotWire(epoch, 4));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(),
                              NotificationPresentationClient::ClientState::Degraded,
                              100);
    QVERIFY(!client.snapshot().has_value());
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 4, 100);
}

void NotificationPresentationClientTests::timesOutAndRetriesWithoutParallelRequests()
{
    FakeTransport transport;
    auto timing = fastTiming();
    timing.requestTimeoutMilliseconds = 8;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), timing);
    QVERIFY(client.start());
    transport.announceOwner(QStringLiteral(":1.30"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    QVERIFY(client.requestInFlight());
    QTRY_COMPARE_WITH_TIMEOUT(client.state(),
                              NotificationPresentationClient::ClientState::Degraded,
                              100);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    QVERIFY(client.requestInFlight());
}

void NotificationPresentationClientTests::accessDenialForcesReauthentication()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    QVERIFY(client.start());
    transport.announceOwner(QStringLiteral(":1.40"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.fail(transport.requests[0],
                   QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                   QStringLiteral("authentication failed"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    QCOMPARE(transport.requests[1].type, RequestType::Register);
    QCOMPARE(client.state(),
             NotificationPresentationClient::ClientState::Authenticating);
}

void NotificationPresentationClientTests::validatesAndTracksOperations()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.50");
    const QString epoch = QStringLiteral("55555555-5555-5555-5555-555555555555");
    transport.announceOwner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests[0], snapshotWire(epoch, 1));
    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);

    QString error;
    QVERIFY(!client.dismiss(99, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(client.invokeAction(7, QStringLiteral("open"),
                                QStringLiteral("activation-token"), &error));
    QCOMPARE(transport.operations.size(), 1);
    QVERIFY(!client.dismiss(7, &error));
    QSignalSpy succeeded(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::operationSucceeded);
    QSignalSpy inFlightChanged(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationInFlightChanged);
    QVERIFY(client.operationInFlight());
    transport.finish(transport.operations[0], 1, 2);
    QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(), 1, 100);
    QCOMPARE(inFlightChanged.size(), 1);
    QVERIFY(!client.operationInFlight());
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
}

void NotificationPresentationClientTests::stopReleasesTheAuthenticatedPresenter()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    QVERIFY(client.start());
    const QString owner = QStringLiteral(":1.60");
    transport.announceOwner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(
        transport.requests[0],
        snapshotWire(QStringLiteral("66666666-6666-6666-6666-666666666666"), 1));
    QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().has_value(), 100);
    client.stop();
    QCOMPARE(transport.releasedOwners, QVector<QString>{owner});
    QVERIFY(!transport.running);
}

void NotificationPresentationClientTests::rejectsInvalidTimingAndTransportStartup()
{
    FakeTransport transport;
    auto timing = fastTiming();
    timing.retryMilliseconds = {5, 4};
    NotificationPresentationClient::NotificationPresentationClient invalidClient(
        transport, accessToken(), timing);
    QString error;
    QVERIFY(!invalidClient.start(&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(transport.startCalls, 0);

    FakeTransport failing;
    failing.startSucceeds = false;
    NotificationPresentationClient::NotificationPresentationClient failedClient(
        failing, accessToken(), fastTiming());
    QVERIFY(!failedClient.start(&error));
    QCOMPARE(failing.startCalls, 1);
}

QTEST_GUILESS_MAIN(NotificationPresentationClientTests)

#include "tst_notification_presentation_client.moc"
