// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "bluez_compose.h"

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusVirtualObject>

namespace QindaQt::Tests
{

class FakeBluez;

// One virtual object serves the whole fake org.bluez tree. Manual dispatch by
// path/interface/member gives the fixture exact BlueZ reply control: success,
// exact org.bluez error names, and deliberately deferred replies, with no
// QtDBus adaptor machinery in between.
class FakeBluezServiceObject final : public QDBusVirtualObject
{
    Q_OBJECT

public:
    explicit FakeBluezServiceObject(FakeBluez *bluez, QObject *parent);

    [[nodiscard]] QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override;

private:
    FakeBluez *m_bluez;
};

// In-test fake org.bluez service on a private bus. It models the BlueZ
// behavior the production adapter depends on: per-caller reference-counted
// discovery sessions, Powered-off consequences (discovery and connections
// die), PropertiesChanged emission, ObjectManager truth, exact error reply
// names, deferred method replies, and owner loss/replacement through a fresh
// unique connection. It never touches the host system bus.
class FakeBluez : public QObject
{
    Q_OBJECT

public:
    enum class PairingMode {
        ConfirmPasskey,
        RequestPasskey,
        RequestPin,
        DisplayPasskey,
        DisplayPin,
        AuthorizeService,
    };

    struct AdapterEntity
    {
        QString address;
        QString alias;
        QString rawAddress;
        bool powered = false;
        bool discovering = false;
        // A discovery session another (non-QindaQt) BlueZ client holds; the
        // adapter stays discovering no matter what our sessions do.
        bool externalSession = false;
        QSet<QString> discoveryCallers;
    };

    struct DeviceEntity
    {
        QString adapterPath;
        QString address;
        QString alias;
        QString rawAddress;
        quint32 deviceClass = 0;
        QString icon;
        bool paired = false;
        bool trusted = false;
        bool connected = false;
        bool rssiKnown = false;
        qint16 rssi = 0;
        QString connectError;
        QString disconnectError;
        bool deferConnect = false;
        QList<QDBusMessage> deferredConnectRequests;
        PairingMode pairingMode = PairingMode::ConfirmPasskey;
        quint32 pairingPasskey = 123456;
        quint16 entered = 0;
        QString pairingPin = QStringLiteral("123456");
        QString serviceUuid = QStringLiteral("00001124-0000-1000-8000-00805f9b34fb");
        QList<QDBusMessage> deferredPairRequests;
    };

    explicit FakeBluez(const QString &busAddress, QObject *parent = nullptr);
    ~FakeBluez() override;

    FakeBluez(const FakeBluez &) = delete;
    FakeBluez &operator=(const FakeBluez &) = delete;

    // Ownership lifecycle. returnAsNewOwner models a restarted daemon: a
    // fresh unique connection whose runtime state (sessions, connections,
    // discovery) is gone.
    bool takeOwnership();
    void dropOwnership();
    bool returnAsNewOwner();
    [[nodiscard]] bool ownsService() const noexcept { return m_ownsService; }

    // Register the entity and announce it through InterfacesAdded.
    [[nodiscard]] QString addAdapter(const QString &id, const QString &address,
                                     const QString &alias, bool powered);
    [[nodiscard]] QString addDevice(const QString &adapterPath,
                                    const QString &address, const QString &alias);
    void removeAdapterObject(const QString &path);
    void removeDeviceObject(const QString &path);

    [[nodiscard]] AdapterEntity *adapter(const QString &path);
    [[nodiscard]] DeviceEntity *device(const QString &path);

    void setAdapterPowered(const QString &path, bool powered);
    void setExternalDiscovery(const QString &path, bool active);
    void emitAdapterProperties(const QString &path, const QVariantMap &changed);
    void emitDeviceProperties(const QString &path, const QVariantMap &changed);
    void emitInterfacesAdded(const QString &path,
                             const FakeBluezInterfaces &interfaces);
    void emitInterfacesRemoved(const QString &path, const QStringList &interfaces);
    void replyDeferredConnects(const QString &errorName);

    int startDiscoveryCalls = 0;
    int stopDiscoveryCalls = 0;
    int connectCalls = 0;
    int disconnectCalls = 0;
    int registerAgentCalls = 0;
    int pairCalls = 0;
    int cancelPairingCalls = 0;
    int removeDeviceCalls = 0;
    QString registeredCapability;
    QString lastPinReply;
    quint32 lastPasskeyReply = 0;

private:
    friend class FakeBluezServiceObject;

    void registerObjects();
    void unregisterObjects();
    void adapterStartDiscovery(const QString &path, const QDBusMessage &request);
    void adapterStopDiscovery(const QString &path, const QDBusMessage &request);
    void deviceConnect(const QString &path, const QDBusMessage &request);
    void deviceDisconnect(const QString &path, const QDBusMessage &request);
    void registerAgent(const QDBusMessage &request);
    void devicePair(const QString &path, const QDBusMessage &request);
    void deviceCancelPairing(const QString &path, const QDBusMessage &request);
    void adapterRemoveDevice(const QString &path, const QDBusMessage &request);
    void dispatchAgentRequest(const QString &devicePath);
    void propertySet(const QString &path, const QString &interfaceName,
                     const QString &name, const QVariant &value,
                     const QDBusMessage &request);
    void publishConsequencesOfPowerOff(const QString &path);
    [[nodiscard]] FakeBluezObjectTree objectTree() const;
    void sendReply(const QDBusMessage &request);
    void sendError(const QDBusMessage &request, const QString &name,
                   const QString &text);

    QDBusConnection m_connection{QStringLiteral("invalid")};
    QString m_busAddress;
    QString m_connectionName;
    QHash<QString, AdapterEntity> m_adapters;
    QHash<QString, DeviceEntity> m_devices;
    QObject *m_nodes = nullptr;
    bool m_ownsService = false;
    QString m_agentOwner;
    QString m_agentPath;
};

} // namespace QindaQt::Tests
