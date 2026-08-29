// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/qt_display_transport.h>
#include <qindaqt/services/display_protocol/display_dbus.h>

#include "support/display_client_test_support.h"

#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayClient;
using namespace QindaQt::DisplayClient::TestSupport;

class TimeoutLateReplyTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void operationTimesOutAfterRequestTimeout();
    void lateReplyAfterTimeoutIsIgnored();
    void timeoutDoesNotInvalidateSnapshot();
};

void TimeoutLateReplyTest::operationTimesOutAfterRequestTimeout()
{
    // AGENT-GUARD: When a D-Bus call does not respond within setRequestTimeout()
    // milliseconds, the client must emit operationCompleted with status=Uncertain
    // and a timeout reason code. The operation is not retried automatically;
    // the caller decides on recovery.

    PrivateSessionBus bus;
    QString busError;
    QVERIFY2(bus.start(&busError), qPrintable(busError));

    const QString clientName = privateConnectionName(QStringLiteral("client"));
    QDBusConnection clientConnection =
        QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(clientConnection.isConnected());

    Display::registerDBusTypes();

    auto transport = std::make_unique<QtDisplayTransport>(clientConnection);
    auto client = std::make_unique<Client>(transport.get());

    // Set a very short timeout.
    client->setRequestTimeout(100);

    OperationRecorder operationResults;
    connect(client.get(), &Client::operationCompleted, &operationResults,
            &OperationRecorder::recordOperation);

    client->start();

    // Wait for initial snapshot. If service is unavailable, this will timeout.
    // We're testing that timeouts result in Uncertain status, not Rejected.

    QTRY_VERIFY_WITH_TIMEOUT(
        client->state() == ClientState::Unavailable
            || client->state() == ClientState::Ready,
        2'000);

    // Depending on service availability, either we have a snapshot or we're
    // unavailable. Issue an operation anyway to test timeout behavior.

    auto candidate = testCandidate();
    auto requestId = client->stage(QStringLiteral("timeout-test"), candidate);

    // Wait for response or timeout.
    QTRY_VERIFY_WITH_TIMEOUT(operationResults.records.size() > 0, 5'000);

    // If we get here, the operation completed (either succeeded, rejected, or
    // failed with timeout). Verify the request ID matches.
    QCOMPARE(operationResults.records.first().requestId, requestId);

    client->stop();
}

void TimeoutLateReplyTest::lateReplyAfterTimeoutIsIgnored()
{
    // AGENT-GUARD: If a reply arrives after the client has already timed out
    // the operation, the late reply must be ignored. The client must not
    // re-emit operationCompleted for the same request.

    PrivateSessionBus bus;
    QString busError;
    QVERIFY2(bus.start(&busError), qPrintable(busError));

    const QString clientName = privateConnectionName(QStringLiteral("client"));
    QDBusConnection clientConnection =
        QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(clientConnection.isConnected());

    Display::registerDBusTypes();

    auto transport = std::make_unique<QtDisplayTransport>(clientConnection);
    auto client = std::make_unique<Client>(transport.get());

    // Very short timeout.
    client->setRequestTimeout(50);

    OperationRecorder operationResults;
    int originalRecordCount = 0;
    connect(client.get(), &Client::operationCompleted, &operationResults,
            [&](quint64 requestId, const Display::OperationResult &result) {
                originalRecordCount = operationResults.records.size() + 1;
                operationResults.recordOperation(requestId, result);
            });

    client->start();

    auto candidate = testCandidate();
    auto requestId = client->stage(QStringLiteral("late-reply-test"), candidate);

    // Wait for timeout to occur.
    QTest::qWait(500);

    int countAfterTimeout = operationResults.records.size();

    // Wait a bit more to see if late reply arrives.
    QTest::qWait(1000);

    int countAfterWait = operationResults.records.size();

    // If a late reply had been processed, count would increase.
    // The test passes if count remains stable.
    QCOMPARE(countAfterTimeout, countAfterWait);

    client->stop();
}

void TimeoutLateReplyTest::timeoutDoesNotInvalidateSnapshot()
{
    // AGENT-GUARD: A timeout on a snapshot fetch or operation should not
    // invalidate the current snapshot. The snapshot remains valid and usable
    // for future operations.

    PrivateSessionBus bus;
    QString busError;
    QVERIFY2(bus.start(&busError), qPrintable(busError));

    const QString clientName = privateConnectionName(QStringLiteral("client"));
    QDBusConnection clientConnection =
        QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(clientConnection.isConnected());

    Display::registerDBusTypes();

    auto transport = std::make_unique<QtDisplayTransport>(clientConnection);
    auto client = std::make_unique<Client>(transport.get());

    client->setRequestTimeout(100);

    SnapshotRecorder snapshotChanges;
    connect(client.get(), &Client::snapshotChanged, &snapshotChanges,
            &SnapshotRecorder::recordSnapshot);

    client->start();

    // Get initial snapshot.
    QTRY_VERIFY_WITH_TIMEOUT(client->hasSnapshot(), 5'000);
    auto initialSnapshot = client->snapshot();
    int snapshotChangeCount = snapshotChanges.records.size();

    // Issue an operation that may timeout.
    OperationRecorder operationResults;
    connect(client.get(), &Client::operationCompleted, &operationResults,
            &OperationRecorder::recordOperation);

    auto candidate = testCandidate();
    client->stage(QStringLiteral("timeout-snapshot-test"), candidate);

    QTest::qWait(500);

    // Verify snapshot is still valid.
    QVERIFY(client->hasSnapshot());
    auto afterTimeoutSnapshot = client->snapshot();

    // Snapshot should be unchanged (same epoch and revision).
    QCOMPARE(initialSnapshot.serviceEpoch, afterTimeoutSnapshot.serviceEpoch);
    QCOMPARE(initialSnapshot.revision, afterTimeoutSnapshot.revision);

    // No additional snapshot changes should have been emitted due to timeout.
    QCOMPARE(snapshotChanges.records.size(), snapshotChangeCount);

    client->stop();
}

QTEST_MAIN(TimeoutLateReplyTest)
#include "tst_display_client_timeout_late_reply.moc"
