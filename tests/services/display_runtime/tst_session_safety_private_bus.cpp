// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_runtime/session_safety_port.h>
#include <qindaqt/compositor_names/compositor_names.h>

#include "../display_service/support/private_bus_test_support.h"

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusUnixFileDescriptor>
#include <QtTest/QTest>

#include <unistd.h>
#include <utility>

using namespace QindaQt;
using namespace QindaQt::DisplayRuntime;
using namespace QindaQt::DisplayService::TestSupport;

namespace
{

class NativeLockBackend final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.KWin.NativeLock1")

public:
    explicit NativeLockBackend(QDBusConnection connection)
        : m_connection(std::move(connection))
    {
    }

    void setLocked(bool locked, bool protectedPresentation)
    {
        m_locked = locked;
        m_protected = protectedPresentation;
        Q_EMIT lockedChanged(locked);
        Q_EMIT protectedChanged(protectedPresentation);
    }

public Q_SLOTS:
    void RequestStateWithReceipt(const QString &nonce)
    {
        auto receipt = QDBusMessage::createTargetedSignal(
            message().service(), QString(CompositorNames::nativeLockPath),
            QString(CompositorNames::nativeLockInterface),
            QStringLiteral("stateReceipt"));
        receipt << nonce << m_locked << m_protected;
        m_connection.send(receipt);
    }

Q_SIGNALS:
    void lockedChanged(bool locked);
    void protectedChanged(bool protectedPresentation);

private:
    QDBusConnection m_connection;
    bool m_locked = false;
    bool m_protected = false;
};

class LogindBackend final : public QObject
{
    Q_OBJECT
};

class LogindAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.login1.Manager")

public:
    explicit LogindAdaptor(LogindBackend *backend)
        : QDBusAbstractAdaptor(backend)
    {
    }

public Q_SLOTS:
    QDBusUnixFileDescriptor Inhibit(const QString &what, const QString &who,
                                    const QString &why, const QString &mode)
    {
        ++inhibitCalls;
        lastArguments = {what, who, why, mode};
        int descriptors[2] = {-1, -1};
        if (::pipe(descriptors) != 0) {
            return {};
        }
        QDBusUnixFileDescriptor result(descriptors[0]);
        (void)::close(descriptors[0]);
        (void)::close(descriptors[1]);
        return result;
    }

Q_SIGNALS:
    void PrepareForSleep(bool preparing);

public:
    QStringList lastArguments;
    int inhibitCalls = 0;
};

class RecordingObserver final : public SessionSafetyObserver
{
public:
    void sessionSafetyReady() override { ++readyCount; }

    void sessionSafetyChanged(
        const DisplayTransaction::SafetyState safety) override
    {
        states.push_back(safety);
    }

    void sessionPreparingForSleep() override { ++prepareCount; }

    void sessionAuthorityLost(const QString &reasonCode) override
    {
        lostReasons.push_back(reasonCode);
    }

    QList<DisplayTransaction::SafetyState> states;
    QStringList lostReasons;
    int readyCount = 0;
    int prepareCount = 0;
};

} // namespace

class SessionSafetyPrivateBusTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void authenticatesLockAndOwnsExactLogindDelayLifetime();
};

