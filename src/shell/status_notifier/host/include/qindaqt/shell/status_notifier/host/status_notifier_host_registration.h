// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtDBus/QDBusConnection>

#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::StatusNotifier
{

// AGENT-CONTRACT: the StatusNotifierItem specification's HOST side of
// registration, which is a different role from the watcher this process also
// serves. A conformant item asks the watcher whether any host is present
// (`IsStatusNotifierHostRegistered`, plus the `StatusNotifierHostRegistered`
// signal) and is entitled to hide its icon, refuse to register, or fall back
// to the legacy XEmbed tray when the answer is no. Until this component
// existed, QindaQt served the watcher but never announced a host, so
// `IsStatusNotifierHostRegistered` stayed false on a live session and
// well-behaved items had no reason to present themselves. See
// docs/wiki/shell/status-notifier.md and ADR-0166.
//
// Responsibility: own the conventional per-process host bus name and keep it
// registered with whichever connection owns the watcher name, including a
// watcher that appears, disappears, or is replaced later in the session.
//
// This deliberately goes through the bus rather than calling the in-process
// watcher service directly: the same code then works when the watcher is
// QindaQt's own service and when another implementation owns the name, and the
// observable protocol traffic is identical to any other conformant host.
//
// Lifetime and threading: the injected connection is borrowed and must outlive
// this object. All bus traffic stays on the thread that runs the connection;
// there is no internal synchronization. start()/stop() are idempotent and the
// destructor stops.
class StatusNotifierHostRegistration : public QObject
{
    Q_OBJECT

public:
    explicit StatusNotifierHostRegistration(QDBusConnection connection,
                                            QObject *parent = nullptr);
    // Test and multi-instance seam: an explicit host name instead of the
    // conventional per-process one. Two hosts in one process cannot both own
    // the pid-derived name on the same bus.
    StatusNotifierHostRegistration(QDBusConnection connection,
                                   QString hostServiceName,
                                   QObject *parent = nullptr);
    ~StatusNotifierHostRegistration() override;

    StatusNotifierHostRegistration(const StatusNotifierHostRegistration &) = delete;
    StatusNotifierHostRegistration &operator=(const StatusNotifierHostRegistration &) = delete;

    // The conventional host name for this process:
    // `org.kde.StatusNotifierHost-<pid>`. Items and other desktops look for
    // exactly this shape, so it is not an arbitrary private name.
    [[nodiscard]] static QString conventionalHostServiceName();

    // Claims the host name and registers it with the watcher. Returns false
    // only when the name could not be owned at all; a watcher that is not on
    // the bus yet is NOT a failure - the registration is retried when the
    // watcher name gains an owner, which is the normal shell startup order.
    [[nodiscard]] bool start(QString *errorMessage = nullptr);
    void stop();

    [[nodiscard]] QString hostServiceName() const;
    // True once the name is owned; independent of whether a watcher has
    // accepted the registration yet.
    [[nodiscard]] bool ownsHostName() const noexcept;
    // True once a watcher accepted RegisterStatusNotifierHost for this name.
    [[nodiscard]] bool isRegisteredWithWatcher() const noexcept;
    [[nodiscard]] QString lastError() const;

signals:
    void registeredWithWatcherChanged();

private slots:
    void handleWatcherOwnerChanged(const QString &name,
                                   const QString &oldOwner,
                                   const QString &newOwner);

private:
    void sendRegistration();

    QDBusConnection m_connection;
    QString m_hostServiceName;
    bool m_ownsHostName = false;
    bool m_registeredWithWatcher = false;
    bool m_watchingWatcherName = false;
    QString m_lastError;
};

} // namespace QindaQt::StatusNotifier
