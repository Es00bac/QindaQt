// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/host/status_notifier_host_registration.h>

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include <utility>

namespace QindaQt::StatusNotifier
{

StatusNotifierHostRegistration::StatusNotifierHostRegistration(QDBusConnection connection,
                                                              QObject *parent)
    : StatusNotifierHostRegistration(std::move(connection),
                                     conventionalHostServiceName(),
                                     parent)
{
}

StatusNotifierHostRegistration::StatusNotifierHostRegistration(QDBusConnection connection,
                                                               QString hostServiceName,
                                                               QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
    , m_hostServiceName(std::move(hostServiceName))
{
}

StatusNotifierHostRegistration::~StatusNotifierHostRegistration()
{
    stop();
}

QString StatusNotifierHostRegistration::conventionalHostServiceName()
{
    return QStringLiteral("org.kde.StatusNotifierHost-%1")
        .arg(QCoreApplication::applicationPid());
}

bool StatusNotifierHostRegistration::start(QString *errorMessage)
{
    if (m_ownsHostName) {
        return true;
    }
    m_lastError.clear();

    if (!m_connection.isConnected()) {
        m_lastError = QStringLiteral("host-bus-disconnected");
        if (errorMessage != nullptr) {
            *errorMessage = m_lastError;
        }
        return false;
    }

    if (!m_connection.registerService(m_hostServiceName)) {
        // AGENT-GUARD: never proceed without owning the name. Registering a
        // name we do not hold would let the watcher retire the host on a
        // NameOwnerChanged tuple that has nothing to do with this process.
        m_lastError = QStringLiteral("host-name-registration-failed");
        if (errorMessage != nullptr) {
            *errorMessage = m_connection.lastError().isValid()
                ? m_connection.lastError().message()
                : m_lastError;
        }
        return false;
    }
    m_ownsHostName = true;

    // AGENT-GUARD: match the bus daemon as sender. A peer can emit this
    // path/interface/member tuple; an unfiltered match would let it drive
    // host re-registration at will.
    m_watchingWatcherName = m_connection.connect(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(handleWatcherOwnerChanged(QString,QString,QString)));

    // A watcher that is not up yet is expected during shell startup; the
    // NameOwnerChanged watch above completes the handshake when it arrives.
    sendRegistration();
    return true;
}

void StatusNotifierHostRegistration::stop()
{
    if (m_watchingWatcherName) {
        m_connection.disconnect(
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("NameOwnerChanged"),
            this,
            SLOT(handleWatcherOwnerChanged(QString,QString,QString)));
        m_watchingWatcherName = false;
    }
    if (m_ownsHostName) {
        const bool released = m_connection.unregisterService(m_hostServiceName);
        Q_UNUSED(released)
        m_ownsHostName = false;
    }
    if (m_registeredWithWatcher) {
        m_registeredWithWatcher = false;
        emit registeredWithWatcherChanged();
    }
}

void StatusNotifierHostRegistration::sendRegistration()
{
    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath),
        QString::fromLatin1(kWatcherInterfaceName),
        QStringLiteral("RegisterStatusNotifierHost"));
    message << QVariant(m_hostServiceName);

    // AGENT-GUARD: this must stay asynchronous. In the production shell the
    // watcher being called lives in THIS process on THIS connection, and a
    // blocking call would wait for a reply that only this thread's event loop
    // can produce.
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                const QDBusPendingReply<> reply = *call;
                call->deleteLater();
                const bool accepted = !reply.isError();
                if (!accepted) {
                    m_lastError = reply.error().name().isEmpty()
                        ? QStringLiteral("host-registration-refused")
                        : reply.error().name();
                }
                if (accepted != m_registeredWithWatcher) {
                    m_registeredWithWatcher = accepted;
                    emit registeredWithWatcherChanged();
                }
            });
}

void StatusNotifierHostRegistration::handleWatcherOwnerChanged(const QString &name,
                                                               const QString &oldOwner,
                                                               const QString &newOwner)
{
    if (name != QLatin1StringView(kWatcherServiceName)) {
        return;
    }
    Q_UNUSED(oldOwner)
    if (newOwner.isEmpty()) {
        // The watcher went away; a replacement starts with an empty host set,
        // so the registration no longer holds.
        if (m_registeredWithWatcher) {
            m_registeredWithWatcher = false;
            emit registeredWithWatcherChanged();
        }
        return;
    }
    if (!m_ownsHostName) {
        return;
    }
    sendRegistration();
}

QString StatusNotifierHostRegistration::hostServiceName() const
{
    return m_hostServiceName;
}

bool StatusNotifierHostRegistration::ownsHostName() const noexcept
{
    return m_ownsHostName;
}

bool StatusNotifierHostRegistration::isRegisteredWithWatcher() const noexcept
{
    return m_registeredWithWatcher;
}

QString StatusNotifierHostRegistration::lastError() const
{
    return m_lastError;
}

} // namespace QindaQt::StatusNotifier
