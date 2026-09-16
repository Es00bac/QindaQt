// SPDX-License-Identifier: GPL-3.0-or-later

// The specification's host side of registration (ADR-0166): a conformant item
// asks the watcher whether a host exists before it presents itself, and is
// entitled to hide or fall back to XEmbed when none does. These rows pin the
// behavior that was missing on a live QindaQt session, where the shell served
// the watcher but `IsStatusNotifierHostRegistered` stayed false.

#include <qindaqt/shell/status_notifier/host/status_notifier_host_registration.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h>

#include "status_notifier_private_bus_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QStandardPaths>
#include <QtTest>

#include <memory>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifier::TestSupport;

namespace
{

// Reads IsStatusNotifierHostRegistered the way an item does: over the bus,
// through Properties.Get, from a connection that is neither the watcher's nor
// the host's.
[[nodiscard]] bool readHostRegisteredProperty(QDBusConnection &connection, bool *ok)
{
    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    message << QString::fromLatin1(kWatcherInterfaceName)
            << QStringLiteral("IsStatusNotifierHostRegistered");
    QDBusPendingCall pending = connection.asyncCall(message);
    QDBusPendingCallWatcher watcher(pending);
    while (!pending.isFinished()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    const QDBusMessage reply = pending.reply();
    *ok = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty();
    if (!*ok) {
        return false;
    }
    return reply.arguments().constFirst().value<QDBusVariant>().variant().toBool();
}

} // namespace

class StatusNotifierHostRegistrationTests final : public QObject
{
    Q_OBJECT

private slots:
    void conventionalNameFollowsTheSpecShape();
    void watcherReportsNoHostUntilOneRegisters();
    void registersWithAWatcherThatStartsLater();
    void dropsRegistrationWhenTheWatcherDisappears();
    void failsClosedWithoutABusConnection();
};

void StatusNotifierHostRegistrationTests::conventionalNameFollowsTheSpecShape()
{
    const QString name = StatusNotifierHostRegistration::conventionalHostServiceName();
    QVERIFY(name.startsWith(QStringLiteral("org.kde.StatusNotifierHost-")));
    QCOMPARE(name, QStringLiteral("org.kde.StatusNotifierHost-%1")
                       .arg(QCoreApplication::applicationPid()));
}

// The regression: with a live watcher and NO host registration the property an
// item consults is false; starting the host flips it to true and the watcher
// lists the host's unique name.
void StatusNotifierHostRegistrationTests::watcherReportsNoHostUntilOneRegisters()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-watcher-a"));
    auto hostConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-host-a"));
    auto itemConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-item-a"));

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    bool ok = false;
    QCOMPARE(readHostRegisteredProperty(itemConnection, &ok), false);
    QVERIFY(ok);
    QVERIFY(watcher.registeredHosts().isEmpty());

    StatusNotifierHostRegistration host(hostConnection,
                                        QStringLiteral("org.kde.StatusNotifierHost-test-a"));
    QVERIFY2(host.start(&error), qPrintable(error));
    QVERIFY(host.ownsHostName());
    QTRY_VERIFY_WITH_TIMEOUT(host.isRegisteredWithWatcher(), 5'000);

    QCOMPARE(readHostRegisteredProperty(itemConnection, &ok), true);
    QVERIFY(ok);
    QCOMPARE(watcher.registeredHosts().size(), 1);
    QCOMPARE(watcher.registeredHosts().constFirst(), hostConnection.baseService());

    QDBusConnection::disconnectFromBus(QStringLiteral("host-watcher-a"));
    QDBusConnection::disconnectFromBus(QStringLiteral("host-host-a"));
    QDBusConnection::disconnectFromBus(QStringLiteral("host-item-a"));
}

// Shell startup order is not guaranteed: the host may claim its name before any
// watcher owns the watcher name. That must not strand the registration.
void StatusNotifierHostRegistrationTests::registersWithAWatcherThatStartsLater()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto hostConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-host-b"));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-watcher-b"));

    StatusNotifierHostRegistration host(hostConnection,
                                        QStringLiteral("org.kde.StatusNotifierHost-test-b"));
    // No watcher on the bus yet: starting succeeds and stays unregistered.
    QVERIFY2(host.start(&error), qPrintable(error));
    QVERIFY(host.ownsHostName());
    QCOMPARE(host.isRegisteredWithWatcher(), false);

    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));

    QTRY_VERIFY_WITH_TIMEOUT(host.isRegisteredWithWatcher(), 5'000);
    QCOMPARE(watcher.registeredHosts().size(), 1);

    QDBusConnection::disconnectFromBus(QStringLiteral("host-host-b"));
    QDBusConnection::disconnectFromBus(QStringLiteral("host-watcher-b"));
}

void StatusNotifierHostRegistrationTests::dropsRegistrationWhenTheWatcherDisappears()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto hostConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-host-c"));
    auto watcherConnection = connectToPrivateBus(bus.address(), QStringLiteral("host-watcher-c"));

    auto watcher = std::make_unique<StatusNotifierWatcherService>(watcherConnection);
    QVERIFY2(watcher->start(&error), qPrintable(error));
    StatusNotifierHostRegistration host(hostConnection,
                                        QStringLiteral("org.kde.StatusNotifierHost-test-c"));
    QVERIFY2(host.start(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(host.isRegisteredWithWatcher(), 5'000);

    // A replacement watcher starts with an empty host set, so the host must
    // stop claiming a registration it no longer has.
    watcher->stop();
    QTRY_COMPARE_WITH_TIMEOUT(host.isRegisteredWithWatcher(), false, 5'000);

    // ... and must re-register when a watcher returns.
    auto replacement = std::make_unique<StatusNotifierWatcherService>(watcherConnection);
    QVERIFY2(replacement->start(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(host.isRegisteredWithWatcher(), 5'000);
    QCOMPARE(replacement->registeredHosts().size(), 1);

    QDBusConnection::disconnectFromBus(QStringLiteral("host-host-c"));
    QDBusConnection::disconnectFromBus(QStringLiteral("host-watcher-c"));
}

void StatusNotifierHostRegistrationTests::failsClosedWithoutABusConnection()
{
    StatusNotifierHostRegistration host(QDBusConnection(QStringLiteral("not-connected")),
                                        QStringLiteral("org.kde.StatusNotifierHost-test-d"));
    QString error;
    QCOMPARE(host.start(&error), false);
    QCOMPARE(error, QStringLiteral("host-bus-disconnected"));
    QCOMPARE(host.ownsHostName(), false);
    QCOMPARE(host.isRegisteredWithWatcher(), false);
}

QTEST_MAIN(StatusNotifierHostRegistrationTests)
#include "tst_status_notifier_host_registration.moc"
