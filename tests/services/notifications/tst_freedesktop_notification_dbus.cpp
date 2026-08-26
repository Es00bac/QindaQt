// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/services/notifications/freedesktop_notification_server.h"

#include "support/notification_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>
#include <QUuid>

using namespace QindaQt::Services::Notifications;
using namespace QindaQt::Services::Notifications::TestSupport;

namespace {

constexpr auto ServiceName = "org.qindaqt.TestNotifications";
constexpr auto ObjectPath = "/org/freedesktop/Notifications";
constexpr auto InterfaceName = "org.freedesktop.Notifications";

class PrivateSessionBus final {
public:
    ~PrivateSessionBus()
    {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.terminate();
            if (!m_process.waitForFinished(1'000)) {
                m_process.kill();
                m_process.waitForFinished(1'000);
            }
        }
    }

    bool start(QString *error)
    {
        m_process.start(QStringLiteral("dbus-daemon"),
                        {QStringLiteral("--session"),
                         QStringLiteral("--nofork"),
                         QStringLiteral("--nopidfile"),
                         QStringLiteral("--print-address=1")});
        if (!m_process.waitForStarted(5'000) || !m_process.waitForReadyRead(5'000)) {
            *error = m_process.errorString();
            return false;
        }
        m_address = QString::fromUtf8(m_process.readLine()).trimmed();
        if (m_address.isEmpty()) {
            *error = QStringLiteral("private dbus-daemon did not publish an address");
            return false;
        }
        return true;
    }

    [[nodiscard]] const QString &address() const noexcept { return m_address; }

private:
    QProcess m_process;
    QString m_address;
};

class ProtocolSignalReceiver final : public QObject {
    Q_OBJECT

public slots:
    void notificationClosed(uint id, uint reason)
    {
        closedIds.push_back(id);
        closeReasons.push_back(reason);
    }

    void actionInvoked(uint id, const QString &key)
    {
        actionIds.push_back(id);
        actionKeys.push_back(key);
    }

