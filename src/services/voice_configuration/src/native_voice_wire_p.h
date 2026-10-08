// SPDX-License-Identifier: LGPL-3.0-or-later
// AGENT-NOTE: Voice-owned adaptation of the reviewed Bluetooth native reply
// authority mechanism. No Bluetooth private headers cross this boundary.
#pragma once
#include <QtCore/QObject>
#include <QtCore/QString>
#include <dbus/dbus.h>
#include <functional>
#include <memory>

class QSocketNotifier;
namespace QindaQt::Services::VoiceConfiguration {
struct NativeMessageDeleter {
    void operator()(DBusMessage *value) const { if (value) dbus_message_unref(value); }
};
using NativeMessage = std::unique_ptr<DBusMessage, NativeMessageDeleter>;

// One pending call owns its native reference and expected peer/serial. A foreign
// reply may consume libdbus's serial-only pending slot: reject, never retry.
class NativeVoiceCall final {
public:
    NativeVoiceCall(DBusPendingCall *pending, QString peer, dbus_uint32_t serial);
    ~NativeVoiceCall();
    NativeVoiceCall(const NativeVoiceCall &) = delete;
    NativeVoiceCall &operator=(const NativeVoiceCall &) = delete;
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
class NativeVoiceWire final : public QObject {
public:
    explicit NativeVoiceWire(QObject *parent = nullptr);
    ~NativeVoiceWire() override;
    bool open(const QString &singleUnixAddress, bool eventPump = true);
    bool connected() const;
    QString uniqueOwner() const;
    QString serverGuid() const { return m_guid; }
    bool selectedPeer() const { return m_selectedPeer; }
    QString pinnedAddress() const { return m_pinnedAddress; }
    static bool boundedAddress(const QString &address, bool *hasGuid = nullptr);
    std::unique_ptr<NativeVoiceCall> send(NativeMessage request, int timeoutMs);
    NativeMessage call(NativeMessage request, int timeoutMs);
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
    bool m_filterInstalled = false;
};
NativeMessage nativeMethod(const QString &peer, const char *path,
    const char *interface, const char *method);
bool nativeText(DBusMessage *message, const char *signature, QString *value);
} // namespace QindaQt::Services::VoiceConfiguration
