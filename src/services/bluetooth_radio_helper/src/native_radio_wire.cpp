// SPDX-License-Identifier: LGPL-3.0-or-later
#include "native_radio_wire_p.h"
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QSocketNotifier>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <unistd.h>
#include <utility>

namespace QindaQt::BluetoothRadio {
namespace {
constexpr auto Driver = "org.freedesktop.DBus";
constexpr auto DriverPath = "/org/freedesktop/DBus";
bool unique(const QString &value) {
    static const QRegularExpression pattern(QStringLiteral("^:[0-9]+\\.[0-9]+\\z"));
    return value.size() <= 255 && pattern.match(value).hasMatch();
}
bool sender(DBusMessage *message, const QString &expected) {
    const auto *actual = message ? dbus_message_get_sender(message) : nullptr;
    return actual && expected == QString::fromUtf8(actual);
}
}
NativeRadioCall::NativeRadioCall(DBusPendingCall *pending, QString peer, dbus_uint32_t serial)
    : m_pending(pending), m_peer(std::move(peer)), m_serial(serial) {}
NativeRadioCall::~NativeRadioCall() {
    if (m_pending) { dbus_pending_call_cancel(m_pending); dbus_pending_call_unref(m_pending); }
}
bool NativeRadioCall::completed() const {
    return m_pending && dbus_pending_call_get_completed(m_pending);
}
void NativeRadioCall::wait() { if (m_pending) dbus_pending_call_block(m_pending); }
NativeMessage NativeRadioCall::take() {
    return NativeMessage(completed() ? dbus_pending_call_steal_reply(m_pending) : nullptr);
}
bool NativeRadioCall::fromExpectedPeer(DBusMessage *reply) const {
    return reply && sender(reply, m_peer) && m_serial
        && dbus_message_get_reply_serial(reply) == m_serial;
}
NativeMessage nativeMethod(const QString &peer, const char *path,
    const char *interface, const char *method) {
    NativeMessage value(dbus_message_new_method_call(peer.toUtf8().constData(), path, interface, method));
    if (value) dbus_message_set_auto_start(value.get(), false);
    return value;
}
bool nativeText(DBusMessage *message, const char *signature, QString *value) {
    if (!message || dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_RETURN
        || !dbus_message_has_signature(message, signature)) return false;
    const char *text = nullptr;
    if (!dbus_message_get_args(message, nullptr, DBUS_TYPE_STRING, &text, DBUS_TYPE_INVALID)
        || !text) return false;
    *value = QString::fromUtf8(text);
    return value->size() <= 4096;
}
bool nativeBool(DBusMessage *message, bool *value) {
    if (!message || dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_RETURN
        || !dbus_message_has_signature(message, "b")) return false;
    dbus_bool_t result = false;
    if (!dbus_message_get_args(message, nullptr, DBUS_TYPE_BOOLEAN, &result, DBUS_TYPE_INVALID)) return false;
    *value = result;
    return true;
}
NativeRadioWire::NativeRadioWire(QObject *parent) : QObject(parent) {}
NativeRadioWire::~NativeRadioWire() {
    m_closing = true;
    if (m_notifier) { m_notifier->setEnabled(false); delete m_notifier; }
    m_handler = {}; m_progress = {};
    if (m_connection) {
        dbus_connection_remove_filter(m_connection, filter, this);
        dbus_connection_close(m_connection); dbus_connection_unref(m_connection);
    }
}
bool NativeRadioWire::boundedAddress(const QString &address, bool *hasGuid) {
    if (hasGuid) *hasGuid = false;
    if (address.isEmpty() || address.size() > 4096 || address.contains(QChar(0))
        || !address.startsWith(QLatin1String("unix:")) || address.contains(QLatin1Char(';'))) return false;
    const auto fields = address.mid(5).split(QLatin1Char(','));
    bool endpoint = false; QStringList keys;
    static const QRegularExpression guid(QStringLiteral("^[0-9a-f]{32}\\z"));
    for (const auto &field : fields) {
        const auto equal = field.indexOf(QLatin1Char('='));
        if (equal <= 0 || equal == field.size() - 1) return false;
        const auto key = field.left(equal);
        if (keys.contains(key)) return false;
        keys.append(key);
        if (key == QLatin1String("guid")) {
            if (!guid.match(field.mid(equal + 1)).hasMatch()) return false;
            if (hasGuid) *hasGuid = true;
        } else if (key == QLatin1String("path") || key == QLatin1String("abstract")) {
            if (endpoint) return false;
            endpoint = true;
        } else if (key == QLatin1String("runtime") && field.mid(equal + 1) == QLatin1String("yes")) {
            if (endpoint) return false;
            endpoint = true;
        } else return false;
    }
    DBusAddressEntry **entries = nullptr; int count = 0; DBusError error;
    dbus_error_init(&error);
    const bool parsed = dbus_parse_address(address.toUtf8().constData(), &entries, &count, &error);
    dbus_error_free(&error);
    if (entries) dbus_address_entries_free(entries);
    return endpoint && parsed && count == 1;
}
bool NativeRadioWire::open(const QString &address, bool eventPump) {
    static const bool threadsReady = dbus_threads_init_default();
    if (!threadsReady) return false;
    bool hasGuid = false;
    if (m_connection || QThread::currentThread() != thread() || !boundedAddress(address, &hasGuid)) return false;
    // An explicit GUID selects an incarnation even if authentication fails.
    m_selectedPeer = hasGuid;
    DBusError error; dbus_error_init(&error);
    m_connection = dbus_connection_open_private(address.toUtf8().constData(), &error);
    dbus_error_free(&error);
    if (!m_connection) return false;
    dbus_connection_set_exit_on_disconnect(m_connection, false);
    dbus_connection_set_max_message_size(m_connection, 8192);
    dbus_connection_set_max_received_size(m_connection, 131072);
    auto hello = call(nativeMethod(QString::fromLatin1(Driver), DriverPath, Driver, "Hello"), 500);
    QString name;
    char *guid = dbus_connection_get_server_id(m_connection);
    if (guid) { m_guid = QString::fromLatin1(guid); dbus_free(guid); m_selectedPeer = true; }
    if (!nativeText(hello.get(), "s", &name) || !unique(name)
        || !dbus_bus_set_unique_name(m_connection, name.toUtf8().constData()) || m_guid.isEmpty()) return false;
    m_pinnedAddress = address;
    if (!hasGuid) m_pinnedAddress += QStringLiteral(",guid=") + m_guid;
    int descriptor = -1;
    if (!dbus_connection_get_unix_fd(m_connection, &descriptor) || descriptor < 0
        || !dbus_connection_add_filter(m_connection, filter, this, nullptr)) return false;
    if (eventPump) {
        m_notifier = new QSocketNotifier(descriptor, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, [this] { pump(); });
        // AGENT-NOTE: read readiness alone cannot drain a partially queued
        // outgoing message or already-buffered dispatch after a synchronous
        // broker query. This bounded same-thread pump grants no reply authority.
        auto *progress = new QTimer(this);
        progress->setInterval(25);
        connect(progress, &QTimer::timeout, this, [this] { pump(); });
        progress->start();
    }
    return true;
}
bool NativeRadioWire::connected() const {
    return !m_closing && m_connection && dbus_connection_get_is_connected(m_connection);
}
QString NativeRadioWire::uniqueOwner() const {
    const char *value = connected() ? dbus_bus_get_unique_name(m_connection) : nullptr;
    return value ? QString::fromUtf8(value) : QString{};
}
std::unique_ptr<NativeRadioCall> NativeRadioWire::send(NativeMessage request, int timeoutMs) {
    if (!connected() || !request || timeoutMs <= 0 || timeoutMs > 2000
        || QThread::currentThread() != thread()) return {};
    const char *destination = dbus_message_get_destination(request.get());
    if (!destination) return {};
    const auto peer = QString::fromUtf8(destination);
    if (peer != QLatin1String(Driver) && !unique(peer)) return {};
    DBusPendingCall *pending = nullptr;
    if (!dbus_connection_send_with_reply(m_connection, request.get(), &pending, timeoutMs) || !pending) return {};
    return std::make_unique<NativeRadioCall>(pending, peer, dbus_message_get_serial(request.get()));
}
NativeMessage NativeRadioWire::call(NativeMessage request, int timeoutMs) {
    auto pending = send(std::move(request), timeoutMs);
    if (!pending) return {};
    pending->wait(); auto reply = pending->take();
    if (!pending->fromExpectedPeer(reply.get())) return {};
    return reply;
}
QString NativeRadioWire::owner(const QString &name, int timeoutMs) {
    auto request = nativeMethod(QString::fromLatin1(Driver), DriverPath, Driver, "GetNameOwner");
    const auto bytes = name.toUtf8(); const char *value = bytes.constData();
    if (!request || !dbus_message_append_args(request.get(), DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID)) return {};
    const auto reply = call(std::move(request), timeoutMs); QString result;
    return nativeText(reply.get(), "s", &result) && unique(result) ? result : QString{};
}
bool NativeRadioWire::sameUser(const QString &name, int timeoutMs) {
    auto request = nativeMethod(QString::fromLatin1(Driver), DriverPath, Driver, "GetConnectionUnixUser");
    const auto bytes = name.toUtf8(); const char *value = bytes.constData();
    if (!request || !dbus_message_append_args(request.get(), DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID)) return false;
    const auto reply = call(std::move(request), timeoutMs); dbus_uint32_t uid = 0;
    return reply && dbus_message_get_type(reply.get()) == DBUS_MESSAGE_TYPE_METHOD_RETURN
        && dbus_message_has_signature(reply.get(), "u")
        && dbus_message_get_args(reply.get(), nullptr, DBUS_TYPE_UINT32, &uid, DBUS_TYPE_INVALID)
        && uid == geteuid();
}
bool NativeRadioWire::own(const QString &name) {
    auto request = nativeMethod(QString::fromLatin1(Driver), DriverPath, Driver, "RequestName");
    const auto bytes = name.toUtf8(); const char *value = bytes.constData();
    dbus_uint32_t flags = DBUS_NAME_FLAG_DO_NOT_QUEUE;
    if (!request || !dbus_message_append_args(request.get(), DBUS_TYPE_STRING, &value,
        DBUS_TYPE_UINT32, &flags, DBUS_TYPE_INVALID)) return false;
    const auto reply = call(std::move(request), 250); dbus_uint32_t result = 0;
    return reply && dbus_message_get_type(reply.get()) == DBUS_MESSAGE_TYPE_METHOD_RETURN
        && dbus_message_has_signature(reply.get(), "u")
        && dbus_message_get_args(reply.get(), nullptr, DBUS_TYPE_UINT32, &result, DBUS_TYPE_INVALID)
        && result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
}
bool NativeRadioWire::sendReply(DBusMessage *request, NativeMessage reply) {
    if (!connected() || !request || !reply) return false;
    return dbus_connection_send(m_connection, reply.get(), nullptr);
}
void NativeRadioWire::setHandler(std::function<bool(DBusMessage *)> handler) { m_handler = std::move(handler); }
void NativeRadioWire::setProgress(std::function<void()> progress) { m_progress = std::move(progress); }
DBusHandlerResult NativeRadioWire::filter(DBusConnection *, DBusMessage *message, void *data) {
    auto *wire = static_cast<NativeRadioWire *>(data);
    if (wire->m_closing || !wire->m_handler) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    return wire->m_handler(message) ? DBUS_HANDLER_RESULT_HANDLED : DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
void NativeRadioWire::dispatchPending() { pump(); }
void NativeRadioWire::pump() {
    if (m_closing || m_pumping || !m_connection) return;
    QScopedValueRollback<bool> active(m_pumping, true);
    dbus_connection_read_write(m_connection, 0);
    DBusDispatchStatus status = DBUS_DISPATCH_COMPLETE;
    for (int count = 0; count < 32; ++count) {
        status = dbus_connection_dispatch(m_connection);
        if (status != DBUS_DISPATCH_DATA_REMAINS) break;
    }
    if (m_progress) m_progress();
    if (m_notifier && status == DBUS_DISPATCH_DATA_REMAINS)
        QTimer::singleShot(0, this, [this] { pump(); });
}
} // namespace QindaQt::BluetoothRadio
