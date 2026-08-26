// SPDX-License-Identifier: LGPL-3.0-or-later
#include "support/notification_presentation_client_test_support.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services;
using namespace QindaQt::Tests::NotificationPresentationClientSupport;

namespace {

constexpr auto Epoch = "77777777-7777-7777-7777-777777777777";

void beginReadyClient(
    FakeTransport &transport,
    NotificationPresentationClient::NotificationPresentationClient &client,
    const QString &owner, quint64 revision, bool resident = false)
{
    QVERIFY(client.start());
    transport.announceOwner(owner);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 1, 100);
    transport.reply(transport.requests.constFirst(),
                    snapshotWire(QString::fromLatin1(Epoch), revision,
                                 QStringLiteral("Build complete"), resident));
    QTRY_COMPARE_WITH_TIMEOUT(
        client.state(), NotificationPresentationClient::ClientState::Ready, 100);
}

} // namespace

class NotificationPresentationClientOperationTests final : public QObject {
    Q_OBJECT

private slots:
    void acceptsResidentActionWithoutRevisionAdvance();
    void rejectsInvalidOperationResults_data();
    void rejectsInvalidOperationResults();
    void timeoutRecoversAndIgnoresLateReply();
    void ownerChangeRejectsOperationAndIgnoresOldReply();
    void operationFailureForcesSnapshotAfterConcurrentRequest();
    void accessDenialCancelsConcurrentRequestAndReauthenticates();
    void presentsNormalizedRemoteErrors_data();
    void presentsNormalizedRemoteErrors();
};

void NotificationPresentationClientOperationTests::
    acceptsResidentActionWithoutRevisionAdvance()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    beginReadyClient(transport, client, QStringLiteral(":1.70"), 5, true);

    QSignalSpy succeeded(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationSucceeded);
    QSignalSpy rejected(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationRejected);
    QSignalSpy busyChanged(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationInFlightChanged);

    QVERIFY(client.invokeAction(7, QStringLiteral("open"), {}, nullptr));
    QVERIFY(client.operationInFlight());
    QCOMPARE(busyChanged.size(), 1);
    transport.finish(transport.operations.constFirst(), 5, 5);

    QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(), 1, 100);
    QCOMPARE(rejected.size(), 0);
    QCOMPARE(busyChanged.size(), 2);
    QVERIFY(!client.operationInFlight());
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    QCOMPARE(transport.requests.constLast().type, RequestType::Snapshot);
}

void NotificationPresentationClientOperationTests::
    rejectsInvalidOperationResults_data()
{
    QTest::addColumn<QVariantMap>("result");
    QTest::addColumn<bool>("resident");
    QTest::addColumn<bool>("action");

    QTest::newRow("revision-before-predates-dispatch")
        << operationResult(4, 6) << false << false;
    QTest::newRow("non-resident-action-does-not-advance")
        << operationResult(5, 5) << false << true;
    QVariantMap missingStatus = operationResult(5, 6);
    missingStatus.remove(QStringLiteral("status"));
    QTest::newRow("missing-required-field")
        << missingStatus << true << true;
}

void NotificationPresentationClientOperationTests::
    rejectsInvalidOperationResults()
{
    QFETCH(QVariantMap, result);
    QFETCH(bool, resident);
    QFETCH(bool, action);

    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    beginReadyClient(transport, client, QStringLiteral(":1.71"), 5, resident);
    QSignalSpy rejected(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationRejected);
    QSignalSpy succeeded(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationSucceeded);

    if (action) {
        QVERIFY(client.invokeAction(7, QStringLiteral("open"), {}, nullptr));
    } else {
        QVERIFY(client.dismiss(7, nullptr));
    }
    transport.finishRaw(transport.operations.constFirst(), result);

    QTRY_COMPARE_WITH_TIMEOUT(rejected.size(), 1, 100);
    QCOMPARE(rejected.constFirst().at(0).toUInt(), quint32(7));
    QCOMPARE(rejected.constFirst().at(1).toString(),
             QStringLiteral("notification operation reply is invalid"));
    QCOMPARE(succeeded.size(), 0);
    QVERIFY(!client.operationInFlight());
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    QCOMPARE(transport.requests.constLast().type, RequestType::Snapshot);
}

