// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_notifications/app_settings.h>
#include <qindaqt/services/link_notifications/icon_cache.h>
#include <qindaqt/services/link_notifications/notification_feature.h>
#include <qindaqt/services/link_notifications/notifications_adaptor.h>
#include <qindaqt/services/link_transport/hub.h>

namespace QindaQt::Link {

struct NotificationsConfig {
    QString configDir;  // notifications.json lives here ($XDG_CONFIG_HOME/qindalink)
    QString cacheDir;   // icons/ lives here ($XDG_CACHE_HOME/qindalink)
    NotificationPresenter *presenter = nullptr;  // required, not owned
    ReplyLauncher *reply = nullptr;              // optional, not owned
};

// Composition of the notify capability for qindaqt-link-service: registers
// the feature on the hub and attaches the Link1.Notifications adaptor to
// `exported`. Ownership: parented to `exported`; the hub must outlive it.
class NotificationsModule final : public QObject {
    Q_OBJECT
public:
    NotificationsModule(Hub &hub, NotificationsConfig config, QObject *exported);
    [[nodiscard]] NotificationFeature &feature() { return m_feature; }

private:
    IconCache m_icons;
    AppSettings m_apps;
    NotificationFeature m_feature;
};

} // namespace QindaQt::Link
