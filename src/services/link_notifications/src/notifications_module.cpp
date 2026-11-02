// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/notifications_module.h>

namespace QindaQt::Link {

NotificationsModule::NotificationsModule(Hub &hub, NotificationsConfig config, QObject *exported)
    : QObject(exported),
      m_icons(config.cacheDir + QStringLiteral("/icons")),
      m_apps(config.configDir + QStringLiteral("/notifications.json")),
      m_feature(NotificationFeatureDeps{config.presenter, &m_icons, &m_apps, config.reply,
                                        [&hub](const QString &id) { return hub.session(id); }, 10'000},
                this)
{
    hub.addFeature(&m_feature);
    new NotificationsAdaptor(m_feature, exported);
}

} // namespace QindaQt::Link
