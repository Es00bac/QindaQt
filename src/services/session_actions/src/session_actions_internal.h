#pragma once
#include <QDBusMessage>
#include <QString>
namespace QindaQt::Services::SessionActions::Detail {
constexpr auto SessionService = "org.qindaqt.Session1";
constexpr auto SessionPath = "/org/qindaqt/Session1";
constexpr auto SessionInterface = "org.qindaqt.Session1";
constexpr auto ScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto ScreenSaverPath = "/ScreenSaver";
constexpr auto ScreenSaverInterface = "org.freedesktop.ScreenSaver";
constexpr auto LogindService = "org.freedesktop.login1";
constexpr auto LogindPath = "/org/freedesktop/login1";
constexpr auto LogindInterface = "org.freedesktop.login1.Manager";

constexpr auto ScreenPowerService = "org.qindaqt.ScreenPower1";
// AGENT-NOTE (ADR-0379): owners are cached from QDBusServiceWatcher events
// plus one asynchronous GetNameOwner at start. Never a blocking owner query
// and never the connection's shared interface timeout: the shell's UI thread
// answered every Sleep1.Changed with ~13 blocking GetNameOwner calls.
inline QDBusMessage daemonCall(const char *method, const QString &argument)
{
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QString::fromLatin1(method));
    message << argument;
    return message;
}

inline bool isSleepAction(SessionAction action) {
    return action == SessionAction::Suspend || action == SessionAction::Hibernate;
}
inline QString sleepMethod(SessionAction action) {
    return action == SessionAction::Hibernate ? QStringLiteral("Hibernate")
                                             : QStringLiteral("Suspend");
}

constexpr auto SleepService = "org.qindaqt.Sleep1";
constexpr auto SleepPath = "/org/qindaqt/Sleep1";
constexpr auto SleepInterface = "org.qindaqt.Sleep1";
}
