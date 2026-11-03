// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/notifications_adaptor.h>

#include <QtCore/QJsonDocument>

#include <memory>
#include <optional>

namespace QindaQt::Link {
namespace {
QString compact(const QJsonArray &a) { return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)); }
}

NotificationsAdaptor::NotificationsAdaptor(NotificationFeature &feature, QObject *exported)
    : QDBusAbstractAdaptor(exported), m_feature(feature)
{
    connect(&feature, &NotificationFeature::entryChanged, this, [this](const QJsonObject &item) {
        Q_EMIT CurrentChanged(++m_revision, QString::fromUtf8(QJsonDocument(item).toJson(QJsonDocument::Compact)));
    });
}

QString NotificationsAdaptor::ListApps() const { return compact(m_feature.listApps()); }
QString NotificationsAdaptor::Current() const { return compact(m_feature.current()); }
void NotificationsAdaptor::SetAppMuted(const QString &pkg, bool muted) { m_feature.setAppMuted(pkg, muted); }

QString NotificationsAdaptor::Reply(const QString &device, const QString &key, const QString &action,
                                    const QString &text)
{
    // A failure before anything was sent arrives inside the call; the phone's answer arrives later.
    auto syncError = std::make_shared<std::optional<QString>>();
    auto sending = std::make_shared<bool>(true);
    m_feature.reply(device, key, action, text,
                    [this, device, key, action, syncError, sending](const QString &error) {
                        if (*sending) {
                            *syncError = error;
                        } else {
                            Q_EMIT ReplyFinished(device, key, action, error);
                        }
                    });
    *sending = false;
    return syncError->value_or(QString());
}

} // namespace QindaQt::Link
