// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_bluez.h"

#include <QtCore/QObject>
#include <QtCore/QUuid>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>

#include <utility>

namespace QindaQt::Tests
{
namespace
{

constexpr QLatin1StringView kBluezServiceName{"org.bluez"};
constexpr QLatin1StringView kAdapterInterface{"org.bluez.Adapter1"};
constexpr QLatin1StringView kDeviceInterface{"org.bluez.Device1"};

QString reportedAddress(const QString &raw, const QString &canonical)
{
    return raw.isEmpty() ? canonical : raw;
}

} // namespace

FakeBluezServiceObject::FakeBluezServiceObject(FakeBluez *bluez, QObject *parent)
    : QDBusVirtualObject(parent), m_bluez(bluez)
{
}

QString FakeBluezServiceObject::introspect(const QString &path) const
{
    Q_UNUSED(path)
    // The production adapter never introspects; a non-empty document keeps
    // generic D-Bus tools usable against the fake during diagnosis.
    return QStringLiteral("<node/>");
}

bool FakeBluezServiceObject::handleMessage(const QDBusMessage &message,
                                           const QDBusConnection &connection)
{
    Q_UNUSED(connection)
    const QString member = message.member();
    const QString path = message.path();
    const QString interfaceName = message.interface();
    if (message.type() != QDBusMessage::MethodCallMessage) {
        return false;
    }
    if (path == QLatin1String("/") && interfaceName == QLatin1String("org.freedesktop.DBus.ObjectManager")
        && member == QLatin1String("GetManagedObjects")) {
        connection.send(
            message.createReply({QVariant::fromValue(m_bluez->objectTree())}));
        return true;
    }
    if (path == QLatin1String("/org/bluez")
        && interfaceName == QLatin1String("org.bluez.AgentManager1")
        && member == QLatin1String("RegisterAgent")) {
        m_bluez->registerAgent(message);
        return true;
    }
    if (interfaceName == QLatin1String("org.freedesktop.DBus.Properties")
        && member == QLatin1String("Set") && message.arguments().size() == 3) {
        m_bluez->propertySet(path, message.arguments().value(0).toString(),
                             message.arguments().value(1).toString(),
                             message.arguments().value(2).value<QDBusVariant>().variant(),
                             message);
        return true;
    }
    if (interfaceName == QLatin1String("org.bluez.Adapter1")) {
        if (member == QLatin1String("StartDiscovery")) {
            m_bluez->adapterStartDiscovery(path, message);
            return true;
        }
        if (member == QLatin1String("StopDiscovery")) {
            m_bluez->adapterStopDiscovery(path, message);
            return true;
        }
        if (member == QLatin1String("RemoveDevice")) {
            m_bluez->adapterRemoveDevice(path, message);
            return true;
        }
    }
    if (interfaceName == QLatin1String("org.bluez.Device1")) {
        if (member == QLatin1String("Connect")) {
            m_bluez->deviceConnect(path, message);
            return true;
        }
        if (member == QLatin1String("Disconnect")) {
            m_bluez->deviceDisconnect(path, message);
            return true;
        }
        if (member == QLatin1String("Pair")) {
            m_bluez->devicePair(path, message);
            return true;
        }
        if (member == QLatin1String("CancelPairing")) {
            m_bluez->deviceCancelPairing(path, message);
            return true;
        }
    }
    connection.send(message.createErrorReply(
        QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
        QStringLiteral("Unknown method on the fake org.bluez service")));
    return true;
}

FakeBluez::FakeBluez(const QString &busAddress, QObject *parent)
    : QObject(parent), m_busAddress(busAddress)
{
    qDBusRegisterMetaType<FakeBluezInterfaces>();
    qDBusRegisterMetaType<FakeBluezObjectTree>();
    m_nodes = new QObject(this);
}

FakeBluez::~FakeBluez()
{
    dropOwnership();
}

bool FakeBluez::takeOwnership()
{
    if (m_ownsService) {
        return true;
    }
    if (!m_connection.isConnected()) {
        m_connectionName = QStringLiteral("fake-bluez-%1")
                               .arg(QUuid::createUuid().toString(QUuid::Id128));
        m_connection = QDBusConnection::connectToBus(m_busAddress, m_connectionName);
    }
    if (!m_connection.isConnected() || !m_connection.registerService(QString(kBluezServiceName))) {
        return false;
    }
    m_ownsService = true;
    registerObjects();
    return true;
}

void FakeBluez::dropOwnership()
{
    if (!m_ownsService) {
        return;
    }
    m_ownsService = false;
    unregisterObjects();
    m_connection.unregisterService(QString(kBluezServiceName));
    if (!m_connectionName.isEmpty()) {
        QDBusConnection::disconnectFromBus(m_connectionName);
        m_connection = QDBusConnection{QStringLiteral("invalid")};
        m_connectionName.clear();
    }
}

bool FakeBluez::returnAsNewOwner()
{
    dropOwnership();
    // A restarted daemon comes back with no runtime state: discovery sessions,
    // connections, and deferred replies do not survive the process.
    for (auto it = m_adapters.begin(); it != m_adapters.end(); ++it) {
        it.value().discovering = false;
        it.value().externalSession = false;
        it.value().discoveryCallers.clear();
    }
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        it.value().connected = false;
        it.value().deferConnect = false;
        it.value().deferredConnectRequests.clear();
    }
    startDiscoveryCalls = 0;
    stopDiscoveryCalls = 0;
    connectCalls = 0;
    disconnectCalls = 0;
    registerAgentCalls = 0;
    pairCalls = 0;
    cancelPairingCalls = 0;
    removeDeviceCalls = 0;
    m_agentOwner.clear();
    m_agentPath.clear();
    registeredCapability.clear();
    return takeOwnership();
}

