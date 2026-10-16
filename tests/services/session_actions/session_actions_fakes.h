// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/session_actions/session_actions_client.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtCore/QScopeGuard>
#include <QtCore/QUuid>
#include <QtTest>

#include <optional>

// Private-broker fakes shared by the session-actions suites.
namespace SessionActionsFakes {

class FakeScreenSaver final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

public:
    int lockCount = 0;
    int getActiveCount = 0;
    bool holdReply = false;
    std::optional<QDBusMessage> delayedLockReply;

public Q_SLOTS:
    bool GetActive()
    {
        ++getActiveCount;
        return false;
    }

    void Lock()
    {
        ++lockCount;
        if (holdReply && calledFromDBus()) {
            setDelayedReply(true);
            delayedLockReply = message().createReply();
        }
    }

public:
    bool sendDelayedLockReply(const QDBusConnection &bus)
    {
        if (!delayedLockReply) {
            return false;
        }
        const bool sent = bus.send(*delayedLockReply);
        delayedLockReply.reset();
        return sent;
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

class FakeSleep final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Sleep1")
public:
    bool allowed = true;
    int canCount = 0, suspendCount = 0;
    bool hibernateAllowed = false;
    int canHibernateCount = 0, hibernateCount = 0;
public Q_SLOTS:
    bool CanSuspend() { ++canCount; return allowed; }
    bool CanHibernate() { ++canHibernateCount; return hibernateAllowed; }
    bool Hibernate() { ++hibernateCount; return hibernateAllowed; }
    bool Suspend() { ++suspendCount; return allowed; }
Q_SIGNALS:
    void Changed();
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
    explicit PrivateServices(bool registerScreenSaverObject = true)
        : clientConnectionName(
              QStringLiteral("qindaqt-session-actions-test-%1")
                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
        , bus(QDBusConnection::sessionBus())
        , clientBus(QDBusConnection::connectToBus(
              QDBusConnection::SessionBus, clientConnectionName))
    {
        const auto flags = QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals;
        QVERIFY(bus.isConnected());
        QVERIFY(clientBus.isConnected());
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.ScreenSaver")));
        if (registerScreenSaverObject) {
            QVERIFY(bus.registerObject(QStringLiteral("/ScreenSaver"), &screenSaver, flags));
        }
        QVERIFY(bus.registerService(QStringLiteral("org.qindaqt.Session1")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/qindaqt/Session1"), &session, flags));
        QVERIFY(bus.registerService(QStringLiteral("org.qindaqt.Sleep1")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/qindaqt/Sleep1"), &sleep, flags));
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.login1")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/login1"), &logind, flags));
    }

    ~PrivateServices()
    {
        bus.unregisterObject(QStringLiteral("/org/qindaqt/Sleep1"));
        bus.unregisterService(QStringLiteral("org.qindaqt.Sleep1"));
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
    FakeSleep sleep;
};

} // namespace SessionActionsFakes
