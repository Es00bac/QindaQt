// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h>

#include "status_notifier_watcher_object_p.h"

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_validation.h>

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QRegularExpression>

#include <algorithm>

namespace QindaQt::StatusNotifier
{
namespace
{

// A well-known bus name: dot-separated elements of [A-Za-z0-9_-] where no
// element begins with a digit, at least one dot, at most 255 bytes. Unique
// names (leading ':') are validated by the foundation's unique-name rule.
[[nodiscard]] bool isPlausibleBusServiceName(const QString &name)
{
    if (name.isEmpty() || name.size() > kMaxUniqueNameUtf8Bytes || name.startsWith(QLatin1Char(':'))) {
        return false;
    }
    const QStringList elements = name.split(QLatin1Char('.'));
    if (elements.size() < 2) {
        return false;
    }
    for (const QString &element : elements) {
        if (element.isEmpty() || element.at(0).isDigit()
            || element.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] OwnerKey makeKey(const QString &uniqueName, const QString &objectPath)
{
    OwnerKey key;
    key.uniqueName = uniqueName;
    key.objectPath = objectPath;
    key.generation = 0;
    return key;
}

} // namespace

StatusNotifierWatcherService::StatusNotifierWatcherService(QDBusConnection connection,
                                                           QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
{
}

StatusNotifierWatcherService::~StatusNotifierWatcherService()
{
    stop();
}

bool StatusNotifierWatcherService::start(QString *errorMessage)
{
    if (m_state != WatcherServiceState::Stopped) {
        // Both published states are successful, stable outcomes. In
        // particular, NameOwnedElsewhere is truthful degradation rather than
        // a failed start; repeating start() must not change that result.
        return true;
    }
    m_degradedReason.clear();

    if (!m_connection.isConnected()) {
        const QString message = QStringLiteral("session bus connection is not connected");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_degradedReason = QStringLiteral("watcher-bus-disconnected");
        return false;
    }

    m_object = std::make_unique<StatusNotifierWatcherObject>(*this);
    // AGENT-GUARD: the Properties adaptor must be created before
    // registerObject and ExportAdaptors must stay in the options, otherwise
    // Properties.Get on the watcher fails with UnknownInterface (see the
    // adaptor's AGENT-NOTE). Ownership is QObject parent-child (the adaptor
    // is a child of m_object); do not wrap it in a second owning pointer or
    // m_object.reset() in stop() double-deletes it.
    new StatusNotifierWatcherPropertiesAdaptor(m_object.get());
    m_objectRegistered = m_connection.registerObject(
        QString::fromLatin1(kWatcherObjectPath),
        m_object.get(),
        QDBusConnection::ExportAdaptors | QDBusConnection::ExportAllSlots
            | QDBusConnection::ExportAllProperties);
    if (!m_objectRegistered) {
        const QString message = m_connection.lastError().message();
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_degradedReason = QStringLiteral("watcher-object-registration-failed");
        m_object.reset();
        return false;
    }

    if (m_connection.registerService(QString::fromLatin1(kWatcherServiceName))) {
        // AGENT-NOTE: the first connect argument is empty, not the bus daemon
        // name: a service filter matches the sender's owned names, and the
        // daemon's loss broadcasts are not reliably matched through its
        // well-known name on private buses (this is the project's proven
        // pattern from the resident Bluetooth service).
        m_watchingNameChanges = m_connection.connect(
            QString{},
            QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("NameOwnerChanged"),
            this,
            SLOT(handleNameOwnerChanged(QString,QString,QString)));
        if (!m_watchingNameChanges) {
            m_degradedReason = QStringLiteral("watcher-owner-watch-failed");
            m_connection.unregisterService(QString::fromLatin1(kWatcherServiceName));
            m_connection.unregisterObject(QString::fromLatin1(kWatcherObjectPath));
            m_objectRegistered = false;
            m_object.reset();
            if (errorMessage != nullptr) {
                *errorMessage = m_degradedReason;
            }
            return false;
        }
        setState(WatcherServiceState::Active);
        return true;
    }

    // The name is unavailable. Never impersonate or shadow the owner: when
    // another connection holds the name, report truthful degraded state.
    const QString owner =
        m_connection.interface()->serviceOwner(QString::fromLatin1(kWatcherServiceName));
    if (!owner.isEmpty()) {
        m_degradedReason = QStringLiteral("watcher-name-owned-elsewhere");
        setState(WatcherServiceState::NameOwnedElsewhere);
        return true;
    }

    const QString message = m_connection.lastError().message();
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    m_degradedReason = QStringLiteral("watcher-name-registration-failed");
    m_connection.unregisterObject(QString::fromLatin1(kWatcherObjectPath));
    m_objectRegistered = false;
    m_object.reset();
    return false;
}

void StatusNotifierWatcherService::stop()
{
    if (m_state == WatcherServiceState::Stopped) {
        return;
    }
    if (m_watchingNameChanges) {
        m_connection.disconnect(
            QString{},
            QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("NameOwnerChanged"),
            this,
            SLOT(handleNameOwnerChanged(QString,QString,QString)));
        m_watchingNameChanges = false;
    }
    const bool released =
        m_connection.unregisterService(QString::fromLatin1(kWatcherServiceName));
    Q_UNUSED(released)
    if (m_objectRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(kWatcherObjectPath));
        m_objectRegistered = false;
    }
    m_object.reset();
    m_itemPathsByOwner.clear();
    m_hosts.clear();
    m_degradedReason.clear();
    setState(WatcherServiceState::Stopped);
}

WatcherServiceState StatusNotifierWatcherService::state() const noexcept
{
    return m_state;
}

bool StatusNotifierWatcherService::isActive() const noexcept
{
    return m_state == WatcherServiceState::Active;
}

QString StatusNotifierWatcherService::degradedReason() const
{
    return m_degradedReason;
}

QList<OwnerKey> StatusNotifierWatcherService::registeredItems() const
{
    QList<OwnerKey> keys;
    for (auto owner = m_itemPathsByOwner.cbegin(); owner != m_itemPathsByOwner.cend();
         ++owner) {
        for (const QString &path : owner.value()) {
            keys.append(makeKey(owner.key(), path));
        }
    }
    std::sort(keys.begin(), keys.end(), [](const OwnerKey &lhs, const OwnerKey &rhs) {
        if (lhs.uniqueName != rhs.uniqueName) {
            return lhs.uniqueName < rhs.uniqueName;
        }
        return lhs.objectPath < rhs.objectPath;
    });
    return keys;
}

QStringList StatusNotifierWatcherService::registeredItemServiceIds() const
{
    QStringList ids;
    const QList<OwnerKey> keys = registeredItems();
    ids.reserve(keys.size());
    for (const OwnerKey &key : keys) {
        ids.append(key.uniqueName + key.objectPath);
    }
    return ids;
}

QStringList StatusNotifierWatcherService::registeredHosts() const
{
    QStringList hosts = m_hosts.values();
    std::sort(hosts.begin(), hosts.end());
    return hosts;
}

StatusNotifierWatcherService::RegistrationAttempt
StatusNotifierWatcherService::registerItem(const QString &callerUniqueName,
                                           const QString &serviceOrPath)
{
    if (m_state != WatcherServiceState::Active) {
        return {false, QStringLiteral("watcher-not-active")};
    }
    if (!isValidUniqueBusName(callerUniqueName)) {
        return {false, QStringLiteral("caller-not-unique-bus-name")};
    }

    QString owner = callerUniqueName;
    QString objectPath;
    if (serviceOrPath.startsWith(QLatin1Char('/'))) {
        // A bare object path registers against the caller's unique name.
        if (!isValidObjectPath(serviceOrPath)) {
            return {false, QStringLiteral("invalid-object-path")};
        }
        objectPath = serviceOrPath;
    } else {
        QString errorMessage;
        if (!resolveServiceName(serviceOrPath, &owner, &errorMessage)) {
            return {false, errorMessage};
        }
        // A service-name registration advertises the item at the conventional
        // /StatusNotifierItem path of the resolved owner's connection.
        objectPath = QStringLiteral("/StatusNotifierItem");
    }

    if (!isValidUniqueBusName(owner)) {
        return {false, QStringLiteral("owner-not-unique-bus-name")};
    }

    QStringList &paths = m_itemPathsByOwner[owner];
    if (paths.contains(objectPath)) {
        return {true, {}}; // Duplicate registration of a live key is a no-op.
    }
    qsizetype totalItems = 0;
    for (auto iterator = m_itemPathsByOwner.cbegin();
         iterator != m_itemPathsByOwner.cend();
         ++iterator) {
        totalItems += iterator.value().size();
    }
    if (totalItems >= kMaxItems) {
        return {false, QStringLiteral("watcher-item-capacity-exceeded")};
    }

    paths.append(objectPath);
    const OwnerKey key = makeKey(owner, objectPath);
    emitItemSignal(QStringLiteral("StatusNotifierItemRegistered"), key);
    emit itemRegistered(key);
    return {true, {}};
}

StatusNotifierWatcherService::RegistrationAttempt
StatusNotifierWatcherService::registerHost(const QString &serviceName)
{
    if (m_state != WatcherServiceState::Active) {
        return {false, QStringLiteral("watcher-not-active")};
    }
    QString uniqueName;
    QString errorMessage;
    if (!resolveServiceName(serviceName, &uniqueName, &errorMessage)) {
        return {false, errorMessage};
    }
    if (m_hosts.contains(uniqueName)) {
        return {true, {}};
    }
    if (m_hosts.size() >= kMaxTrackedOwners) {
        return {false, QStringLiteral("watcher-host-capacity-exceeded")};
    }
    m_hosts.insert(uniqueName);
    emitHostSignal(QStringLiteral("StatusNotifierHostRegistered"), uniqueName);
    emit hostRegistered(uniqueName);
    return {true, {}};
}

void StatusNotifierWatcherService::handleNameOwnerChanged(const QString &name,
                                                          const QString &oldOwner,
                                                          const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (!newOwner.isEmpty()) {
        return;
    }
    retireOwnerItems(name);
    retireHost(name);
}

bool StatusNotifierWatcherService::resolveServiceName(const QString &serviceName,
                                                      QString *uniqueName,
                                                      QString *errorMessage) const
{
    const bool uniqueForm = serviceName.startsWith(QLatin1Char(':'));
    if (uniqueForm ? !isValidUniqueBusName(serviceName)
                   : !isPlausibleBusServiceName(serviceName)) {
        *errorMessage = QStringLiteral("invalid-service-name");
        return false;
    }
    auto request = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                                  QStringLiteral("/org/freedesktop/DBus"),
                                                  QStringLiteral("org.freedesktop.DBus"),
                                                  QStringLiteral("GetNameOwner"));
    request << serviceName;
    const QDBusMessage ownerReply = m_connection.call(request, QDBus::Block, 5'000);
    if (ownerReply.type() != QDBusMessage::ReplyMessage) {
        *errorMessage = QStringLiteral("service-resolution-failed");
        return false;
    }
    const QString resolved = ownerReply.arguments().value(0).toString();
    if (resolved.isEmpty() || !isValidUniqueBusName(resolved)) {
        *errorMessage = QStringLiteral("unknown-service");
        return false;
    }
    *uniqueName = resolved;
    return true;
}

void StatusNotifierWatcherService::retireOwnerItems(const QString &uniqueName)
{
    const auto iterator = m_itemPathsByOwner.constFind(uniqueName);
    if (iterator == m_itemPathsByOwner.cend()) {
        return;
    }
    const QStringList paths = iterator.value();
    m_itemPathsByOwner.erase(iterator);
    for (const QString &path : paths) {
        const OwnerKey key = makeKey(uniqueName, path);
        emitItemSignal(QStringLiteral("StatusNotifierItemUnregistered"), key);
        emit itemUnregistered(key);
    }
}

void StatusNotifierWatcherService::retireHost(const QString &uniqueName)
{
    if (m_hosts.remove(uniqueName)) {
        // AGENT-GUARD: P1-1 showed that changing only the property strands
        // hosts which consume the documented four-signal watcher contract.
        // Emit the wire retirement before local observers see the state.
        emitHostSignal(QStringLiteral("StatusNotifierHostUnregistered"), uniqueName);
        emit hostUnregistered(uniqueName);
        emit stateChanged();
    }
}

void StatusNotifierWatcherService::emitItemSignal(const QString &member, const OwnerKey &key)
{
    auto message = QDBusMessage::createSignal(QString::fromLatin1(kWatcherObjectPath),
                                              QString::fromLatin1(kWatcherInterfaceName),
                                              member);
    message << QVariant::fromValue(key.uniqueName + key.objectPath);
    const bool sent = m_connection.send(message);
    Q_UNUSED(sent)
}

void StatusNotifierWatcherService::emitHostSignal(const QString &member,
                                                  const QString &uniqueName)
{
    auto message = QDBusMessage::createSignal(QString::fromLatin1(kWatcherObjectPath),
                                              QString::fromLatin1(kWatcherInterfaceName),
                                              member);
    message << QVariant::fromValue(uniqueName);
    const bool sent = m_connection.send(message);
    Q_UNUSED(sent)
}

void StatusNotifierWatcherService::setState(WatcherServiceState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

} // namespace QindaQt::StatusNotifier
