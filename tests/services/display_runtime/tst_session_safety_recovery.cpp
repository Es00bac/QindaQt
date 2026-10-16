// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0379: Display1's logind delay authority survives back-to-back suspends
// and slow/failed Inhibit replies instead of ending the process.

#include "../../../src/services/display_runtime/src/qt_session_safety_port_p.h"
#include "../display_service/support/private_bus_test_support.h"

#include <qindaqt/compositor_names/compositor_names.h>

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusUnixFileDescriptor>
#include <QtDBus/QDBusVirtualObject>
#include <QtTest/QTest>

#include <fcntl.h>
#include <unistd.h>

using namespace QindaQt;
using namespace QindaQt::DisplayRuntime;
using namespace QindaQt::DisplayService::TestSupport;
using QindaQt::Platform::Recovery::ManualRecoveryTimer;

namespace
{

// Compositor native lock: always unlocked, answered with nonce receipts.
class NativeLock final : public QDBusVirtualObject
{
public:
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.member() != QStringLiteral("RequestStateWithReceipt")) {
            return false;
        }
        connection.send(message.createReply());
        auto receipt = QDBusMessage::createTargetedSignal(
            message.service(), QString(CompositorNames::nativeLockPath),
            QString(CompositorNames::nativeLockInterface), QStringLiteral("stateReceipt"));
        receipt << message.arguments().first().toString() << false << false;
        connection.send(receipt);
        return true;
    }
};

class Logind final : public QDBusVirtualObject
{
public:
    explicit Logind(QDBusConnection connection) : bus(std::move(connection)) {}
    ~Logind() override
    {
        for (int fd : std::as_const(readEnds)) {
            ::close(fd);
        }
    }
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override
    {
        if (message.member() != QStringLiteral("Inhibit")) {
            return false;
        }
        ++inhibitCalls;
        if (failInhibits > 0) {
            --failInhibits;
            bus.send(message.createErrorReply(QDBusError::NoReply, QStringLiteral("slow logind")));
        } else if (deferInhibit) {
            deferred = message;
        } else {
            grant(message);
        }
        return true;
    }
    void grant(const QDBusMessage &message)
    {
        int descriptors[2] = {-1, -1};
        QVERIFY(::pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) == 0);
        readEnds.append(descriptors[0]);
        QDBusUnixFileDescriptor fd(descriptors[1]);
        ::close(descriptors[1]);
        bus.send(message.createReply(QVariant::fromValue(fd)));
    }
    void prepare(bool preparing)
    {
        auto signal = QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/login1"),
                                                 QStringLiteral("org.freedesktop.login1.Manager"),
                                                 QStringLiteral("PrepareForSleep"));
        signal << preparing;
        bus.send(signal);
    }
    QDBusConnection bus;
    QDBusMessage deferred;
    QList<int> readEnds;
    int inhibitCalls = 0, failInhibits = 0;
    bool deferInhibit = false;
};

class Observer final : public SessionSafetyObserver
{
public:
    void sessionSafetyReady() override { ++ready; }
    void sessionSafetyChanged(DisplayTransaction::SafetyState) override {}
    void sessionPreparingForSleep() override { ++preparing; }
    void sessionAuthorityLost(const QString &reason) override { lost.append(reason); }
    QStringList lost;
    int ready = 0, preparing = 0;
};

