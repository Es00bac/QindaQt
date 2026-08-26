// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_host/resident_notification_host.h"
#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"
#include "qindaqt/services/notification_presentation_client/qt_notification_presentation_transport.h"

#include "../notification_host/support/notification_host_test_support.h"

#include <QDBusConnection>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include <memory>

using namespace QindaQt::Services;
using namespace QindaQt::Services::NotificationHost;
using namespace QindaQt::Services::NotificationHost::TestSupport;
using namespace QindaQt::Services::NotificationPresentation;
using namespace QindaQt::Services::NotificationPresentationClient;

namespace {

ClientTiming fastTiming()
{
    return {.debounceMilliseconds = 1,
            .requestTimeoutMilliseconds = 1'000,
            .retryMilliseconds = {2, 5, 10}};
}

} // namespace

class QtNotificationPresentationTransportTests final : public QObject {
    Q_OBJECT

private slots:
    void followsTheAuthenticatedHostAcrossRestartAndActions();
};

void QtNotificationPresentationTransportTests::
    followsTheAuthenticatedHostAcrossRestartAndActions()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));

    const QString firstHostName = connectionName(QStringLiteral("client-host-a"));
    const QString clientName = connectionName(QStringLiteral("client"));
    auto firstHostConnection = QDBusConnection::connectToBus(bus.address(), firstHostName);
    auto clientConnection = QDBusConnection::connectToBus(bus.address(), clientName);
    QVERIFY(firstHostConnection.isConnected());
    QVERIFY(clientConnection.isConnected());

    const QString tokenText(64, QLatin1Char('c'));
    auto firstHostToken = PresentationAccessToken::fromHex(tokenText, &error);
    auto clientToken = PresentationAccessToken::fromHex(tokenText, &error);
    QVERIFY(firstHostToken.has_value());
    QVERIFY(clientToken.has_value());
    ManualNotificationClock firstClock;
    ManualDeadlineScheduler firstScheduler;
    RecordingNotificationBackend firstBackend;
    auto firstHost = std::make_unique<ResidentNotificationHost>(
        firstHostConnection, firstClock, firstScheduler,
        Notifications::NotificationPolicy{},
        Notifications::FreedesktopServerIdentity{}, &firstBackend,
        std::move(*firstHostToken));
    QVERIFY2(firstHost->start().ok(), "first notification host failed to start");

    QtNotificationPresentationTransport transport(clientConnection);
    NotificationPresentationClient::NotificationPresentationClient client(
        transport, std::move(*clientToken), fastTiming());
    QVERIFY2(client.start(&error), qPrintable(error));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
    QVERIFY(client.snapshot().has_value());
    QVERIFY(client.snapshot()->notifications.isEmpty());
    const QString firstEpoch = client.snapshot()->epoch;

    auto submitted = request(QStringLiteral(":1.800"), 0);
    submitted.summary = QStringLiteral("Transport integration");
    submitted.actions = {{QStringLiteral("open"), QStringLiteral("Open")}};
    const auto submission = firstHost->service().submit(submitted);
    QVERIFY2(submission.ok(), qPrintable(submission.message));
    QTRY_VERIFY_WITH_TIMEOUT(
        client.snapshot().has_value() &&
            client.snapshot()->notifications.size() == 1,
        5'000);
    QCOMPARE(client.snapshot()->notifications.constFirst().summary,
             QStringLiteral("Transport integration"));

    QSignalSpy operationSucceeded(&client,
        &NotificationPresentationClient::NotificationPresentationClient::
            operationSucceeded);
    QVERIFY(client.invokeAction(submission.notificationId, QStringLiteral("open"),
                                QStringLiteral("activation-token"), &error));
    QTRY_COMPARE_WITH_TIMEOUT(operationSucceeded.size(), 1, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(firstBackend.actions.size(), 1, 5'000);
    QCOMPARE(firstBackend.actions.constFirst().activationToken,
             QStringLiteral("activation-token"));
    QTRY_VERIFY_WITH_TIMEOUT(
        client.snapshot().has_value() && client.snapshot()->notifications.isEmpty(),
        5'000);

    firstHost->stop();
    firstHost.reset();
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Unavailable, 5'000);
    QVERIFY(!client.snapshot().has_value());
    QDBusConnection::disconnectFromBus(firstHostName);
    firstHostConnection = QDBusConnection(QStringLiteral("released-first-host"));

    const QString secondHostName = connectionName(QStringLiteral("client-host-b"));
    auto secondHostConnection = QDBusConnection::connectToBus(bus.address(), secondHostName);
    QVERIFY(secondHostConnection.isConnected());
    auto secondHostToken = PresentationAccessToken::fromHex(tokenText, &error);
    QVERIFY(secondHostToken.has_value());
    ManualNotificationClock secondClock;
    ManualDeadlineScheduler secondScheduler;
    RecordingNotificationBackend secondBackend;
    ResidentNotificationHost secondHost(
        secondHostConnection, secondClock, secondScheduler,
        Notifications::NotificationPolicy{},
        Notifications::FreedesktopServerIdentity{}, &secondBackend,
        std::move(*secondHostToken));
    QVERIFY2(secondHost.start().ok(), "second notification host failed to start");
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
    QVERIFY(client.snapshot().has_value());
    QVERIFY(client.snapshot()->notifications.isEmpty());
    QVERIFY(client.snapshot()->epoch != firstEpoch);

    client.stop();
    secondHost.stop();
    QDBusConnection::disconnectFromBus(secondHostName);
    QDBusConnection::disconnectFromBus(clientName);
    bus.stop();
    QVERIFY(bus.isStopped());
}

QTEST_GUILESS_MAIN(QtNotificationPresentationTransportTests)

#include "tst_qt_notification_presentation_transport.moc"
