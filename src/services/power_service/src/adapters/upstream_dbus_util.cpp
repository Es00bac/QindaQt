// SPDX-License-Identifier: GPL-3.0-or-later

#include "upstream_dbus_util.h"

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Power::Upstream {

namespace {

// 32 hex characters = 32 UTF-8 bytes, comfortably inside the 128-byte
// Power1 opaque-ID bound while keeping collision odds negligible.
constexpr qsizetype kOpaqueIdHexLength = 32;

} // namespace

bool readObjectPathArray(const QVariant &value, QStringList &paths)
{
    if (!value.canConvert<QDBusArgument>()) {
        return false;
    }
    const QDBusArgument array = value.value<QDBusArgument>();
    if (array.currentType() != QDBusArgument::ArrayType) {
        return false;
    }
    QStringList parsed;
    array.beginArray();
    while (!array.atEnd()) {
        QDBusObjectPath path;
        array >> path;
        const QString pathText = path.path();
        if (pathText.isEmpty() || !pathText.startsWith(QLatin1Char('/'))) {
            return false;
        }
        parsed.push_back(pathText);
    }
    array.endArray();
    paths = std::move(parsed);
    return true;
}

bool readStringVariantMapArray(const QVariant &value, QList<QVariantMap> &entries)
{
    if (!value.canConvert<QDBusArgument>()) {
        return false;
    }
    const QDBusArgument array = value.value<QDBusArgument>();
    if (array.currentType() != QDBusArgument::ArrayType) {
        return false;
    }
    QList<QVariantMap> parsed;
    array.beginArray();
    while (!array.atEnd()) {
        QVariantMap entry;
        array >> entry;
        parsed.push_back(std::move(entry));
    }
    array.endArray();
    entries = std::move(parsed);
    return true;
}

bool optionalBool(const QVariantMap &properties, const QString &name, bool &value)
{
    const auto it = properties.constFind(name);
    if (it == properties.constEnd() || it.value().userType() != QMetaType::Bool) {
        return false;
    }
    value = it.value().toBool();
    return true;
}

void getAllProperties(const QDBusConnection &connection, const QString &service,
                      const QString &path, const QString &interface, QObject *context,
                      const std::function<void(const QVariantMap &,
                                               const QString &sender)> &accept,
                      const std::function<void(const QString &)> &reject)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        service, path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("GetAll"));
    call.setArguments({interface});
    auto *watcher =
        new QDBusPendingCallWatcher(connection.asyncCall(call), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
                     [watcher, accept, reject]() {
                         const QDBusPendingReply<QVariantMap> reply = *watcher;
                         const QString replyingService =
                             watcher->reply().service();
                         watcher->deleteLater();
                         if (reply.isError()) {
                             reject(reply.error().name());
                             return;
                         }
                         accept(reply.value(), replyingService);
                     });
}

} // namespace QindaQt::Power::Upstream
