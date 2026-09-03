// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_model/adapter_backend.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>

#include <functional>

namespace QindaQt::Bluetooth::Bluez
{

// One org.bluez.Agent1 registered on the injected connection. BlueZ method
// calls are retained only while a bounded user reply is required; display
// facts return immediately. The resolver converts a BlueZ object path into a
// canonical device address without exposing platform handles across the port.
class BluezPairingAgent final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")
    Q_CLASSINFO(
        "D-Bus Introspection",
        "<interface name=\"org.bluez.Agent1\">"
        "<method name=\"Release\"/>"
        "<method name=\"RequestPinCode\"><arg name=\"device\" type=\"o\" direction=\"in\"/>"
        "<arg name=\"pincode\" type=\"s\" direction=\"out\"/></method>"
        "<method name=\"DisplayPinCode\"><arg name=\"device\" type=\"o\" direction=\"in\"/>"
        "<arg name=\"pincode\" type=\"s\" direction=\"in\"/></method>"
        "<method name=\"RequestPasskey\"><arg name=\"device\" type=\"o\" direction=\"in\"/>"
        "<arg name=\"passkey\" type=\"u\" direction=\"out\"/></method>"
        "<method name=\"DisplayPasskey\"><arg name=\"device\" type=\"o\" direction=\"in\"/>"
        "<arg name=\"passkey\" type=\"u\" direction=\"in\"/><arg name=\"entered\" type=\"q\" direction=\"in\"/></method>"
        "<method name=\"RequestConfirmation\"><arg name=\"device\" type=\"o\" direction=\"in\"/>"
        "<arg name=\"passkey\" type=\"u\" direction=\"in\"/></method>"
        "<method name=\"RequestAuthorization\"><arg name=\"device\" type=\"o\" direction=\"in\"/></method>"
        "<method name=\"AuthorizeService\"><arg name=\"device\" type=\"o\" direction=\"in\"/>"
        "<arg name=\"uuid\" type=\"s\" direction=\"in\"/></method>"
        "<method name=\"Cancel\"/></interface>")

public:
    using DeviceResolver = std::function<QString(const QString &)>;

    BluezPairingAgent(const QDBusConnection &connection, DeviceResolver resolver,
                      int timeoutMs, QObject *parent = nullptr);
    ~BluezPairingAgent() override;

    void adoptOwner(const QString &owner);
    void setAdapterAvailable(bool available);
    void stop();
    [[nodiscard]] BackendPairingPrompt prompt() const { return m_prompt; }
    [[nodiscard]] bool replyConfirmation(quint64 promptId, bool accepted);
    [[nodiscard]] bool replyPasskey(quint64 promptId, const QString &passkey);
    [[nodiscard]] bool replyPin(quint64 promptId, const QString &pin);
    [[nodiscard]] bool cancelPrompt(quint64 promptId);

public Q_SLOTS:
    Q_SCRIPTABLE void Release();
    Q_SCRIPTABLE void RequestPinCode(const QDBusObjectPath &device);
    Q_SCRIPTABLE void DisplayPinCode(const QDBusObjectPath &device,
                                     const QString &pinCode);
    Q_SCRIPTABLE void RequestPasskey(const QDBusObjectPath &device);
    Q_SCRIPTABLE void DisplayPasskey(const QDBusObjectPath &device,
                                     quint32 passkey, ushort entered);
    Q_SCRIPTABLE void RequestConfirmation(const QDBusObjectPath &device,
                                          quint32 passkey);
    Q_SCRIPTABLE void RequestAuthorization(const QDBusObjectPath &device);
    Q_SCRIPTABLE void AuthorizeService(const QDBusObjectPath &device,
                                       const QString &uuid);
    Q_SCRIPTABLE void Cancel();

Q_SIGNALS:
    void promptChanged(const QindaQt::Bluetooth::BackendPairingPrompt &prompt);

private:
    void registerWithOwner();
    void unregisterFromOwner();
    void syncRegistration();
    void beginRequest(PairingPromptKind kind, const QString &devicePath,
                      QString detail = {}, QString serviceUuid = {});
    void publishDisplay(PairingPromptKind kind, const QString &devicePath,
                        QString detail, quint16 entered = 0);
    void clearPrompt();
    void rejectCurrent(const QString &errorName);
    [[nodiscard]] bool authenticCall() const;
    [[nodiscard]] bool requireAuthenticCall();
    [[nodiscard]] QString resolve(const QString &devicePath) const;
    [[nodiscard]] quint64 issuePromptId();

    QDBusConnection m_connection;
    DeviceResolver m_resolver;
    QTimer m_timer;
    QString m_owner;
    QDBusMessage m_pendingCall;
    BackendPairingPrompt m_prompt;
    quint64 m_ownerToken = 0;
    quint64 m_nextPromptId = 1;
    bool m_adapterAvailable = false;
    bool m_objectRegistered = false;
    bool m_agentRegistered = false;
    bool m_registrationPending = false;
};

} // namespace QindaQt::Bluetooth::Bluez
