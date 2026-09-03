// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "bluez_object_store.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtDBus/QDBusConnection>

#include <memory>

class QDBusServiceWatcher;

namespace QindaQt::Bluetooth::Bluez
{

// Exact-owner QtDBus seam to `org.bluez`. This class owns every mechanical
// concern of the platform transport — owner resolution, match-rule lifetime,
// ObjectManager fetching, and asynchronous method calls — and publishes only
// parsed Qt values. Semantic decisions (mapping, leases, operations) belong
// to BluezAdapterBackend.
//
// AGENT-CONTRACT: Every signal match is bound to the exact unique owner of
// `org.bluez`, installed after resolving that owner, and uninstalled on owner
// loss, replacement, or stop(). Every asynchronous reply is fenced by an
// owner token that advances on any owner transition, so a late or
// replaced-owner reply is dropped instead of mutating current state. Calls
// are addressed to the unique owner, never the well-known name. See
// ADR-0057.
class BluezTransport final : public QObject
{
    Q_OBJECT

public:
    explicit BluezTransport(const QDBusConnection &connection,
                            QObject *parent = nullptr);
    ~BluezTransport() override;

    void start();
    void stop();
    void requestManagedObjects();
    [[nodiscard]] bool running() const noexcept { return m_running; }
    [[nodiscard]] QString owner() const { return m_owner; }

    // Each call returns a fresh callId delivered exactly once through
    // callFinished, unless the owner changed or the transport stopped first.
    [[nodiscard]] quint64 setAdapterPowered(const QString &adapterPath,
                                            bool powered);
    [[nodiscard]] quint64 startDiscovery(const QString &adapterPath);
    [[nodiscard]] quint64 stopDiscovery(const QString &adapterPath);
    [[nodiscard]] quint64 connectDevice(const QString &devicePath);
    [[nodiscard]] quint64 disconnectDevice(const QString &devicePath);

Q_SIGNALS:
    // Emitted on every owner transition (appearance, loss, replacement).
    // owner() is already updated; an empty owner means BlueZ is absent and
    // previously fetched truth must be retired.
    void ownerChanged(const QString &owner);
    // Empty objects mean "no objects could be observed": an exact-owner
    // snapshot that failed fail-closed to nothing, never stale truth.
    void managedObjectsReady(
        const QindaQt::Bluetooth::Bluez::BluezManagedObjects &objects);
    void interfacesAdded(const QString &path,
                         const QindaQt::Bluetooth::Bluez::BluezInterfaces &interfaces);
    void interfacesRemoved(const QString &path, const QStringList &interfaces);
    void propertiesChanged(const QString &path, const QString &interfaceName,
                           const QVariantMap &changed, const QStringList &invalidated);
    void callFinished(quint64 callId, bool succeeded, const QString &errorName,
                      const QString &errorMessage);

private Q_SLOTS:
    void onOwnerWatchChanged(const QString &serviceName, const QString &oldOwner,
                             const QString &newOwner);
    void onObjectManagerSignal(const QDBusMessage &message);
    void onPropertiesSignal(const QDBusMessage &message);

private:
    void queryInitialOwner();
    void adoptOwner(const QString &owner);
    void installMatches();
    void uninstallMatches();
    [[nodiscard]] quint64 beginCall(const QDBusMessage &message);

    QDBusConnection m_connection;
    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    QString m_owner;
    quint64 m_ownerToken = 0;
    quint64 m_nextCallId = 1;
    bool m_running = false;
    bool m_matchesInstalled = false;
    bool m_ownerResolved = false;
};

} // namespace QindaQt::Bluetooth::Bluez