void FakeBluez::registerObjects()
{
    auto *service = new FakeBluezServiceObject(this, m_nodes);
    m_connection.registerVirtualObject(QStringLiteral("/"), service,
                                       QDBusConnection::SubPath);
}

void FakeBluez::unregisterObjects()
{
    m_connection.unregisterObject(QStringLiteral("/"),
                                  QDBusConnection::UnregisterTree);
}

QString FakeBluez::addAdapter(const QString &id, const QString &address,
                              const QString &alias, const bool powered)
{
    const QString path = QStringLiteral("/org/bluez/") + id;
    AdapterEntity entity;
    entity.address = address;
    entity.alias = alias;
    entity.powered = powered;
    m_adapters.insert(path, entity);
    FakeBluezInterfaces interfaces;
    interfaces.insert(QString(kAdapterInterface),
                      {{QStringLiteral("Address"), reportedAddress(entity.rawAddress, address)},
                       {QStringLiteral("Alias"), alias},
                       {QStringLiteral("Name"), alias},
                       {QStringLiteral("Powered"), powered},
                       {QStringLiteral("Discovering"), false}});
    if (m_ownsService) {
        emitInterfacesAdded(path, interfaces);
    }
    return path;
}

QString FakeBluez::addDevice(const QString &adapterPath, const QString &address,
                             const QString &alias)
{
    // BlueZ object paths use underscore-separated address bytes; ':' is not a
    // legal object-path character.
    QString addressId = address;
    addressId.replace(QLatin1Char(':'), QLatin1Char('_'));
    const QString path = adapterPath + QStringLiteral("/dev_") + addressId;
    DeviceEntity entity;
    entity.adapterPath = adapterPath;
    entity.address = address;
    entity.alias = alias;
    m_devices.insert(path, entity);
    FakeBluezInterfaces interfaces;
    QVariantMap properties;
    properties.insert(QStringLiteral("Address"), address);
    properties.insert(QStringLiteral("Alias"), alias);
    properties.insert(QStringLiteral("Name"), alias);
    properties.insert(QStringLiteral("Paired"), false);
    properties.insert(QStringLiteral("Trusted"), false);
    properties.insert(QStringLiteral("Connected"), false);
    properties.insert(QStringLiteral("Adapter"),
                      QVariant::fromValue(QDBusObjectPath(adapterPath)));
    interfaces.insert(QString(kDeviceInterface), properties);
    if (m_ownsService) {
        emitInterfacesAdded(path, interfaces);
    }
    return path;
}

void FakeBluez::removeAdapterObject(const QString &path)
{
    if (!m_adapters.contains(path)) {
        return;
    }
    if (m_ownsService) {
        m_connection.unregisterObject(path);
        emitInterfacesRemoved(path, {QString(kAdapterInterface)});
    }
    m_adapters.remove(path);
}

void FakeBluez::removeDeviceObject(const QString &path)
{
    if (!m_devices.contains(path)) {
        return;
    }
    if (m_ownsService) {
        m_connection.unregisterObject(path);
        emitInterfacesRemoved(path, {QString(kDeviceInterface)});
    }
    m_devices.remove(path);
}

