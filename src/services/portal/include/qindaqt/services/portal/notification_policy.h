// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusUnixFileDescriptor>
#include <QMap>
#include <QVariantMap>
#include <optional>
namespace QindaQt::Services::Portal {
struct SerializedIcon { QString kind; QDBusVariant value; };
QDBusArgument &operator<<(QDBusArgument &, const SerializedIcon &);
const QDBusArgument &operator>>(const QDBusArgument &, SerializedIcon &);
struct PortalNotificationAction { QString name; QVariant target; };
struct PortalNotification {
    QString appId, id, title, body, icon;
    QStringList actions;
    QMap<QString, PortalNotificationAction> actionValues;
    QVariantMap hints;
    QDBusUnixFileDescriptor image;
};
// Pure bounded conversion of frontend-authenticated app identity and standard
// serialized notification values. No icon FD reads or bus/process side effects.
// IDs remain case sensitive and isolated by (appId,id), never display text.
std::optional<PortalNotification> portalNotification(const QString &appId,
    const QString &id, const QVariantMap &notification);
// File-descriptor icons are bounded, sealed image snapshots. The adapter owns
// no borrowed descriptor after this call. Decoding failure is explicit; no
// pathname is constructed for another process to borrow. GUI drawing is absent.
bool decodeNotificationIcon(const QDBusUnixFileDescriptor &, QVariantMap *hints);
void registerNotificationTypes();
} // namespace QindaQt::Services::Portal
Q_DECLARE_METATYPE(QindaQt::Services::Portal::SerializedIcon)