void NotificationPresentationClientOperationTests::
    timeoutRecoversAndIgnoresLateReply()
{
    FakeTransport transport;
    auto timing = fastTiming();
    timing.requestTimeoutMilliseconds = 100;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), timing);
    beginReadyClient(transport, client, QStringLiteral(":1.72"), 1, true);
    QSignalSpy rejected(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationRejected);
    QSignalSpy succeeded(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationSucceeded);

    QVERIFY(client.invokeAction(7, QStringLiteral("open"), {}, nullptr));
    const RecordedOperation timedOut = transport.operations.constFirst();
    QVERIFY(rejected.wait(250));
    QCOMPARE(rejected.constFirst().at(1).toString(),
             QStringLiteral("notification operation timed out"));
    QVERIFY(!client.operationInFlight());
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 50);

    transport.finish(timedOut, 1, 1);
    QCoreApplication::processEvents();
    QCOMPARE(succeeded.size(), 0);
    QCOMPARE(rejected.size(), 1);
    transport.reply(transport.requests.constLast(),
                    snapshotWire(QString::fromLatin1(Epoch), 1,
                                 QStringLiteral("Build complete"), true));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(),
                              NotificationPresentationClient::ClientState::Ready,
                              100);
}

void NotificationPresentationClientOperationTests::
    ownerChangeRejectsOperationAndIgnoresOldReply()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    beginReadyClient(transport, client, QStringLiteral(":1.73"), 1);
    QSignalSpy rejected(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationRejected);
    QSignalSpy succeeded(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationSucceeded);
    auto stateAtRejection = NotificationPresentationClient::ClientState::Ready;
    connect(&client,
            &NotificationPresentationClient::NotificationPresentationClient::
                operationRejected,
            &client, [&client, &stateAtRejection] {
                stateAtRejection = client.state();
            });

    QVERIFY(client.dismiss(7, nullptr));
    const RecordedOperation abandoned = transport.operations.constFirst();
    transport.announceOwner(QStringLiteral(":1.74"));

    QCOMPARE(client.state(),
             NotificationPresentationClient::ClientState::Authenticating);
    QVERIFY(!client.snapshot().has_value());
    QVERIFY(!client.operationInFlight());
    QCOMPARE(rejected.size(), 1);
    QCOMPARE(stateAtRejection,
             NotificationPresentationClient::ClientState::Authenticating);
    QCOMPARE(rejected.constFirst().at(1).toString(),
             QStringLiteral("notification service changed during operation"));
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    QCOMPARE(transport.requests.constLast().owner, QStringLiteral(":1.74"));
    QCOMPARE(transport.requests.constLast().type, RequestType::Register);

    transport.finish(abandoned, 1, 2);
    QCoreApplication::processEvents();
    QCOMPARE(succeeded.size(), 0);
    QCOMPARE(rejected.size(), 1);
    transport.reply(transport.requests.constLast(),
                    snapshotWire(QStringLiteral(
                                     "88888888-8888-8888-8888-888888888888"),
                                 1));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(),
                              NotificationPresentationClient::ClientState::Ready,
                              100);
}

void NotificationPresentationClientOperationTests::
    operationFailureForcesSnapshotAfterConcurrentRequest()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    const QString owner = QStringLiteral(":1.75");
    beginReadyClient(transport, client, owner, 1);
    transport.invalidate(owner, QString::fromLatin1(Epoch), 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);

    QVERIFY(client.dismiss(7, nullptr));
    transport.reject(transport.operations.constFirst(),
                     QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                     QStringLiteral("injected failure"));
    QCOMPARE(transport.requests.size(), 2);
    transport.reply(transport.requests.at(1),
                    snapshotWire(QString::fromLatin1(Epoch), 2));

    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    QCOMPARE(transport.requests.constLast().type, RequestType::Snapshot);
}