void SessionSafetyPrivateBusTest::
    authenticatesLockAndOwnsExactLogindDelayLifetime()
{
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    const QString lockName = privateConnectionName(QStringLiteral("lock-owner"));
    const QString logindName =
        privateConnectionName(QStringLiteral("logind-owner"));
    const QString sessionName =
        privateConnectionName(QStringLiteral("safety-session"));
    const QString systemName =
        privateConnectionName(QStringLiteral("safety-system"));
    QDBusConnection lockOwner =
        QDBusConnection::connectToBus(bus.address(), lockName);
    QDBusConnection logindOwner =
        QDBusConnection::connectToBus(bus.address(), logindName);
    QDBusConnection sessionClient =
        QDBusConnection::connectToBus(bus.address(), sessionName);
    QDBusConnection systemClient =
        QDBusConnection::connectToBus(bus.address(), systemName);
    QVERIFY(lockOwner.isConnected());
    QVERIFY(logindOwner.isConnected());
    QVERIFY(sessionClient.isConnected());
    QVERIFY(systemClient.isConnected());
    QVERIFY(lockOwner.registerService(QString(CompositorNames::service)));
    // Native sessions own the compatibility facades separately; they cannot
    // participate in authentication of the compositor's lock receipt.
    QVERIFY(logindOwner.registerService(QStringLiteral("org.freedesktop.ScreenSaver")));
    QVERIFY(logindOwner.registerService(QStringLiteral("org.kde.screensaver")));
    QVERIFY(logindOwner.registerService(
        QStringLiteral("org.freedesktop.login1")));

    NativeLockBackend nativeLock(lockOwner);
    QVERIFY(lockOwner.registerObject(QString(CompositorNames::nativeLockPath),
                                     &nativeLock,
                                     QDBusConnection::ExportAllSlots
                                         | QDBusConnection::ExportAllSignals));
    LogindBackend logind;
    auto *logindAdaptor = new LogindAdaptor(&logind);
    QVERIFY(logindOwner.registerObject(QStringLiteral("/org/freedesktop/login1"),
                                       &logind,
                                       QDBusConnection::ExportAdaptors));

    auto safety = makeQtSessionSafetyPort(sessionClient, systemClient);
    RecordingObserver observer;
    safety->setObserver(&observer);
    QCOMPARE(safety->start(QCoreApplication::applicationPid()),
             SessionSafetyStartStatus::Started);
    QTRY_COMPARE_WITH_TIMEOUT(observer.readyCount, 1, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(safety->currentSafety(),
                              DisplayTransaction::SafetyState::Safe, 5'000);
    QVERIFY(safety->delayHeld());
    QCOMPARE(logindAdaptor->inhibitCalls, 1);
    QCOMPARE(logindAdaptor->lastArguments,
             QStringList({QStringLiteral("sleep"),
                          QStringLiteral("QindaQt Display1"),
                          QStringLiteral("recover display preview before sleep"),
                          QStringLiteral("delay")}));

    // A different kernel peer PID must never inherit the native lock receipt.
    auto wrongPidSafety = makeQtSessionSafetyPort(sessionClient, systemClient);
    RecordingObserver wrongPidObserver;
    wrongPidSafety->setObserver(&wrongPidObserver);
    QCOMPARE(wrongPidSafety->start(QCoreApplication::applicationPid() + 100'000),
             SessionSafetyStartStatus::Started);
    QTRY_COMPARE_WITH_TIMEOUT(wrongPidObserver.readyCount, 1, 5'000);
    QCOMPARE(wrongPidSafety->currentSafety(),
             DisplayTransaction::SafetyState::Unknown);
    wrongPidSafety->stop();

    nativeLock.setLocked(true, false);
    QTRY_COMPARE_WITH_TIMEOUT(safety->currentSafety(),
                              DisplayTransaction::SafetyState::Locked, 5'000);
    nativeLock.setLocked(true, true);
    QTRY_COMPARE_WITH_TIMEOUT(safety->currentSafety(),
                              DisplayTransaction::SafetyState::Locked, 5'000);
    nativeLock.setLocked(false, false);
    QTRY_COMPARE_WITH_TIMEOUT(safety->currentSafety(),
                              DisplayTransaction::SafetyState::Safe, 5'000);

    Q_EMIT logindAdaptor->PrepareForSleep(true);
    QTRY_COMPARE_WITH_TIMEOUT(observer.prepareCount, 1, 5'000);
    QCOMPARE(safety->currentSafety(),
             DisplayTransaction::SafetyState::Unknown);
    QVERIFY(safety->delayHeld());
    safety->releaseSleepDelay();
    QVERIFY(!safety->delayHeld());
    Q_EMIT logindAdaptor->PrepareForSleep(false);
    QTRY_COMPARE_WITH_TIMEOUT(observer.readyCount, 2, 5'000);
    QTRY_VERIFY_WITH_TIMEOUT(safety->delayHeld(), 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(safety->currentSafety(),
                              DisplayTransaction::SafetyState::Safe, 5'000);

    QVERIFY(logindOwner.unregisterService(
        QStringLiteral("org.freedesktop.login1")));
    QTRY_COMPARE_WITH_TIMEOUT(observer.lostReasons.size(), 1, 5'000);
    QCOMPARE(observer.lostReasons.constFirst(),
             QStringLiteral("logind-owner-replaced"));
    QCOMPARE(safety->currentSafety(),
             DisplayTransaction::SafetyState::Unknown);
    QVERIFY(!safety->delayHeld());

    safety->stop();
    lockOwner.unregisterObject(QString(CompositorNames::nativeLockPath));
    logindOwner.unregisterObject(QStringLiteral("/org/freedesktop/login1"));
    QDBusConnection::disconnectFromBus(systemName);
    QDBusConnection::disconnectFromBus(sessionName);
    QDBusConnection::disconnectFromBus(logindName);
    QDBusConnection::disconnectFromBus(lockName);
    systemClient = QDBusConnection(QStringLiteral("released-system"));
    sessionClient = QDBusConnection(QStringLiteral("released-session"));
    logindOwner = QDBusConnection(QStringLiteral("released-logind"));
    lockOwner = QDBusConnection(QStringLiteral("released-lock"));
    bus.stop();
}

QTEST_GUILESS_MAIN(SessionSafetyPrivateBusTest)

#include "tst_session_safety_private_bus.moc"