FakeBluez::AdapterEntity *FakeBluez::adapter(const QString &path)
{
    const auto it = m_adapters.find(path);
    return it == m_adapters.end() ? nullptr : &it.value();
}

FakeBluez::DeviceEntity *FakeBluez::device(const QString &path)
{
    const auto it = m_devices.find(path);
    return it == m_devices.end() ? nullptr : &it.value();
}

void FakeBluez::emitAdapterProperties(const QString &path, const QVariantMap &changed)
{
    QDBusMessage signal = QDBusMessage::createSignal(
        path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    signal.setArguments(
        {QString(kAdapterInterface), changed, QStringList{}});
    m_connection.send(signal);
}

void FakeBluez::emitDeviceProperties(const QString &path, const QVariantMap &changed)
{
    QDBusMessage signal = QDBusMessage::createSignal(
        path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    signal.setArguments({QString(kDeviceInterface), changed, QStringList{}});
    m_connection.send(signal);
}

void FakeBluez::emitInterfacesAdded(const QString &path,
                                    const FakeBluezInterfaces &interfaces)
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"));
    signal.setArguments(
        {QVariant::fromValue(QDBusObjectPath(path)), QVariant::fromValue(interfaces)});
    m_connection.send(signal);
}

void FakeBluez::emitInterfacesRemoved(const QString &path,
                                      const QStringList &interfaces)
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"));
    signal.setArguments(
        {QVariant::fromValue(QDBusObjectPath(path)), interfaces});
    m_connection.send(signal);
}

void FakeBluez::setAdapterPowered(const QString &path, const bool powered)
{
    AdapterEntity *entity = adapter(path);
    if (entity == nullptr || entity->powered == powered) {
        return;
    }
    entity->powered = powered;
    emitAdapterProperties(path, {{QStringLiteral("Powered"), powered}});
    if (!powered) {
        publishConsequencesOfPowerOff(path);
    }
}

void FakeBluez::publishConsequencesOfPowerOff(const QString &path)
{
    AdapterEntity *entity = adapter(path);
    if (entity == nullptr) {
        return;
    }
    if (entity->discovering) {
        entity->discovering = false;
        entity->externalSession = false;
        entity->discoveryCallers.clear();
        emitAdapterProperties(path, {{QStringLiteral("Discovering"), false}});
    }
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it.value().adapterPath == path && it.value().connected) {
            it.value().connected = false;
            emitDeviceProperties(it.key(), {{QStringLiteral("Connected"), false}});
        }
    }
}

void FakeBluez::setExternalDiscovery(const QString &path, const bool active)
{
    AdapterEntity *entity = adapter(path);
    if (entity == nullptr || entity->externalSession == active) {
        return;
    }
    entity->externalSession = active;
    entity->discovering = active || !entity->discoveryCallers.isEmpty();
    emitAdapterProperties(path, {{QStringLiteral("Discovering"), entity->discovering}});
}

void FakeBluez::replyDeferredConnects(const QString &errorName)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        for (const QDBusMessage &request : std::as_const(it.value().deferredConnectRequests)) {
            if (errorName.isEmpty()) {
                sendReply(request);
            } else {
                sendError(request, errorName, QStringLiteral("deferred reply"));
            }
        }
        it.value().deferredConnectRequests.clear();
    }
}

void FakeBluez::adapterStartDiscovery(const QString &path, const QDBusMessage &request)
{
    ++startDiscoveryCalls;
    AdapterEntity *entity = adapter(path);
    if (entity == nullptr) {
        sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                  QStringLiteral("Adapter does not exist"));
        return;
    }
    if (!entity->powered) {
        sendError(request, QStringLiteral("org.bluez.Error.NotReady"),
                  QStringLiteral("Adapter is not powered"));
        return;
    }
    if (entity->discoveryCallers.contains(request.service())) {
        sendError(request, QStringLiteral("org.bluez.Error.AlreadyExists"),
                  QStringLiteral("Discovery session already active"));
        return;
    }
    entity->discoveryCallers.insert(request.service());
    if (!entity->discovering) {
        entity->discovering = true;
        emitAdapterProperties(path, {{QStringLiteral("Discovering"), true}});
    }
    sendReply(request);
}