void NotificationPresentationClientOperationTests::
    accessDenialCancelsConcurrentRequestAndReauthenticates()
{
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    const QString owner = QStringLiteral(":1.76");
    beginReadyClient(transport, client, owner, 1);
    transport.invalidate(owner, QString::fromLatin1(Epoch), 2);
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
    const RecordedRequest unauthorizedSnapshot = transport.requests.at(1);
    auto stateAtRejection = NotificationPresentationClient::ClientState::Ready;
    connect(&client,
            &NotificationPresentationClient::NotificationPresentationClient::
                operationRejected,
            &client, [&client, &stateAtRejection] {
                stateAtRejection = client.state();
            });

    QVERIFY(client.dismiss(7, nullptr));
    transport.reject(transport.operations.constFirst(),
                     QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                     QStringLiteral("presenter binding expired"));
    QCOMPARE(client.state(),
             NotificationPresentationClient::ClientState::Authenticating);
    QCOMPARE(stateAtRejection,
             NotificationPresentationClient::ClientState::Authenticating);
    QVERIFY(!client.snapshot().has_value());
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 3, 100);
    QCOMPARE(transport.requests.constLast().type, RequestType::Register);

    transport.reply(unauthorizedSnapshot,
                    snapshotWire(QString::fromLatin1(Epoch), 2));
    QVERIFY(!client.snapshot().has_value());
    transport.reply(transport.requests.constLast(),
                    snapshotWire(QString::fromLatin1(Epoch), 2));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(),
                              NotificationPresentationClient::ClientState::Ready,
                              100);
}

void NotificationPresentationClientOperationTests::
    presentsNormalizedRemoteErrors_data()
{
    QTest::addColumn<QString>("remoteMessage");
    QTest::addColumn<QString>("exactMessage");
    QTest::addColumn<QString>("prefix");
    QTest::addColumn<int>("expectedLength");

    QTest::newRow("blank-uses-fallback")
        << QStringLiteral(" \t ")
        << QStringLiteral("notification operation failed") << QString{} << 29;
    QTest::newRow("hostile-text-is-sanitized-and-bounded")
        << (QStringLiteral("  failure") + QChar::Null +
            QChar(char16_t(0xd800)) + QString(700, QLatin1Char('x')) +
            QStringLiteral("  "))
        << QString{} << QStringLiteral("failure\ufffd\ufffd") << 512;
}

void NotificationPresentationClientOperationTests::
    presentsNormalizedRemoteErrors()
{
    QFETCH(QString, remoteMessage);
    QFETCH(QString, exactMessage);
    QFETCH(QString, prefix);
    QFETCH(int, expectedLength);

    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, accessToken(), fastTiming());
    beginReadyClient(transport, client, QStringLiteral(":1.77"), 1);
    QSignalSpy rejected(
        &client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationRejected);

    QVERIFY(client.dismiss(7, nullptr));
    transport.reject(transport.operations.constFirst(),
                     QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                     remoteMessage);

    QCOMPARE(rejected.size(), 1);
    const QString presented = rejected.constFirst().at(1).toString();
    QCOMPARE(presented.size(), expectedLength);
    QVERIFY(!presented.contains(QChar::Null));
    if (!exactMessage.isEmpty()) {
        QCOMPARE(presented, exactMessage);
    } else {
        QVERIFY(presented.startsWith(prefix));
        QCOMPARE(presented.back(), QChar(0x2026));
    }
    QTRY_COMPARE_WITH_TIMEOUT(transport.requests.size(), 2, 100);
}

QTEST_GUILESS_MAIN(NotificationPresentationClientOperationTests)

#include "tst_notification_presentation_client_operations.moc"
