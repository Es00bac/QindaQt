// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QtCore/QObject>
#include <QtCore/QString>
#include <dbus/dbus.h>
#include <functional>
#include <memory>

class QSocketNotifier;
namespace QindaQt::BluetoothRadio {
struct NativeMessageDeleter {
    void operator()(DBusMessage *value) const { if (value) dbus_message_unref(value); }
};
using NativeMessage = std::unique_ptr<DBusMessage, NativeMessageDeleter>;

// One pending call owns its native reference and expected peer/serial. A foreign
// reply may consume libdbus's serial-only pending slot: reject, never retry.
class NativeRadioCall final {
public:
    NativeRadioCall(DBusPendingCall *pending, QString peer, dbus_uint32_t serial);
    ~NativeRadioCall();
    NativeRadioCall(const NativeRadioCall &) = delete;
    NativeRadioCall &operator=(const NativeRadioCall &) = delete;
    bool completed() const;
    NativeMessage take();
    void wait();
    bool fromExpectedPeer(DBusMessage *reply) const;
private:
    DBusPendingCall *m_pending;
    QString m_peer;
    dbus_uint32_t m_serial;
};

// Owning same-thread native bus transport. Never borrow Qt internalPointer.
// Public libdbus preserves real sender and serial, including error messages.
// Message pump is bounded; callbacks must not destroy this wire during dispatch.
class NativeRadioWire final : public QObject {
public:
    explicit NativeRadioWire(QObject *parent = nullptr);
    ~NativeRadioWire() override;
    bool open(const QString &singleUnixAddress, bool eventPump = true);
    bool connected() const;
    QString uniqueOwner() const;
    QString serverGuid() const { return m_guid; }
    bool selectedPeer() const { return m_selectedPeer; }
    QString pinnedAddress() const { return m_pinnedAddress; }
    static bool boundedAddress(const QString &address, bool *hasGuid = nullptr);
    std::unique_ptr<NativeRadioCall> send(NativeMessage request, int timeoutMs);
    NativeMessage call(NativeMessage request, int timeoutMs);
    QString owner(const QString &name, int timeoutMs = 250);
    bool sameUser(const QString &name, int timeoutMs = 250);
    bool own(const QString &name);
    bool sendReply(DBusMessage *request, NativeMessage reply);
    void dispatchPending(); // Same-thread bounded pump; also usable without a Qt event loop.
    void setHandler(std::function<bool(DBusMessage *)> handler);
    void setProgress(std::function<void()> progress);
private:
    static DBusHandlerResult filter(DBusConnection *, DBusMessage *, void *);
    void pump();
    DBusConnection *m_connection = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    std::function<bool(DBusMessage *)> m_handler;
    std::function<void()> m_progress;
    QString m_guid, m_pinnedAddress;
    bool m_selectedPeer = false, m_pumping = false, m_closing = false;
};
NativeMessage nativeMethod(const QString &peer, const char *path,
    const char *interface, const char *method);
bool nativeText(DBusMessage *message, const char *signature, QString *value);
bool nativeBool(DBusMessage *message, bool *value);
} // namespace QindaQt::BluetoothRadio
