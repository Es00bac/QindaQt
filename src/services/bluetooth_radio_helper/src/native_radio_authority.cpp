// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_radio_authority_p.h"
#include "native_radio_codec_p.h"
#include <algorithm>
namespace QindaQt::BluetoothRadio {
namespace {
int remaining(const Request &request) {
    const auto now = boottimeMilliseconds();
    if (!now || now >= request.deadlineBoottimeMs) return 0;
    return static_cast<int>(std::min<quint64>(250, request.deadlineBoottimeMs - now));
}
}
NativeRadioAuthority::NativeRadioAuthority(NativeRadioWire &session, NativeRadioWire &bluez)
    : m_session(session), m_bluez(bluez) {}
bool NativeRadioAuthority::intent(const Request &request) {
    auto query = nativeMethod(request.authorityOwner, kIntentPath, kIntentInterface, "Current");
    if (!appendNativeRequest(query.get(), request)) return false;
    const auto reply = m_session.call(std::move(query), remaining(request));
    bool allowed = false;
    return nativeBool(reply.get(), &allowed) && allowed;
}
bool NativeRadioAuthority::lineage(const QString &sender, const Request &request) {
    if (!validRequest(request) || sender != request.transportCaller || !remaining(request)
        || m_session.owner(QString::fromLatin1(kService), remaining(request)) != m_session.uniqueOwner()
        || m_session.owner(QStringLiteral("org.qindaqt.Bluetooth1"), remaining(request)) != request.authorityOwner
        || !m_session.sameUser(request.authorityOwner, remaining(request))
        || !m_session.sameUser(request.transportCaller, remaining(request))
        || !m_session.sameUser(request.initiatingCaller, remaining(request))
        || m_bluez.owner(QStringLiteral("org.bluez"), remaining(request)) != request.bluezOwner)
        return false;
    return intent(request) && remaining(request);
}
bool NativeRadioAuthority::current(const QString &sender, const Request &request) {
    if (!lineage(sender, request)) return false;
    auto query = nativeMethod(request.bluezOwner, request.adapterPath.toUtf8().constData(),
        "org.freedesktop.DBus.Properties", "Get");
    DBusMessageIter args;
    if (!query) return false;
    dbus_message_iter_init_append(query.get(), &args);
    if (!appendNativeText(&args, QStringLiteral("org.bluez.Adapter1"))
        || !appendNativeText(&args, QStringLiteral("Address"))) return false;
    const auto reply = m_bluez.call(std::move(query), remaining(request));
    if (!reply || dbus_message_get_type(reply.get()) != DBUS_MESSAGE_TYPE_METHOD_RETURN
        || !dbus_message_has_signature(reply.get(), "v")) return false;
    DBusMessageIter outer, value; dbus_message_iter_init(reply.get(), &outer);
    dbus_message_iter_recurse(&outer, &value);
    if (dbus_message_iter_get_arg_type(&value) != DBUS_TYPE_STRING
        || readNativeText(&value) != request.adapterAddress) return false;
    return lineage(sender, request);
}
} // namespace QindaQt::BluetoothRadio
