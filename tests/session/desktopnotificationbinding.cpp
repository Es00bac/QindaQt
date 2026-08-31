// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationbinding.h"

#include <KGlobalAccel>
#include <KGlobalShortcutInfo>

#include <QDBusInterface>
#include <QDBusReply>
#include <QKeySequence>

namespace QindaQt::Test {
namespace {

constexpr auto GlobalAccelService = "org.kde.kglobalaccel";
constexpr auto NotificationAction = "qindaqt_toggle_notification_center";

} // namespace

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
    return binding.uniqueName == desktopNotificationActionId()
        && binding.defaultKeys.contains(expected)
        && binding.activeKeys.contains(expected);
}

std::optional<DesktopNotificationBinding>
queryDesktopNotificationBinding(QString *error)
{
    const QKeySequence expected(Qt::META | Qt::Key_N);
    std::optional<KGlobalShortcutInfo> found;
    for (const auto &shortcut : KGlobalAccel::globalShortcutsByKey(expected)) {
        if (shortcut.uniqueName() == desktopNotificationActionId()) {
            found = shortcut;
            break;
        }
    }
    if (!found) {
        *error = QStringLiteral(
            "KGlobalAccel has not published the notification Meta+N action");
        return std::nullopt;
    }

    const QStringList actionId{
        found->componentUniqueName(), found->uniqueName(),
        found->componentFriendlyName(), found->friendlyName()};
    QDBusInterface globalAccel(QString::fromLatin1(GlobalAccelService),
                               QStringLiteral("/kglobalaccel"),
                               QStringLiteral("org.kde.KGlobalAccel"));
    const QDBusReply<QList<int>> defaults =
        globalAccel.call(QStringLiteral("defaultShortcut"), actionId);
    const QDBusReply<QList<int>> active =
        globalAccel.call(QStringLiteral("shortcut"), actionId);
    if (!defaults.isValid() || !active.isValid()) {
        *error = QStringLiteral(
            "KGlobalAccel did not return default and active notification bindings");
        return std::nullopt;
    }

    DesktopNotificationBinding binding{
        found->uniqueName(), defaults.value(), active.value()};
    if (!desktopNotificationBindingReady(binding)) {
        *error = QStringLiteral(
            "KGlobalAccel has not published exact default and active Meta+N bindings");
        return std::nullopt;
    }
    error->clear();
    return binding;
}

} // namespace QindaQt::Test
