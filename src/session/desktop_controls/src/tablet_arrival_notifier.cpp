// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/desktop_controls/tablet_arrival_notifier.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace QindaQt::Session::DesktopControls {
namespace {

constexpr auto kServiceName = "org.freedesktop.Notifications";
constexpr auto kObjectPath = "/org/freedesktop/Notifications";
constexpr auto kInterfaceName = "org.freedesktop.Notifications";
// A device announcement is a thing to act on, not a flash of feedback; it
// waits the notification host's default time rather than the media-key
// 1.2 seconds.
constexpr int ExpireDefault = -1;

} // namespace

TabletArrivalNotifier::TabletArrivalNotifier(QDBusConnection connection,
                                             QObject *parent)
    : QObject(parent), m_connection(std::move(connection)) {}

TabletArrivalNotifier::~TabletArrivalNotifier() = default;

QString TabletArrivalNotifier::setupActionKey() {
    return QStringLiteral("setup");
}

QString TabletArrivalNotifier::useInternalActionKey() {
    return QStringLiteral("internal");
}

QString TabletArrivalNotifier::dismissActionKey() {
    return QStringLiteral("dismiss");
}

QString TabletArrivalNotifier::bodyText(const QString &outputName) {
    if (outputName.isEmpty()) {
        return QObject::tr("The pen follows whichever screen is active. "
                           "Choose a screen in Pen & tablet settings.");
    }
    return QObject::tr("The pen now draws on %1.").arg(outputName);
}

bool TabletArrivalNotifier::start(QString *error) {
    if (m_started) {
        return true;
    }
    if (!m_connection.isConnected()) {
        if (error != nullptr) {
            *error = QStringLiteral("No session bus for tablet notifications");
        }
        return false;
    }
    // AGENT-GUARD: No sender filter. The notification host may not own the
    // name yet when this process starts, and a sender-filtered match
    // registered too early never fires afterwards.
    const bool connected = m_connection.connect(
        QString(), QLatin1String(kObjectPath), QLatin1String(kInterfaceName),
        QStringLiteral("ActionInvoked"), this,
        SLOT(handleActionInvoked(uint, QString)));
    if (!connected) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Could not observe notification actions; the pen display "
                "announcement's buttons will do nothing");
        }
        return false;
    }
    m_started = true;
    return true;
}

void TabletArrivalNotifier::announce(const QString &deviceGroupId,
                                     const QString &deviceName,
                                     const QString &outputName) {
    if (deviceGroupId.isEmpty()) {
        return;
    }
    const quint32 replacesId = m_idsByGroup.value(deviceGroupId, 0);
    const QString summary =
        deviceName.isEmpty()
            ? QObject::tr("Pen tablet connected")
            : QObject::tr("%1 connected").arg(deviceName);
    const QStringList actions{
        setupActionKey(),       QObject::tr("Set up pen display"),
        useInternalActionKey(), QObject::tr("Use the active screen instead"),
        dismissActionKey(),     QObject::tr("Not now"),
    };
    QDBusMessage message = QDBusMessage::createMethodCall(
        QLatin1String(kServiceName), QLatin1String(kObjectPath),
        QLatin1String(kInterfaceName), QStringLiteral("Notify"));
    message.setArguments({
        QStringLiteral("QindaQt"),
        replacesId,
        QStringLiteral("input-tablet"),
        summary,
        bodyText(outputName),
        actions,
        QVariantMap{},
        ExpireDefault,
    });
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, deviceGroupId, replacesId, watcher] {
                const QDBusPendingReply<quint32> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError()) {
                    // A lost or absent host must not resurrect a stale id
                    // against a future server instance.
                    m_idsByGroup.remove(deviceGroupId);
                    m_liveNotifications.remove(replacesId);
                    Q_EMIT notificationFailed(
                        QStringLiteral("tablet-announcement-failed: %1")
                            .arg(reply.error().message()));
                    return;
                }
                const quint32 id = reply.value();
                if (replacesId != 0 && replacesId != id) {
                    m_liveNotifications.remove(replacesId);
                }
                m_idsByGroup.insert(deviceGroupId, id);
                m_liveNotifications.insert(id, deviceGroupId);
                Q_EMIT announcementShown(deviceGroupId, id);
            });
}

void TabletArrivalNotifier::handleActionInvoked(quint32 notificationId,
                                                const QString &actionKey) {
    const QString deviceGroupId = m_liveNotifications.value(notificationId);
    if (deviceGroupId.isEmpty()) {
        // Another application's notification; this process sees every
        // ActionInvoked on the bus and must ignore what is not its own.
        return;
    }
    if (actionKey == setupActionKey()) {
        Q_EMIT setupRequested(deviceGroupId);
    } else if (actionKey == useInternalActionKey()) {
        Q_EMIT useActiveScreenRequested(deviceGroupId);
    }
    // "Not now" closes the popup and records nothing: the tablet is already
    // marked announced, so it will not ask again on its own.
    m_liveNotifications.remove(notificationId);
    m_idsByGroup.remove(deviceGroupId);
}

} // namespace QindaQt::Session::DesktopControls
