// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/session_actions/session_actions_client.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtCore/QUuid>
#include <QtTest>

using namespace QindaQt::Services::SessionActions;

namespace {

class FakeScreenSaver final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

public:
    int lockCount = 0;
    bool holdReply = false;

public Q_SLOTS:
    void Lock()
    {
        ++lockCount;
        if (holdReply && calledFromDBus()) {
            setDelayedReply(true);
        }
    }
};

class FakeSession final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Session1")

public:
    bool allowed = true;
    int canCount = 0;
    int logoutCount = 0;

public Q_SLOTS:
    bool CanLogout()
    {
        ++canCount;
        return allowed;
    }
    void Logout() { ++logoutCount; }
};

class FakeLogind final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.login1.Manager")

public:
    QString suspendAnswer = QStringLiteral("yes");
    QString rebootAnswer = QStringLiteral("no");
    QString powerOffAnswer = QStringLiteral("challenge");
    int canSuspendCount = 0;
    int suspendCount = 0;
    int rebootCount = 0;
    int powerOffCount = 0;
    bool lastInteractive = true;

public Q_SLOTS:
    QString CanSuspend()
    {
        ++canSuspendCount;
        return suspendAnswer;
    }
    QString CanReboot() const { return rebootAnswer; }
    QString CanPowerOff() const { return powerOffAnswer; }
    void Suspend(bool interactive)
    {
        ++suspendCount;
        lastInteractive = interactive;
    }
    void Reboot(bool interactive)
    {
        ++rebootCount;
        lastInteractive = interactive;
    }
    void PowerOff(bool interactive)
    {
        ++powerOffCount;
        lastInteractive = interactive;
    }
};

class PrivateServices final {
public:
    PrivateServices()
        : clientConnectionName(
              QStringLiteral("qindaqt-session-actions-test-%1")
                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
        , bus(QDBusConnection::sessionBus())
        , clientBus(QDBusConnection::connectToBus(
              QDBusConnection::SessionBus, clientConnectionName))
    {
        const auto flags = QDBusConnection::ExportAllSlots;
        QVERIFY(bus.isConnected());
        QVERIFY(clientBus.isConnected());
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.ScreenSaver")));
        QVERIFY(bus.registerObject(QStringLiteral("/ScreenSaver"), &screenSaver, flags));
        QVERIFY(bus.registerService(QStringLiteral("org.qindaqt.Session1")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/qindaqt/Session1"), &session, flags));
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.login1")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/login1"), &logind, flags));
    }

    ~PrivateServices()
    {
        bus.unregisterObject(QStringLiteral("/ScreenSaver"));
        bus.unregisterObject(QStringLiteral("/org/qindaqt/Session1"));
        bus.unregisterObject(QStringLiteral("/org/freedesktop/login1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.ScreenSaver"));
        bus.unregisterService(QStringLiteral("org.qindaqt.Session1"));
        bus.unregisterService(QStringLiteral("org.freedesktop.login1"));
        QDBusConnection::disconnectFromBus(clientConnectionName);
    }

    QString clientConnectionName;
    QDBusConnection bus;
    QDBusConnection clientBus;
    FakeScreenSaver screenSaver;
    FakeSession session;
    FakeLogind logind;
};

} // namespace

class SessionActionsClientTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void canChecksPublishTypedFailClosedTruth();
    void dispatchRepeatsAdmissionAndSerializes();
    void mutationTimeoutIsUncertainAndNeverReplays();
    void ownerLossWithdrawsAvailabilityWithoutPolling();
};

