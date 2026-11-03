// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/notifications_adaptor.h>

#include <QtCore/QJsonDocument>

#include <memory>
#include <optional>

namespace QindaQt::Link {
namespace {
QString compact(const QJsonArray &a) { return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)); }
}

NotificationsAdaptor::NotificationsAdaptor(NotificationFeature &feature, QObject *exported, CallerAuthority *authority,
                                           QDBusConnection bus)
    : QDBusAbstractAdaptor(exported), m_feature(feature), m_authority(authority), m_bus(std::move(bus))
{
    connect(&feature, &NotificationFeature::entryChanged, this, [this](const QJsonObject &item) {
        // AGENT-GUARD: this signal reaches every client on the bus. Reduce the item to its key; the text stays
        // behind the authorized Current() call.
        QJsonObject idOnly{{QStringLiteral("device"), item.value(QStringLiteral("device"))},
                           {QStringLiteral("key"), item.value(QStringLiteral("key"))}};
        if (item.value(QStringLiteral("removed")).toBool()) idOnly.insert(QStringLiteral("removed"), true);
        Q_EMIT CurrentChanged(++m_revision, QString::fromUtf8(QJsonDocument(idOnly).toJson(QJsonDocument::Compact)));
    });
}

bool NotificationsAdaptor::allowed(const QDBusMessage &msg)
{
    if (m_authority != nullptr && m_authority->authorize(msg)) return true;
    if (m_authority == nullptr) {
        msg.setDelayedReply(true);
        m_bus.send(msg.createErrorReply(QStringLiteral("org.qindaqt.Link1.Error.Denied"), QStringLiteral("no caller authority")));
    }
    return false;
}

QString NotificationsAdaptor::ListApps(const QDBusMessage &msg) { return allowed(msg) ? compact(m_feature.listApps()) : QString(); }
QString NotificationsAdaptor::Current(const QDBusMessage &msg) { return allowed(msg) ? compact(m_feature.current()) : QString(); }
void NotificationsAdaptor::SetAppMuted(const QString &pkg, bool muted, const QDBusMessage &msg)
{
    if (allowed(msg)) m_feature.setAppMuted(pkg, muted);
}

QString NotificationsAdaptor::Reply(const QString &device, const QString &key, const QString &action,
                                    const QString &text, const QDBusMessage &msg)
{
    if (!allowed(msg)) return {};
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
