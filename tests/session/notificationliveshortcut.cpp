// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationliveshortcut.h"

#include "notificationliveruntime.h"

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

std::optional<NotificationLiveShortcut>
discoverNotificationLiveShortcut(QString *error)
{
    const QKeySequence expected(Qt::META | Qt::Key_N);
    std::optional<KGlobalShortcutInfo> found;
    if (!awaitNotificationLiveCondition([&] {
            const auto shortcuts = KGlobalAccel::globalShortcutsByKey(expected);
            for (const auto &shortcut : shortcuts) {
                if (shortcut.uniqueName() == QLatin1String(NotificationAction)) {
                    found = shortcut;
                    return true;
                }
            }
            return false;
        })) {
        *error = QStringLiteral(
            "real KGlobalAccel did not publish default Meta+N for the notification action");
        return std::nullopt;
    }
    NotificationLiveShortcut result;
    result.actionId = {found->componentUniqueName(), found->uniqueName(),
                       found->componentFriendlyName(), found->friendlyName()};
    QDBusInterface globalAccel(QString::fromLatin1(GlobalAccelService),
                               QStringLiteral("/kglobalaccel"),
                               QStringLiteral("org.kde.KGlobalAccel"));
    const QDBusReply<QList<int>> defaults =
        globalAccel.call(QStringLiteral("defaultShortcut"), result.actionId);
    const QDBusReply<QList<int>> active =
        globalAccel.call(QStringLiteral("shortcut"), result.actionId);
    if (!defaults.isValid() || !active.isValid() || defaults.value().isEmpty()
        || active.value().isEmpty()
        || !defaults.value().contains(expected[0].toCombined())) {
        *error = QStringLiteral(
            "KGlobalAccel did not confirm the exact default and active bindings");
        return std::nullopt;
    }
    result.defaultKeys = defaults.value();
    result.activeKeys = active.value();
    return result;
}

bool setNotificationLiveShortcut(const NotificationLiveShortcut &shortcut,
                                 const QList<int> &keys, QString *error)
{
    QDBusInterface globalAccel(QString::fromLatin1(GlobalAccelService),
                               QStringLiteral("/kglobalaccel"),
                               QStringLiteral("org.kde.KGlobalAccel"));
    const QDBusReply<QList<int>> reply = globalAccel.call(
        QStringLiteral("setShortcut"), shortcut.actionId,
        QVariant::fromValue(keys),
        static_cast<uint>(KGlobalAccel::NoAutoloading));
    if (!reply.isValid() || reply.value() != keys) {
        *error = QStringLiteral("KGlobalAccel shortcut mutation failed: %1")
                     .arg(reply.error().message());
        return false;
    }
    return true;
}

QList<int> notificationLiveMetaShiftN()
{
    return {QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_N)[0].toCombined()};
}

} // namespace QindaQt::Test