void SessionActionsClientTest::canChecksPublishTypedFailClosedTruth()
{
    PrivateServices services;
    SessionActionsClient client(services.clientBus, services.clientBus);
    client.start();
    QTRY_VERIFY(client.canLock());
    QTRY_VERIFY(client.canLogout());
    QTRY_VERIFY(client.canSuspend());
    QVERIFY(!client.canReboot());
    QVERIFY(!client.canPowerOff());

    const SessionActionAvailability expected{
        .lock = true, .logout = true, .suspend = true,
        .reboot = false, .powerOff = false};
    QCOMPARE(client.availability(), expected);
    QVERIFY(services.session.canCount >= 1);
    QVERIFY(services.logind.canSuspendCount >= 1);
    QVERIFY(!client.requestReboot());
    QCOMPARE(services.logind.rebootCount, 0);
    QVERIFY(client.feedback().contains(QStringLiteral("unavailable")));
}

void SessionActionsClientTest::dispatchRepeatsAdmissionAndSerializes()
{
    PrivateServices services;
    SessionActionsClient client(services.clientBus, services.clientBus);
    QSignalSpy finished(&client, &SessionActionsClient::actionFinished);
    client.start();
    QTRY_VERIFY(client.canSuspend());
    const int baselineCan = services.logind.canSuspendCount;

    QVERIFY(client.requestSuspend());
    QVERIFY(client.pending());
    QVERIFY(!client.requestLock());
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(services.logind.canSuspendCount >= baselineCan + 1);
    QCOMPARE(services.logind.suspendCount, 1);
    QVERIFY(!services.logind.lastInteractive);
    QVERIFY(!client.pending());
    const auto result = qvariant_cast<SessionActionResult>(finished.first().first());
    QCOMPARE(result.action, SessionAction::Suspend);
    QCOMPARE(result.status, ActionStatus::Succeeded);

    QVERIFY(client.requestLock());
    QTRY_COMPARE(services.screenSaver.lockCount, 1);
    QTRY_COMPARE(finished.size(), 2);

    const int baselineLogoutCan = services.session.canCount;
    QVERIFY(client.requestLogout());
    QTRY_COMPARE(services.session.logoutCount, 1);
    QTRY_COMPARE(finished.size(), 3);
    QVERIFY(services.session.canCount >= baselineLogoutCan + 1);
}

void SessionActionsClientTest::ownerLossWithdrawsAvailabilityWithoutPolling()
{
    PrivateServices services;
    SessionActionsClient client(services.clientBus, services.clientBus);
    client.start();
    QTRY_VERIFY(client.canSuspend());
    const int baselineCan = services.logind.canSuspendCount;
    QTest::qWait(50);
    QCOMPARE(services.logind.canSuspendCount, baselineCan);

    services.bus.unregisterObject(QStringLiteral("/org/freedesktop/login1"));
    services.bus.unregisterService(QStringLiteral("org.freedesktop.login1"));
    QTRY_VERIFY(!client.canSuspend());
    QVERIFY(!client.canReboot());
    QVERIFY(!client.canPowerOff());
    QVERIFY(!client.requestSuspend());
    QCOMPARE(services.logind.suspendCount, 0);
}

void SessionActionsClientTest::mutationTimeoutIsUncertainAndNeverReplays()
{
    PrivateServices services;
    services.screenSaver.holdReply = true;
    SessionActionsClient client(services.clientBus, services.clientBus);
    QSignalSpy finished(&client, &SessionActionsClient::actionFinished);
    client.start();
    QTRY_VERIFY(client.canLock());

    QVERIFY(client.requestLock());
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1,
                              SessionActionsClient::ActionTimeoutMilliseconds + 1'000);
    QCOMPARE(services.screenSaver.lockCount, 1);
    const auto result = qvariant_cast<SessionActionResult>(finished.first().first());
    QCOMPARE(result.action, SessionAction::Lock);
    QCOMPARE(result.status, ActionStatus::Uncertain);
    QCOMPARE(result.reasonCode, QStringLiteral("request-timeout"));
    QTest::qWait(50);
    QCOMPARE(services.screenSaver.lockCount, 1);
}

QTEST_GUILESS_MAIN(SessionActionsClientTest)
#include "tst_session_actions_client.moc"
