// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_notifications/notification_presenter.h>

#include <QtCore/QHash>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Link {

// Presents phone notifications on org.freedesktop.Notifications of the
// injected bus. Uses what QindaQt's server renders: app_icon and the
// image-path hint both carry the cached PNG path, actions are (key, label)
// pairs. The body is plain text unless the server advertises `body-markup` in GetCapabilities
// (asked once at construction); until that answer arrives, and whenever it cannot be read, the
// body and summary are escaped as if markup were supported, so a server that renders <a> or <img>
// never gets phone-controlled markup. NotificationClosed reason 2 is the only
// user dismissal.
//
// Error behavior: with no server on the bus nothing is shown and handles
// stay valid. Threading: the bus connection's thread.
class FdoPhoneNotificationPresenter final : public NotificationPresenter {
    Q_OBJECT
public:
    explicit FdoPhoneNotificationPresenter(QDBusConnection bus, QObject *parent = nullptr);
    quint32 present(const PresentedNotification &n, quint32 replaces) override;
    void close(quint32 handle) override;
    [[nodiscard]] bool serverRendersMarkup() const { return m_markup; }
    [[nodiscard]] static QString escapeMarkup(const QString &text);

private Q_SLOTS:
    void onActionInvoked(uint serverId, const QString &key);
    void onClosed(uint serverId, uint reason);

private:
    struct Slot {
        uint serverId = 0;  // 0 until the Notify reply arrives
        bool closeWhenKnown = false;
    };
    QDBusConnection m_bus;
    quint32 m_next = 0;
    bool m_markup = true;  // pessimistic until GetCapabilities says otherwise
    QHash<quint32, Slot> m_slots;
    QHash<uint, quint32> m_byServerId;
};

} // namespace QindaQt::Link
