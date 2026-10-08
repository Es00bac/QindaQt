// SPDX-License-Identifier: LGPL-3.0-or-later
#include "native_radio_codec_p.h"
namespace QindaQt::BluetoothRadio {
bool appendNativeText(DBusMessageIter *iter, const QString &text) {
    const auto bytes = text.toUtf8(); const char *value = bytes.constData();
    return !text.contains(QChar(0)) && bytes.size() <= 4096
        && dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &value);
}
QString readNativeText(DBusMessageIter *iter) {
    if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRING) return {};
    const char *value = nullptr; dbus_message_iter_get_basic(iter, &value);
    dbus_message_iter_next(iter);
    return value ? QString::fromUtf8(value) : QString{};
}
bool appendNativeRequest(DBusMessage *message, const Request &request) {
    if (!message || !validRequest(request)) return false;
    DBusMessageIter outer, inner; dbus_message_iter_init_append(message, &outer);
    if (!dbus_message_iter_open_container(&outer, DBUS_TYPE_STRUCT, nullptr, &inner)) return false;
    const dbus_uint64_t deadline = request.deadlineBoottimeMs;
    const bool added = appendNativeText(&inner, request.nonce)
        && appendNativeText(&inner, request.bluezOwner)
        && appendNativeText(&inner, request.adapterPath)
        && appendNativeText(&inner, request.adapterAddress)
        && appendNativeText(&inner, request.initiatingCaller)
        && dbus_message_iter_append_basic(&inner, DBUS_TYPE_UINT64, &deadline)
        && appendNativeText(&inner, request.authorityOwner)
        && appendNativeText(&inner, request.transportCaller);
    if (!added) { dbus_message_iter_abandon_container(&outer, &inner); return false; }
    return dbus_message_iter_close_container(&outer, &inner);
}
bool readNativeRequest(DBusMessage *message, Request *request) {
    if (!message || !dbus_message_has_signature(message, kRequestSignature)) return false;
    DBusMessageIter outer, inner; dbus_message_iter_init(message, &outer);
    dbus_message_iter_recurse(&outer, &inner);
    Request value;
    value.nonce = readNativeText(&inner); value.bluezOwner = readNativeText(&inner);
    value.adapterPath = readNativeText(&inner); value.adapterAddress = readNativeText(&inner);
    value.initiatingCaller = readNativeText(&inner);
    dbus_uint64_t deadline = 0; dbus_message_iter_get_basic(&inner, &deadline);
    value.deadlineBoottimeMs = deadline; dbus_message_iter_next(&inner);
    value.authorityOwner = readNativeText(&inner); value.transportCaller = readNativeText(&inner);
    if (!validRequest(value)) return false;
    *request = std::move(value); return true;
}
bool appendNativeResult(DBusMessage *message, const Result &result) {
    if (!message || !validResult(result)) return false;
    DBusMessageIter outer, inner; dbus_message_iter_init_append(message, &outer);
    if (!dbus_message_iter_open_container(&outer, DBUS_TYPE_STRUCT, nullptr, &inner)) return false;
    const dbus_uint32_t state = static_cast<dbus_uint32_t>(result.disposition);
    const bool added = appendNativeText(&inner, result.nonce)
        && dbus_message_iter_append_basic(&inner, DBUS_TYPE_UINT32, &state)
        && appendNativeText(&inner, result.reasonCode);
    if (!added) { dbus_message_iter_abandon_container(&outer, &inner); return false; }
    return dbus_message_iter_close_container(&outer, &inner);
}
bool readNativeResult(DBusMessage *message, Result *result) {
    if (!message || dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_RETURN
        || !dbus_message_has_signature(message, "(sus)")) return false;
    DBusMessageIter outer, inner; dbus_message_iter_init(message, &outer);
    dbus_message_iter_recurse(&outer, &inner);
    Result value; value.nonce = readNativeText(&inner);
    dbus_uint32_t state = 0; dbus_message_iter_get_basic(&inner, &state);
    dbus_message_iter_next(&inner); value.disposition = static_cast<Disposition>(state);
    value.reasonCode = readNativeText(&inner);
    value.wireValid = state <= static_cast<dbus_uint32_t>(Disposition::Uncertain);
    if (!validResult(value)) return false;
    *result = std::move(value); return true;
}
} // namespace QindaQt::BluetoothRadio
