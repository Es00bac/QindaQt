// SPDX-License-Identifier: LGPL-3.0-or-later

#include "bluez_transport.h"

#include <QtCore/QVariant>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusServiceWatcher>

#include <utility>

namespace QindaQt::Bluetooth::Bluez
{
namespace
{

constexpr QLatin1StringView kBluezServiceName{"org.bluez"};
constexpr QLatin1StringView kAdapterInterface{"org.bluez.Adapter1"};
constexpr QLatin1StringView kDeviceInterface{"org.bluez.Device1"};
constexpr QLatin1StringView kPropertiesInterface{"org.freedesktop.DBus.Properties"};
constexpr QLatin1StringView kObjectManagerInterface{
    "org.freedesktop.DBus.ObjectManager"};
// AGENT-GUARD: Hostile BlueZ payloads are bounded at parse time; a managed
// object flood is dropped whole instead of exhausting memory. The inventory
// projection applies the much tighter Bluetooth1 caps afterwards.
constexpr qsizetype kMaxParsedObjects = 8192;

bool holdsArgument(const QVariant &value)
{
    return value.metaType() == QMetaType::fromType<QDBusArgument>();
}

QString objectPathString(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    return value.canConvert<QString>() ? value.toString() : QString{};
}

QStringList parseStringList(const QVariant &value)
{
    if (holdsArgument(value)) {
        QStringList list;
        value.value<QDBusArgument>() >> list;
        return list;
    }
    return value.toStringList();
}

QVariantMap parseVariantMap(const QVariant &value)
{
    if (holdsArgument(value)) {
        QVariantMap map;
        value.value<QDBusArgument>() >> map;
        return map;
    }
    return value.toMap();
}

BluezInterfaces parseInterfaces(const QVariant &value)
{
    if (holdsArgument(value)) {
        BluezInterfaces interfaces;
        value.value<QDBusArgument>() >> interfaces;
        return interfaces;
    }
    BluezInterfaces interfaces;
    const QVariantMap map = value.toMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        interfaces.insert(it.key(), parseVariantMap(it.value()));
    }
    return interfaces;
}

BluezManagedObjects parseManagedObjects(const QVariant &value)
{
    BluezManagedObjects objects;
    if (!holdsArgument(value)) {
        return objects;
    }
    const QDBusArgument argument = value.value<QDBusArgument>();
    argument.beginMap();
    while (!argument.atEnd() && objects.size() < kMaxParsedObjects) {
        QString path;
        BluezInterfaces interfaces;
        argument.beginMapEntry();
        argument >> path >> interfaces;
        argument.endMapEntry();
        objects.insert(path, interfaces);
    }
    argument.endMap();
    return objects;
}

QDBusMessage bluezCall(const QString &owner, const QString &path,
                       const QString &interfaceName, const QString &member)
{
    // The exact unique owner is the destination; the well-known name could
    // rebind between resolution and delivery.
    return QDBusMessage::createMethodCall(owner, path, interfaceName, member);
}

} // namespace

BluezTransport::BluezTransport(const QDBusConnection &connection, QObject *parent)
    : QObject(parent), m_connection(connection)
{
}

BluezTransport::~BluezTransport()
{
    stop();
}

void BluezTransport::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_watcher = std::make_unique<QDBusServiceWatcher>(
        QString(kBluezServiceName), m_connection,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
            &BluezTransport::onOwnerWatchChanged);
    queryInitialOwner();
}

void BluezTransport::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    // AGENT-GUARD: Advancing the token fences every reply and fetch that is
    // still in flight; a stopped transport must never deliver platform truth.
    ++m_ownerToken;
    uninstallMatches();
    m_watcher.reset();
    m_owner.clear();
    m_ownerResolved = false;
}

