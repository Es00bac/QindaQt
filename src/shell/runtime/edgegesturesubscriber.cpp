// SPDX-License-Identifier: GPL-3.0-or-later
#include "edgegesturesubscriber.h"

#include <QDBusMessage>
#include <QDBusPendingCall>

namespace QindaQt::Shell {
namespace {

constexpr auto CompositorService = "org.qindaqt.Compositor";
constexpr auto CompositorPath = "/org/qindaqt/Compositor";
constexpr auto CompositorInterface = "org.qindaqt.Compositor1";
constexpr auto SignalName = "EdgeGestureTriggered";

} // namespace

bool dispatchEdgeGesture(const QString &action, const EdgeGestureHandlers &handlers)
{
    if (action == QLatin1String("overview") && handlers.overview) {
        handlers.overview();
        return true;
    }
    if (action == QLatin1String("notifications") && handlers.notifications) {
        handlers.notifications();
        return true;
    }
    if (action == QLatin1String("task-switcher") && handlers.taskSwitcher) {
        handlers.taskSwitcher();
        return true;
    }
    return false;
}

EdgeGestureSubscriber::EdgeGestureSubscriber(EdgeGestureHandlers handlers, QDBusConnection connection,
                                             QObject *parent)
    : QObject(parent), m_handlers(std::move(handlers)), m_connection(std::move(connection))
{
    // Matched by path and interface, not by owner: the compositor may restart
    // and re-acquire its name; the subscription follows the object.
    m_subscribed = m_connection.isConnected()
        && m_connection.connect(QString(), QString::fromLatin1(CompositorPath),
                                QString::fromLatin1(CompositorInterface), QString::fromLatin1(SignalName),
                                this, SLOT(handleGesture(QString, QString)));
    if (!m_subscribed) {
        qWarning("QindaQt shell: touch edge gestures unavailable (no compositor signal subscription)");
    }
}

EdgeGestureSubscriber::~EdgeGestureSubscriber()
{
    if (m_subscribed) {
        m_connection.disconnect(QString(), QString::fromLatin1(CompositorPath),
                                QString::fromLatin1(CompositorInterface), QString::fromLatin1(SignalName),
                                this, SLOT(handleGesture(QString, QString)));
    }
}

void EdgeGestureSubscriber::handleGesture(const QString &edge, const QString &action)
{
    const bool handled = dispatchEdgeGesture(action, m_handlers);
    Q_EMIT gestureReceived(edge, action, handled);
}

void invokeWalkThroughWindows(const QDBusConnection &connection)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"), QStringLiteral("/component/kwin"),
        QStringLiteral("org.kde.kglobalaccel.Component"), QStringLiteral("invokeShortcut"));
    call.setArguments({QStringLiteral("Walk Through Windows")});
    connection.asyncCall(call);
}

} // namespace QindaQt::Shell
