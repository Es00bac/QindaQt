// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationbinding.h"

#include <KGlobalAccel>
#include <KGlobalShortcutInfo>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QKeySequence>
#include <QStringList>

namespace QindaQt::Test {
namespace {

constexpr auto GlobalAccelService = "org.kde.kglobalaccel";
constexpr auto GlobalAccelInterface = "org.kde.KGlobalAccel";
constexpr auto GlobalAccelComponentInterface =
    "org.kde.kglobalaccel.Component";
constexpr auto NotificationComponent = "qindaqt-shell";
constexpr auto NotificationAction = "qindaqt_toggle_notification_center";

} // namespace

QString desktopNotificationComponentId()
{
    return QString::fromLatin1(NotificationComponent);
}

QString desktopNotificationActionId()
{
    return QString::fromLatin1(NotificationAction);
}

int desktopNotificationMetaN()
{
    return QKeySequence(Qt::META | Qt::Key_N)[0].toCombined();
}

bool desktopNotificationBindingReady(
    const DesktopNotificationBinding &binding)
{
    const int expected = desktopNotificationMetaN();
    return binding.componentUniqueName == desktopNotificationComponentId()
        && binding.uniqueName == desktopNotificationActionId()
        && binding.componentResolved
        && !binding.componentObjectPath.isEmpty()
        && binding.componentActiveReplyValid
        && binding.componentActive
        && binding.defaultKeys.contains(expected)
        && binding.activeKeys.contains(expected);
}

bool desktopNotificationActivationComplete(
    const QList<DesktopNotificationActivationEvent> &events)
{
    if (events.size() != 2) {
        return false;
    }
    const auto exactIdentity = [](const DesktopNotificationActivationEvent &event) {
        return event.componentUniqueName == desktopNotificationComponentId()
            && event.actionUniqueName == desktopNotificationActionId();
    };
    return events.at(0).edge == DesktopNotificationActivationEdge::Pressed
        && exactIdentity(events.at(0))
        && events.at(1).edge == DesktopNotificationActivationEdge::Released
        && exactIdentity(events.at(1));
}

QString desktopNotificationActivationDiagnostic(
    const QList<DesktopNotificationActivationEvent> &events)
{
    if (desktopNotificationActivationComplete(events)) {
        return {};
    }
    QStringList observed;
    observed.reserve(events.size());
    for (const auto &event : events) {
        observed.append(QStringLiteral("%1:%2/%3")
                            .arg(event.edge
                                         == DesktopNotificationActivationEdge::Pressed
                                     ? QStringLiteral("pressed")
                                     : QStringLiteral("released"),
                                 event.componentUniqueName,
                                 event.actionUniqueName));
    }
    if (observed.isEmpty()) {
        observed.append(QStringLiteral("none"));
    }
    return QStringLiteral(
               "expected pressed then released for exact %1/%2; observed %3")
        .arg(desktopNotificationComponentId(), desktopNotificationActionId(),
             observed.join(QLatin1Char(',')));
}

std::optional<DesktopNotificationBinding>
queryDesktopNotificationBinding(QString *error)
{
    const QKeySequence expected(Qt::META | Qt::Key_N);
    std::optional<KGlobalShortcutInfo> found;
    for (const auto &shortcut : KGlobalAccel::globalShortcutsByKey(expected)) {
        if (shortcut.componentUniqueName() == desktopNotificationComponentId()
            && shortcut.uniqueName() == desktopNotificationActionId()) {
            found = shortcut;
            break;
        }
    }
    if (!found) {
        *error = QStringLiteral(
            "KGlobalAccel has not published the exact qindaqt-shell notification Meta+N action");
        return std::nullopt;
    }

    const QStringList actionId{
        found->componentUniqueName(), found->uniqueName(),
        found->componentFriendlyName(), found->friendlyName()};
    QDBusInterface globalAccel(QString::fromLatin1(GlobalAccelService),
                               QStringLiteral("/kglobalaccel"),
                               QString::fromLatin1(GlobalAccelInterface));
    const QDBusReply<QList<int>> defaults =
        globalAccel.call(QStringLiteral("defaultShortcut"), actionId);
    const QDBusReply<QList<int>> activeKeys =
        globalAccel.call(QStringLiteral("shortcut"), actionId);
    if (!defaults.isValid() || !activeKeys.isValid()) {
        *error = QStringLiteral(
            "KGlobalAccel did not return default and active notification bindings");
        return std::nullopt;
    }

    const QDBusReply<QDBusObjectPath> componentObject = globalAccel.call(
        QStringLiteral("getComponent"), found->componentUniqueName());
    const bool componentResolved = componentObject.isValid()
        && !componentObject.value().path().isEmpty()
        && componentObject.value().path() != QStringLiteral("/");
    if (!componentResolved) {
        *error = QStringLiteral(
            "KGlobalAccel did not resolve the exact qindaqt-shell component object");
        return std::nullopt;
    }
    QDBusInterface component(QString::fromLatin1(GlobalAccelService),
                             componentObject.value().path(),
                             QString::fromLatin1(GlobalAccelComponentInterface));
    const QDBusReply<bool> componentActive =
        component.call(QStringLiteral("isActive"));

    DesktopNotificationBinding binding{
        found->componentUniqueName(),
        found->uniqueName(),
        componentObject.value().path(),
        true,
        componentActive.isValid(),
        componentActive.isValid() && componentActive.value(),
        defaults.value(),
        activeKeys.value(),
    };
    if (!desktopNotificationBindingReady(binding)) {
        *error = QStringLiteral(
            "KGlobalAccel has not published an active exact-component default and active Meta+N binding");
        return std::nullopt;
    }
    error->clear();
    return binding;
}

DesktopNotificationActivationObserver::DesktopNotificationActivationObserver(
    QObject *parent)
    : QObject(parent)
{
}

bool DesktopNotificationActivationObserver::start(
    QDBusConnection connection,
    const DesktopNotificationBinding &binding,
    QString *error)
{
    if (!desktopNotificationBindingReady(binding)) {
        *error = QStringLiteral(
            "cannot observe activation for an unqualified notification binding");
        return false;
    }
    const QString service = QString::fromLatin1(GlobalAccelService);
    const QString interface = QString::fromLatin1(GlobalAccelComponentInterface);
    const bool pressed = connection.connect(
        service, binding.componentObjectPath, interface,
        QStringLiteral("globalShortcutPressed"), this,
        SLOT(shortcutPressed(QString,QString,qlonglong)));
    const bool released = connection.connect(
        service, binding.componentObjectPath, interface,
        QStringLiteral("globalShortcutReleased"), this,
        SLOT(shortcutReleased(QString,QString,qlonglong)));
    if (!pressed || !released) {
        *error = QStringLiteral(
            "could not observe exact KGlobalAccel component activation signals");
        return false;
    }
    m_events.clear();
    error->clear();
    return true;
}

bool DesktopNotificationActivationObserver::complete() const
{
    return desktopNotificationActivationComplete(m_events);
}

QString DesktopNotificationActivationObserver::diagnostic() const
{
    return desktopNotificationActivationDiagnostic(m_events);
}

void DesktopNotificationActivationObserver::shortcutPressed(
    const QString &componentUniqueName,
    const QString &actionUniqueName,
    qlonglong timestamp)
{
    Q_UNUSED(timestamp)
    m_events.append({DesktopNotificationActivationEdge::Pressed,
                     componentUniqueName, actionUniqueName});
}

void DesktopNotificationActivationObserver::shortcutReleased(
    const QString &componentUniqueName,
    const QString &actionUniqueName,
    qlonglong timestamp)
{
    Q_UNUSED(timestamp)
    m_events.append({DesktopNotificationActivationEdge::Released,
                     componentUniqueName, actionUniqueName});
}

} // namespace QindaQt::Test
