// SPDX-License-Identifier: GPL-3.0-or-later
#include "qt_radio_authority_p.h"
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusVariant>
#include <algorithm>
#include <unistd.h>
#include <utility>

namespace QindaQt::BluetoothRadio {
namespace {
int remaining(const Request &request) {
    const auto now = boottimeMilliseconds();
    if (!now || now >= request.deadlineBoottimeMs) return 0;
    return static_cast<int>(std::min<quint64>(250, request.deadlineBoottimeMs - now));
}
QDBusMessage busQuery(const QDBusConnection &bus, const QString &method,
    const QString &name, const Request &request) {
    const int timeout = remaining(request);
    if (!bus.isConnected() || !timeout) return {};
    auto query = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"), method);
    query << name;
    return bus.call(query, QDBus::Block, timeout);
}
bool ownerEquals(const QDBusConnection &bus, const QString &name,
    const QString &expected, const Request &request) {
    const auto reply = busQuery(bus, QStringLiteral("GetNameOwner"), name, request);
    return reply.type() == QDBusMessage::ReplyMessage && reply.signature() == QLatin1String("s")
        && reply.arguments().size() == 1 && reply.arguments().constFirst().toString() == expected;
}
bool currentUser(const QDBusConnection &bus, const QString &name, const Request &request) {
    const auto reply = busQuery(bus, QStringLiteral("GetConnectionUnixUser"), name, request);
    return reply.type() == QDBusMessage::ReplyMessage && reply.signature() == QLatin1String("u")
        && reply.arguments().size() == 1 && reply.arguments().constFirst().toUInt() == geteuid();
}
bool currentIntent(const QDBusConnection &bus, const QString &sender, const Request &request) {
    const int timeout = remaining(request);
    if (!timeout) return false;
    auto query = QDBusMessage::createMethodCall(sender, QString::fromLatin1(kIntentPath),
        QString::fromLatin1(kIntentInterface), QStringLiteral("Current"));
    query << QVariant::fromValue(request);
    const auto reply = bus.call(query, QDBus::Block, timeout);
    return reply.type() == QDBusMessage::ReplyMessage && reply.service() == sender
        && reply.signature() == QLatin1String("b") && reply.arguments().size() == 1
        && reply.arguments().constFirst().metaType().id() == QMetaType::Bool
        && reply.arguments().constFirst().toBool();
}
}
QtRadioAuthority::QtRadioAuthority(QDBusConnection session, QDBusConnection bluez)
    : m_session(std::move(session)), m_bluez(std::move(bluez)) {}
bool QtRadioAuthority::current(const QString &sender, const Request &request) {
    if (!validRequest(request) || !ownerEquals(m_session, QStringLiteral("org.qindaqt.Bluetooth1"),
            sender, request) || !currentUser(m_session, sender, request)
        || !currentUser(m_session, request.initiatingCaller, request)
        || !ownerEquals(m_bluez, QStringLiteral("org.bluez"), request.bluezOwner, request)
        || !currentIntent(m_session, sender, request))
        return false;
    // Do not enumerate names/devices or accept an IPC path alone. The selected
    // live exact-owner Adapter1 must still report its captured canonical address.
    auto query = QDBusMessage::createMethodCall(request.bluezOwner, request.adapterPath,
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    query << QStringLiteral("org.bluez.Adapter1") << QStringLiteral("Address");
    const int timeout = remaining(request);
    if (!timeout) return false;
    const auto reply = m_bluez.call(query, QDBus::Block, timeout);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.service() != request.bluezOwner
        || reply.signature() != QLatin1String("v") || reply.arguments().size() != 1)
        return false;
    const auto property = qvariant_cast<QDBusVariant>(reply.arguments().constFirst()).variant();
    if (property.metaType().id() != QMetaType::QString
        || property.toString() != request.adapterAddress) return false;
    // Re-resolve the admitting owner after crossing the upstream boundary.
    return ownerEquals(m_session, QStringLiteral("org.qindaqt.Bluetooth1"), sender, request)
        && currentUser(m_session, request.initiatingCaller, request)
        && ownerEquals(m_bluez, QStringLiteral("org.bluez"), request.bluezOwner, request)
        && currentIntent(m_session, sender, request) && remaining(request) > 0;
}
} // namespace QindaQt::BluetoothRadio
