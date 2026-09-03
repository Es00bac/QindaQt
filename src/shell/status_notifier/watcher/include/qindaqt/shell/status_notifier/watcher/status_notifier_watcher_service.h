// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_types.h>

#include <QtDBus/QDBusConnection>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QStringList>

#include <memory>

namespace QindaQt::StatusNotifier
{

class StatusNotifierWatcherObject;
class StatusNotifierWatcherPropertiesAdaptor;

enum class WatcherServiceState : quint32 {
    Stopped = 0,
    // Owns org.kde.StatusNotifierWatcher and serves registrations.
    Active = 1,
    // Another connection owns the watcher name. The service object stays
    // introspectable but refuses registrations and reports a truthful
    // degraded reason instead of impersonating the real watcher.
    NameOwnedElsewhere = 2,
};

// AGENT-CONTRACT: The QindaQt StatusNotifierWatcher service. It serves
// org.kde.StatusNotifierWatcher at /StatusNotifierWatcher on the injected
// session-bus connection: RegisterStatusNotifierItem, RegisterStatusNotifierHost,
// the RegisteredStatusNotifierItems / IsStatusNotifierHostRegistered /
// ProtocolVersion properties, and the four protocol signals.
//
// Ownership rules (ADR-0032 consequences):
// - Every item is keyed to its owner's bus unique name. A bare object path
//   argument registers against the caller's unique name; a service-name
//   argument is resolved through the bus daemon and lands on the resolved
//   owner's /StatusNotifierItem path.
// - Registered items are retired only on a bus-daemon-authenticated
//   NameOwnerChanged loss tuple for their unique owner; a well-known name can
//   never hold an item directly.
// - start() never claims a name another connection owns: it fails closed into
//   NameOwnedElsewhere with degradedReason() naming the cause, so callers can
//   present truthful Degraded state instead of a silently broken watcher.
//
// Lifetime and threading: the injected connection is not owned and must
// outlive the service. All bus and local signal traffic stays on the thread
// that runs the injected connection; there is no internal synchronization.
// start()/stop() are idempotent; the destructor stops the service.
class StatusNotifierWatcherService : public QObject
{
    Q_OBJECT

public:
    explicit StatusNotifierWatcherService(QDBusConnection connection,
                                          QObject *parent = nullptr);
    ~StatusNotifierWatcherService() override;

    // Registers the object and claims the watcher bus name. Returns false
    // only when the service could not be published at all (disconnected bus
    // or object registration failure). A name owned by another watcher is not
    // an error here: state() becomes NameOwnedElsewhere and start() returns
    // true so diagnostics remain available.
    [[nodiscard]] bool start(QString *errorMessage = nullptr);
    void stop();

    [[nodiscard]] WatcherServiceState state() const noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] QString degradedReason() const;

    // Live items in deterministic (uniqueName, objectPath) order. Consumers
    // needing event flow use the signals below; this snapshot is for probes.
    [[nodiscard]] QList<OwnerKey> registeredItems() const;
    // The wire form used by RegisteredStatusNotifierItems: "uniqueName/path".
    [[nodiscard]] QStringList registeredItemServiceIds() const;
    [[nodiscard]] QStringList registeredHosts() const;

signals:
    void itemRegistered(const QindaQt::StatusNotifier::OwnerKey &key);
    void itemUnregistered(const QindaQt::StatusNotifier::OwnerKey &key);
    void hostRegistered(const QString &uniqueName);
    void hostUnregistered(const QString &uniqueName);
    void stateChanged();

private slots:
    void handleNameOwnerChanged(const QString &name,
                                const QString &oldOwner,
                                const QString &newOwner);

private:
    friend class StatusNotifierWatcherObject;

    // Result of a registration attempt from a D-Bus call; a non-empty
    // `errorMessage` means the caller must receive an error reply.
    struct RegistrationAttempt {
        bool accepted = false;
        QString errorMessage;
    };

    [[nodiscard]] RegistrationAttempt registerItem(const QString &callerUniqueName,
                                                   const QString &serviceOrPath);
    [[nodiscard]] RegistrationAttempt registerHost(const QString &serviceName);
    [[nodiscard]] bool resolveServiceName(const QString &serviceName,
                                          QString *uniqueName,
                                          QString *errorMessage) const;
    void retireOwnerItems(const QString &uniqueName);
    void retireHost(const QString &uniqueName);
    void emitItemSignal(const QString &member, const OwnerKey &key);
    void emitHostSignal(const QString &member);
    void setState(WatcherServiceState state);

    QDBusConnection m_connection;
    std::unique_ptr<StatusNotifierWatcherObject> m_object;
    WatcherServiceState m_state = WatcherServiceState::Stopped;
    QString m_degradedReason;
    bool m_objectRegistered = false;
    bool m_watchingNameChanges = false;
    // AGENT-GUARD: Items are keyed by the owner's bus unique name. Only the
    // unique name is tracked (never a well-known name): NameOwnerChanged for
    // a registered item owner retires exactly that owner's items.
    QHash<QString, QStringList> m_itemPathsByOwner;
    QSet<QString> m_hosts;
};

} // namespace QindaQt::StatusNotifier

Q_DECLARE_METATYPE(QindaQt::StatusNotifier::WatcherServiceState)