void BluezTransport::requestManagedObjects()
{
    if (!m_running || m_owner.isEmpty()) {
        return;
    }
    const quint64 token = m_ownerToken;
    QDBusMessage message = bluezCall(m_owner, QStringLiteral("/"),
                                     QString(kObjectManagerInterface),
                                     QStringLiteral("GetManagedObjects"));
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, token](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                if (!m_running || token != m_ownerToken) {
                    return;
                }
                const QDBusMessage response = watcher->reply();
                // AGENT-GUARD: A failed enumeration fails closed to "no
                // objects observed"; stale or partial truth is never kept.
                const QVariantList arguments = response.arguments();
                qWarning() << "PROBE reply type" << response.type() << "signature"
                           << response.signature() << "error" << response.errorName()
                           << "args" << arguments.size();
                if (!arguments.isEmpty()) {
                    qWarning() << "PROBE arg0 metatype"
                               << arguments.value(0).metaType().name();
                }
                const BluezManagedObjects objects =
                    response.type() == QDBusMessage::ReplyMessage
                        ? parseManagedObjects(arguments.value(0))
                        : BluezManagedObjects{};
                qWarning() << "PROBE parsed objects" << objects.size();
                Q_EMIT managedObjectsReady(objects);
            });
}

void BluezTransport::queryInitialOwner()
{
    if (!m_running) {
        return;
    }
    if (!m_connection.isConnected()) {
        // A bus-less transport must still publish its initial empty state so
        // the model can leave Starting behind.
        adoptOwner({});
        return;
    }
    const quint64 token = m_ownerToken;
    QDBusMessage message =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                       QStringLiteral("/org/freedesktop/DBus"),
                                       QStringLiteral("org.freedesktop.DBus"),
                                       QStringLiteral("GetNameOwner"));
    message.setArguments({QString(kBluezServiceName)});
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, token](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                if (!m_running || token != m_ownerToken) {
                    return;
                }
                const QDBusMessage response = watcher->reply();
                const QString owner =
                    response.type() == QDBusMessage::ReplyMessage
                        ? response.arguments().value(0).toString()
                        : QString{};
                adoptOwner(owner);
            });
}

void BluezTransport::onOwnerWatchChanged(const QString &serviceName,
                                         const QString &oldOwner,
                                         const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (m_running && serviceName == QString(kBluezServiceName)) {
        adoptOwner(newOwner);
    }
}

void BluezTransport::adoptOwner(const QString &owner)
{
    if (m_ownerResolved && owner == m_owner) {
        return;
    }
    uninstallMatches();
    m_owner = owner;
    m_ownerResolved = true;
    ++m_ownerToken;
    if (!m_owner.isEmpty()) {
        installMatches();
        requestManagedObjects();
    }
    Q_EMIT ownerChanged(m_owner);
}

void BluezTransport::installMatches()
{
    if (m_matchesInstalled || m_owner.isEmpty() || !m_connection.isConnected()) {
        return;
    }
    m_matchesInstalled = true;
    const QString owner = m_owner;
    m_connection.connect(owner, QStringLiteral("/"), QString(kObjectManagerInterface),
                         QStringLiteral("InterfacesAdded"), this,
                         SLOT(onObjectManagerSignal(QDBusMessage)));
    m_connection.connect(owner, QStringLiteral("/"), QString(kObjectManagerInterface),
                         QStringLiteral("InterfacesRemoved"), this,
                         SLOT(onObjectManagerSignal(QDBusMessage)));
    // Path-less match: BlueZ emits PropertiesChanged per object; the
    // interface name is filtered in the slot, so other BlueZ interfaces on
    // the same owner arrive but never reach the backend.
    m_connection.connect(owner, QString{}, QString(kPropertiesInterface),
                         QStringLiteral("PropertiesChanged"), this,
                         SLOT(onPropertiesSignal(QDBusMessage)));
}