void FakeBluez::adapterStopDiscovery(const QString &path, const QDBusMessage &request)
{
    ++stopDiscoveryCalls;
    AdapterEntity *entity = adapter(path);
    if (entity == nullptr) {
        sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                  QStringLiteral("Adapter does not exist"));
        return;
    }
    if (!entity->discoveryCallers.contains(request.service())) {
        sendError(request, QStringLiteral("org.bluez.Error.Failed"),
                  QStringLiteral("No discovery started"));
        return;
    }
    entity->discoveryCallers.remove(request.service());
    if (entity->discoveryCallers.isEmpty() && !entity->externalSession
        && entity->discovering) {
        entity->discovering = false;
        emitAdapterProperties(path, {{QStringLiteral("Discovering"), false}});
    }
    sendReply(request);
}

void FakeBluez::deviceConnect(const QString &path, const QDBusMessage &request)
{
    ++connectCalls;
    DeviceEntity *entity = device(path);
    if (entity == nullptr) {
        sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                  QStringLiteral("Device does not exist"));
        return;
    }
    if (entity->deferConnect) {
        entity->deferredConnectRequests.append(request);
        return;
    }
    if (entity->connected) {
        sendError(request, QStringLiteral("org.bluez.Error.AlreadyConnected"),
                  QStringLiteral("Already connected"));
        return;
    }
    if (!entity->connectError.isEmpty()) {
        sendError(request, entity->connectError, QStringLiteral("Connect failed"));
        return;
    }
    entity->connected = true;
    emitDeviceProperties(path, {{QStringLiteral("Connected"), true}});
    sendReply(request);
}

void FakeBluez::deviceDisconnect(const QString &path, const QDBusMessage &request)
{
    ++disconnectCalls;
    DeviceEntity *entity = device(path);
    if (entity == nullptr) {
        sendError(request, QStringLiteral("org.bluez.Error.DoesNotExist"),
                  QStringLiteral("Device does not exist"));
        return;
    }
    if (!entity->disconnectError.isEmpty()) {
        sendError(request, entity->disconnectError,
                  QStringLiteral("Disconnect failed"));
        return;
    }
    if (!entity->connected) {
        sendError(request, QStringLiteral("org.bluez.Error.NotConnected"),
                  QStringLiteral("Not connected"));
        return;
    }
    entity->connected = false;
    emitDeviceProperties(path, {{QStringLiteral("Connected"), false}});
    sendReply(request);
}

FakeBluezObjectTree FakeBluez::objectTree() const
{
    FakeBluezObjectTree objects;
    for (auto it = m_adapters.cbegin(); it != m_adapters.cend(); ++it) {
        QVariantMap properties;
        properties.insert(QStringLiteral("Address"),
                          reportedAddress(it.value().rawAddress, it.value().address));
        properties.insert(QStringLiteral("Alias"), it.value().alias);
        properties.insert(QStringLiteral("Name"), it.value().alias);
        properties.insert(QStringLiteral("Powered"), it.value().powered);
        properties.insert(QStringLiteral("Discovering"), it.value().discovering);
        FakeBluezInterfaces interfaces;
        interfaces.insert(QString(kAdapterInterface), properties);
        objects.insert(QDBusObjectPath(it.key()), interfaces);
    }
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        QVariantMap properties;
        properties.insert(QStringLiteral("Address"),
                          reportedAddress(it.value().rawAddress, it.value().address));
        properties.insert(QStringLiteral("Alias"), it.value().alias);
        properties.insert(QStringLiteral("Name"), it.value().alias);
        properties.insert(QStringLiteral("Class"), it.value().deviceClass);
        properties.insert(QStringLiteral("Icon"), it.value().icon);
        properties.insert(QStringLiteral("Paired"), it.value().paired);
        properties.insert(QStringLiteral("Trusted"), it.value().trusted);
        properties.insert(QStringLiteral("Connected"), it.value().connected);
        if (it.value().rssiKnown) {
            properties.insert(QStringLiteral("RSSI"), it.value().rssi);
        }
        properties.insert(QStringLiteral("Adapter"),
                          QVariant::fromValue(QDBusObjectPath(it.value().adapterPath)));
        FakeBluezInterfaces interfaces;
        interfaces.insert(QString(kDeviceInterface), properties);
        objects.insert(QDBusObjectPath(it.key()), interfaces);
    }
    return objects;
}

void FakeBluez::sendReply(const QDBusMessage &request)
{
    m_connection.send(request.createReply());
}

void FakeBluez::sendError(const QDBusMessage &request, const QString &name,
                          const QString &text)
{
    m_connection.send(request.createErrorReply(name, text));
}

} // namespace QindaQt::Tests
