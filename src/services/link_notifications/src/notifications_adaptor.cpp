// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/notifications_adaptor.h>

#include <QtCore/QJsonDocument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Link {
namespace {
QString compact(const QJsonArray &a) { return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)); }
}

NotificationsAdaptor::NotificationsAdaptor(NotificationFeature &feature, QObject *exported)
    : QDBusAbstractAdaptor(exported), m_feature(feature)
{
    connect(&feature, &NotificationFeature::currentChanged, this, [this] { Q_EMIT CurrentChanged(Current()); });
}

QString NotificationsAdaptor::ListApps() const { return compact(m_feature.listApps()); }
QString NotificationsAdaptor::Current() const { return compact(m_feature.current()); }
void NotificationsAdaptor::SetAppMuted(const QString &pkg, bool muted) { m_feature.setAppMuted(pkg, muted); }

QString NotificationsAdaptor::Reply(const QString &device, const QString &key, const QString &action,
                                    const QString &text)
{
    setDelayedReply(true);
    const QDBusMessage request = message();
    const QDBusConnection bus = connection();
    m_feature.reply(device, key, action, text, [request, bus](const QString &error) {
        bus.send(request.createReply(error));
    });
    return {};
}

} // namespace QindaQt::Link
