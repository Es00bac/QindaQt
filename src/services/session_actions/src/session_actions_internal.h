#pragma once
#include <QDBusConnectionInterface>
#include <QDBusReply>
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

inline QString serviceOwner(const QDBusConnection &connection, const char *service)
{
    if (!connection.isConnected() || connection.interface() == nullptr) {
        return {};
    }
    connection.interface()->setTimeout(250);
    const QDBusReply<QString> reply =
        connection.interface()->serviceOwner(QString::fromLatin1(service));
    return reply.isValid() ? reply.value() : QString{};
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