void BluezTransport::uninstallMatches()
{
    if (!m_matchesInstalled) {
        return;
    }
    m_matchesInstalled = false;
    const QString owner = m_owner;
    m_connection.disconnect(owner, QStringLiteral("/"),
                            QString(kObjectManagerInterface),
                            QStringLiteral("InterfacesAdded"), this,
                            SLOT(onObjectManagerSignal(QDBusMessage)));
    m_connection.disconnect(owner, QStringLiteral("/"),
                            QString(kObjectManagerInterface),
                            QStringLiteral("InterfacesRemoved"), this,
                            SLOT(onObjectManagerSignal(QDBusMessage)));
    m_connection.disconnect(owner, QString{}, QString(kPropertiesInterface),
                            QStringLiteral("PropertiesChanged"), this,
                            SLOT(onPropertiesSignal(QDBusMessage)));
}

void BluezTransport::onObjectManagerSignal(const QDBusMessage &message)
{
    if (!m_running) {
        return;
    }
    const QVariantList arguments = message.arguments();
    if (arguments.size() < 2) {
        return;
    }
    const QString path = objectPathString(arguments.at(0));
    if (message.member() == QLatin1String("InterfacesAdded")) {
        Q_EMIT interfacesAdded(path, parseInterfaces(arguments.at(1)));
    } else if (message.member() == QLatin1String("InterfacesRemoved")) {
        Q_EMIT interfacesRemoved(path, parseStringList(arguments.at(1)));
    }
}

void BluezTransport::onPropertiesSignal(const QDBusMessage &message)
{
    if (!m_running) {
        return;
    }
    const QVariantList arguments = message.arguments();
    if (arguments.size() < 3) {
        return;
    }
    const QString interfaceName = arguments.at(0).toString();
    if (interfaceName != QString(kAdapterInterface)
        && interfaceName != QString(kDeviceInterface)) {
        return;
    }
    Q_EMIT propertiesChanged(message.path(), interfaceName,
                             parseVariantMap(arguments.at(1)),
                             parseStringList(arguments.at(2)));
}

quint64 BluezTransport::beginCall(const QDBusMessage &message)
{
    if (!m_running || m_owner.isEmpty()) {
        return 0;
    }
    const quint64 callId = m_nextCallId++;
    if (m_nextCallId == 0) {
        m_nextCallId = 1;
    }
    const quint64 token = m_ownerToken;
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, callId, token](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                if (!m_running || token != m_ownerToken) {
                    return;
                }
                const QDBusMessage response = watcher->reply();
                if (response.type() == QDBusMessage::ReplyMessage) {
                    Q_EMIT callFinished(callId, true, QString{}, QString{});
                } else {
                    Q_EMIT callFinished(callId, false, response.errorName(),
                                        response.errorMessage());
                }
            });
    return callId;
}

quint64 BluezTransport::setAdapterPowered(const QString &adapterPath,
                                          const bool powered)
{
    QDBusMessage message =
        bluezCall(m_owner, adapterPath, QString(kPropertiesInterface),
                  QStringLiteral("Set"));
    message.setArguments({QString(kAdapterInterface), QStringLiteral("Powered"),
                          QVariant::fromValue(QDBusVariant(powered))});
    return beginCall(message);
}

quint64 BluezTransport::startDiscovery(const QString &adapterPath)
{
    return beginCall(bluezCall(m_owner, adapterPath, QString(kAdapterInterface),
                               QStringLiteral("StartDiscovery")));
}

quint64 BluezTransport::stopDiscovery(const QString &adapterPath)
{
    return beginCall(bluezCall(m_owner, adapterPath, QString(kAdapterInterface),
                               QStringLiteral("StopDiscovery")));
}

quint64 BluezTransport::connectDevice(const QString &devicePath)
{
    return beginCall(bluezCall(m_owner, devicePath, QString(kDeviceInterface),
                               QStringLiteral("Connect")));
}

quint64 BluezTransport::disconnectDevice(const QString &devicePath)
{
    return beginCall(bluezCall(m_owner, devicePath, QString(kDeviceInterface),
                               QStringLiteral("Disconnect")));
}

} // namespace QindaQt::Bluetooth::Bluez
