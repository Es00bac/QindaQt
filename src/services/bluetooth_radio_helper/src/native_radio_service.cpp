// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_radio_service_p.h"
#include "native_radio_codec_p.h"
#include <QtCore/QScopedValueRollback>
namespace QindaQt::BluetoothRadio {
NativeRadioService::NativeRadioService(NativeRadioWire &wire, RadioOperation &operation)
    : m_wire(wire), m_operation(operation) {
    m_wire.setHandler([this](DBusMessage *message) { return receive(message); });
}
NativeRadioService::~NativeRadioService() { m_wire.setHandler({}); }
bool NativeRadioService::receive(DBusMessage *message) {
    if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL
        || !dbus_message_has_path(message, kPath)) return false;
    if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        auto reply = NativeMessage(dbus_message_new_method_return(message));
        DBusMessageIter args;
        if (reply) {
            dbus_message_iter_init_append(reply.get(), &args);
            appendNativeText(&args, QStringLiteral("<node><interface name=\"org.qindaqt.BluetoothRadio1\">"
                "<method name=\"ObserveAndUnblock\"><arg type=\"(ssssstss)\" direction=\"in\"/>"
                "<arg type=\"(sus)\" direction=\"out\"/></method></interface></node>"));
            m_wire.sendReply(message, std::move(reply));
        }
        return true;
    }
    if (!dbus_message_is_method_call(message, kInterface, "ObserveAndUnblock")) return false;
    Request request;
    const char *sender = dbus_message_get_sender(message);
    if (!sender || !readNativeRequest(message, &request)) {
        m_wire.sendReply(message, NativeMessage(dbus_message_new_error(message,
            DBUS_ERROR_INVALID_ARGS, "One bounded delegated radio request is required")));
        return true;
    }
    Result result{request.nonce, Disposition::Refused, QStringLiteral("radio-busy")};
    if (!m_busy) {
        QScopedValueRollback<bool> active(m_busy, true);
        result = m_operation.execute(QString::fromUtf8(sender), request);
    }
    NativeMessage reply(dbus_message_new_method_return(message));
    if (appendNativeResult(reply.get(), result)) m_wire.sendReply(message, std::move(reply));
    return true;
}
} // namespace QindaQt::BluetoothRadio