struct Fixture {
    Fixture()
    {
        QString error;
        started = bus.start(&error);
        lockBus = QDBusConnection::connectToBus(bus.address(), lockName);
        logindBus = QDBusConnection::connectToBus(bus.address(), logindName);
        session = QDBusConnection::connectToBus(bus.address(), sessionName);
        system = QDBusConnection::connectToBus(bus.address(), systemName);
        logind = std::make_unique<Logind>(logindBus);
    }
    bool claim()
    {
        return lockBus.registerVirtualObject(QString(CompositorNames::nativeLockPath), &lock)
            && lockBus.registerService(QString(CompositorNames::service))
            && logindBus.registerVirtualObject(QStringLiteral("/org/freedesktop/login1"),
                                               logind.get())
            && logindBus.registerService(QStringLiteral("org.freedesktop.login1"));
    }
    std::unique_ptr<SessionSafetyPort> port()
    {
        auto created = makeQtSessionSafetyPort(session, system, [this] {
            auto timer = std::make_unique<ManualRecoveryTimer>();
            timers.append(timer.get());
            return timer;
        });
        created->setObserver(&observer);
        return created;
    }
    ~Fixture()
    {
        logind.reset();
        for (const QString &name : {lockName, logindName, sessionName, systemName}) {
            QDBusConnection::disconnectFromBus(name);
        }
        lockBus = logindBus = session = system = QDBusConnection(QStringLiteral("released"));
        bus.stop();
    }
    PrivateSessionBus bus;
    bool started = false;
    const QString lockName = privateConnectionName(QStringLiteral("recovery-lock"));
    const QString logindName = privateConnectionName(QStringLiteral("recovery-logind"));
    const QString sessionName = privateConnectionName(QStringLiteral("recovery-session"));
    const QString systemName = privateConnectionName(QStringLiteral("recovery-system"));
    QDBusConnection lockBus{QStringLiteral("unset")}, logindBus{QStringLiteral("unset")};
    QDBusConnection session{QStringLiteral("unset")}, system{QStringLiteral("unset")};
    NativeLock lock;
    std::unique_ptr<Logind> logind;
    Observer observer;
    QList<ManualRecoveryTimer *> timers;
};

} // namespace

class SessionSafetyRecoveryTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void backToBackSuspendIsToleratedAndDelayReacquired()
    {
        Fixture f;
        QVERIFY(f.started);
        QVERIFY(f.claim());
        auto safety = f.port();
        QCOMPARE(safety->start(QCoreApplication::applicationPid()),
                 SessionSafetyStartStatus::Started);
        QTRY_COMPARE(f.observer.ready, 1);
        QTRY_COMPARE(safety->currentSafety(), DisplayTransaction::SafetyState::Safe);
        f.logind->prepare(true);
        QTRY_COMPARE(f.observer.preparing, 1);
        safety->releaseSleepDelay();
        // Resume starts a fresh Inhibit; logind suspends again before it lands.
        f.logind->deferInhibit = true;
        f.logind->prepare(false);
        QTRY_COMPARE(f.logind->inhibitCalls, 2);
        f.logind->prepare(true);
        QTRY_COMPARE(f.observer.preparing, 2);
        QVERIFY(f.observer.lost.isEmpty()); // was "sleep-without-delay-authority"
        QVERIFY(!safety->delayHeld());
        // The late grant during the new sleep is dropped, not held.
        f.logind->deferInhibit = false;
        f.logind->grant(f.logind->deferred);
        QTest::qWait(100);
        QVERIFY(!safety->delayHeld());
        QCOMPARE(f.observer.ready, 1);
        f.logind->prepare(false);
        QTRY_COMPARE(f.observer.ready, 2);
        QVERIFY(safety->delayHeld());
        QTRY_COMPARE(safety->currentSafety(), DisplayTransaction::SafetyState::Safe);
        QVERIFY(f.observer.lost.isEmpty());
        safety->stop();
    }

    void inhibitFailureRetriesWithBackoff()
    {
        Fixture f;
        QVERIFY(f.started);
        f.logind->failInhibits = 2;
        QVERIFY(f.claim());
        auto safety = f.port();
        QCOMPARE(safety->start(QCoreApplication::applicationPid()),
                 SessionSafetyStartStatus::Started);
        auto &timer = *f.timers.first();
        QTRY_VERIFY(timer.isActive()); // was "logind-inhibit-failed": exit
        QVERIFY(f.observer.lost.isEmpty());
        QCOMPARE(f.observer.ready, 0);
        QVERIFY(timer.fire());
        QTRY_COMPARE(timer.requested().size(), 2);
        QVERIFY(timer.fire());
        QTRY_COMPARE(f.observer.ready, 1);
        QVERIFY(safety->delayHeld());
        QCOMPARE(f.logind->inhibitCalls, 3);
        using std::chrono::milliseconds;
        QCOMPARE(timer.requested(), (QList<milliseconds>{milliseconds{250}, milliseconds{500}}));
        QTRY_COMPARE(safety->currentSafety(), DisplayTransaction::SafetyState::Safe);
        safety->stop();
    }
};

QTEST_GUILESS_MAIN(SessionSafetyRecoveryTest)

#include "tst_session_safety_recovery.moc"