    void activationToken(uint id, const QString &token)
    {
        tokenIds.push_back(id);
        tokens.push_back(token);
    }

public:
    QVector<uint> closedIds;
    QVector<uint> closeReasons;
    QVector<uint> actionIds;
    QStringList actionKeys;
    QVector<uint> tokenIds;
    QStringList tokens;
};

QString connectionName(const QString &role)
{
    return QStringLiteral("qindaqt-notification-%1-%2")
        .arg(role, QUuid::createUuid().toString(QUuid::Id128));
}

QVariantList notifyArguments(quint32 replacesId,
                             QString summary,
                             QStringList actions = {},
                             QVariantMap hints = {})
{
    return {
        QStringLiteral("Protocol Test"),
        replacesId,
        QStringLiteral("test-icon"),
        std::move(summary),
        QStringLiteral("Body"),
        std::move(actions),
        std::move(hints),
        0,
    };
}

QDBusPendingCall call(const QDBusConnection &connection,
                      const QString &method,
                      const QVariantList &arguments = {})
{
    auto message = QDBusMessage::createMethodCall(QString::fromLatin1(ServiceName),
                                                  QString::fromLatin1(ObjectPath),
                                                  QString::fromLatin1(InterfaceName),
                                                  method);
    message.setArguments(arguments);
    return connection.asyncCall(message, 5'000);
}

void verifyIdentityAndInitialSubmission(
    const QDBusConnection &client,
    FreedesktopNotificationServer &server,
    RecordingNotificationBackend &presentationBackend,
    quint32 *notificationId)
{
    QDBusPendingCallWatcher informationWatcher(call(
        client, QStringLiteral("GetServerInformation")));
    QTRY_VERIFY_WITH_TIMEOUT(informationWatcher.isFinished(), 5'000);
    const QDBusPendingReply<QString, QString, QString, QString> information(
        informationWatcher);
    QVERIFY2(information.isValid(), qPrintable(information.error().message()));
    QCOMPARE(information.argumentAt<0>(), QStringLiteral("QindaQt"));
    QCOMPARE(information.argumentAt<2>(), QStringLiteral("test-version"));
    QCOMPARE(information.argumentAt<3>(), QStringLiteral("1.3"));

    QDBusPendingCallWatcher capabilitiesWatcher(call(
        client, QStringLiteral("GetCapabilities")));
    QTRY_VERIFY_WITH_TIMEOUT(capabilitiesWatcher.isFinished(), 5'000);
    const QDBusPendingReply<QStringList> capabilities(capabilitiesWatcher);
    QVERIFY(capabilities.isValid());
    QCOMPARE(capabilities.value(),
             QStringList({QStringLiteral("body"), QStringLiteral("actions")}));

    QVariantMap criticalHint;
    criticalHint.insert(QStringLiteral("urgency"), QVariant::fromValue(uchar(2)));
    QDBusPendingCallWatcher notifyWatcher(call(
        client,
        QStringLiteral("Notify"),
        notifyArguments(0,
                        QStringLiteral("Original"),
                        {QStringLiteral("open"), QStringLiteral("Open")},
                        criticalHint)));
    QTRY_VERIFY_WITH_TIMEOUT(notifyWatcher.isFinished(), 5'000);
    const QDBusPendingReply<quint32> notified(notifyWatcher);
    QVERIFY2(notified.isValid(), qPrintable(notified.error().message()));
    *notificationId = notified.value();
    QVERIFY(*notificationId != 0);
    QCOMPARE(server.service().snapshot()->notifications.size(), 1);
    QCOMPARE(presentationBackend.publications.size(), 1);
    QCOMPARE(server.service().snapshot()->notifications.first().hints.urgency,
             Urgency::Critical);
    const QString authenticatedOwner = server.service().snapshot()
                                           ->notifications.first()
                                           .sourceService;
    QVERIFY(authenticatedOwner.startsWith(QLatin1Char(':')));
}

void verifyOwnershipAndAction(
    const QDBusConnection &secondClient,
    FreedesktopNotificationServer &server,
    RecordingNotificationBackend &presentationBackend,
    ProtocolSignalReceiver &receiver,
    quint32 notificationId)
{
    QDBusPendingCallWatcher hostileWatcher(call(
        secondClient,
        QStringLiteral("Notify"),
        notifyArguments(notificationId, QStringLiteral("Hostile replacement"))));
    QTRY_VERIFY_WITH_TIMEOUT(hostileWatcher.isFinished(), 5'000);
    const QDBusPendingReply<quint32> hostile(hostileWatcher);
    QVERIFY(hostile.isError());
    QCOMPARE(hostile.error().name(),
             QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));
    QCOMPARE(server.service().snapshot()->notifications.first().summary,
             QStringLiteral("Original"));

    const auto action = server.service().invokeAction(
        notificationId, QStringLiteral("open"), QStringLiteral("activation-token"));
    QVERIFY(action.ok());
    QTRY_COMPARE_WITH_TIMEOUT(receiver.actionIds.size(), 1, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(receiver.tokenIds.size(), 1, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(receiver.closedIds.size(), 1, 5'000);
    QCOMPARE(receiver.actionIds.first(), notificationId);
    QCOMPARE(receiver.actionKeys.first(), QStringLiteral("open"));
    QCOMPARE(receiver.tokens.first(), QStringLiteral("activation-token"));
    QCOMPARE(receiver.closeReasons.first(), quint32(CloseReason::DismissedByUser));
    QCOMPARE(presentationBackend.actions.size(), 1);
    QCOMPARE(presentationBackend.closures.size(), 1);
}

void verifyProtocolErrorsAndClose(
    const QDBusConnection &client,
    FreedesktopNotificationServer &server,
    ProtocolSignalReceiver &receiver,
    quint32 notificationId)
{
    QDBusPendingCallWatcher malformedWatcher(call(
        client,
        QStringLiteral("Notify"),
        notifyArguments(0,
                        QStringLiteral("Odd actions"),
                        {QStringLiteral("missing-label")})));
    QTRY_VERIFY_WITH_TIMEOUT(malformedWatcher.isFinished(), 5'000);
    const QDBusPendingReply<quint32> malformed(malformedWatcher);
    QVERIFY(malformed.isError());
    QCOMPARE(malformed.error().name(),
             QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
    QVERIFY(server.service().snapshot()->notifications.isEmpty());

    QDBusPendingCallWatcher inactiveReplacementWatcher(call(
        client,
        QStringLiteral("Notify"),
        notifyArguments(notificationId, QStringLiteral("Reuses inactive ID"))));
    QTRY_VERIFY_WITH_TIMEOUT(inactiveReplacementWatcher.isFinished(), 5'000);
    const QDBusPendingReply<quint32> inactiveReplacement(inactiveReplacementWatcher);
    QVERIFY(inactiveReplacement.isValid());
    QCOMPARE(inactiveReplacement.value(), notificationId);
    QCOMPARE(server.service().snapshot()->notifications.first().id, notificationId);
    QDBusPendingCallWatcher closeReusedWatcher(call(
        client, QStringLiteral("CloseNotification"), {notificationId}));
    QTRY_VERIFY_WITH_TIMEOUT(closeReusedWatcher.isFinished(), 5'000);
    QVERIFY(QDBusPendingReply<>(closeReusedWatcher).isValid());

    QDBusPendingCallWatcher closeMissingWatcher(call(
        client, QStringLiteral("CloseNotification"), {notificationId}));
    QTRY_VERIFY_WITH_TIMEOUT(closeMissingWatcher.isFinished(), 5'000);
    const QDBusPendingReply<> missingClose(closeMissingWatcher);
    QVERIFY(missingClose.isError());
    QCOMPARE(missingClose.error().name(),
             QStringLiteral("org.freedesktop.Notifications.Error.InvalidNotification"));
    QVERIFY(closeMissingWatcher.reply().arguments().isEmpty());
    QVERIFY(closeMissingWatcher.reply().signature().isEmpty());

    QDBusPendingCallWatcher secondNotifyWatcher(call(
        client,
        QStringLiteral("Notify"),
        notifyArguments(0, QStringLiteral("Close me"))));
    QTRY_VERIFY_WITH_TIMEOUT(secondNotifyWatcher.isFinished(), 5'000);
    const QDBusPendingReply<quint32> secondNotification(secondNotifyWatcher);
    QVERIFY(secondNotification.isValid());
    QDBusPendingCallWatcher closeWatcher(call(
        client, QStringLiteral("CloseNotification"), {secondNotification.value()}));
    QTRY_VERIFY_WITH_TIMEOUT(closeWatcher.isFinished(), 5'000);
    const QDBusPendingReply<> closed(closeWatcher);
    QVERIFY2(closed.isValid(), qPrintable(closed.error().message()));
    QTRY_COMPARE_WITH_TIMEOUT(receiver.closedIds.size(), 3, 5'000);
    QCOMPARE(receiver.closeReasons.last(), quint32(CloseReason::ClosedByApplication));
}

} // namespace

class FreedesktopNotificationDBusTests final : public QObject {
    Q_OBJECT

private slots:
    void protocolOwnsRequestsAndEmitsStandardSignals();
};

void FreedesktopNotificationDBusTests::protocolOwnsRequestsAndEmitsStandardSignals()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));

    const QString serverConnectionName = connectionName(QStringLiteral("server"));
    const QString firstClientName = connectionName(QStringLiteral("client-a"));
    const QString secondClientName = connectionName(QStringLiteral("client-b"));
    auto serverConnection = QDBusConnection::connectToBus(bus.address(),
                                                          serverConnectionName);
    auto firstClient = QDBusConnection::connectToBus(bus.address(), firstClientName);
    auto secondClient = QDBusConnection::connectToBus(bus.address(), secondClientName);
    QVERIFY(serverConnection.isConnected());
    QVERIFY(firstClient.isConnected());
    QVERIFY(secondClient.isConnected());

    ManualNotificationClock clock;
    RecordingNotificationBackend presentationBackend;
    FreedesktopServerIdentity identity;
    QCOMPARE(identity.capabilities, QStringList({QStringLiteral("body")}));
    identity.version = QStringLiteral("test-version");
    identity.capabilities.push_back(QStringLiteral("actions"));
    FreedesktopNotificationServer server(serverConnection,
                                          clock,
                                          {},
                                          identity,
                                          &presentationBackend);
    QVERIFY2(server.start(QString::fromLatin1(ServiceName)),
             qPrintable(server.lastError()));

    ProtocolSignalReceiver receiver;
    QVERIFY(firstClient.connect(QString::fromLatin1(ServiceName),
                                QString::fromLatin1(ObjectPath),
                                QString::fromLatin1(InterfaceName),
                                QStringLiteral("NotificationClosed"),
                                &receiver,
                                SLOT(notificationClosed(uint,uint))));
    QVERIFY(firstClient.connect(QString::fromLatin1(ServiceName),
                                QString::fromLatin1(ObjectPath),
                                QString::fromLatin1(InterfaceName),
                                QStringLiteral("ActionInvoked"),
                                &receiver,
                                SLOT(actionInvoked(uint,QString))));
    QVERIFY(firstClient.connect(QString::fromLatin1(ServiceName),
                                QString::fromLatin1(ObjectPath),
                                QString::fromLatin1(InterfaceName),
                                QStringLiteral("ActivationToken"),
                                &receiver,
                                SLOT(activationToken(uint,QString))));

    quint32 notificationId = 0;
    verifyIdentityAndInitialSubmission(firstClient,
                                       server,
                                       presentationBackend,
                                       &notificationId);
    verifyOwnershipAndAction(secondClient,
                             server,
                             presentationBackend,
                             receiver,
                             notificationId);
    verifyProtocolErrorsAndClose(firstClient, server, receiver, notificationId);

    server.stop();
    QVERIFY(!server.isRunning());
    QDBusConnection::disconnectFromBus(secondClientName);
    QDBusConnection::disconnectFromBus(firstClientName);
    QDBusConnection::disconnectFromBus(serverConnectionName);
}

QTEST_GUILESS_MAIN(FreedesktopNotificationDBusTests)
#include "tst_freedesktop_notification_dbus.moc"
