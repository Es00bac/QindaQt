// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/notifications/freedesktop_notification_server.h"

#include "freedesktop_notification_object_p.h"

#include <QDBusError>
#include <QDBusMessage>

namespace QindaQt::Services::Notifications {
namespace {

constexpr auto ObjectPath = "/org/freedesktop/Notifications";
constexpr auto InterfaceName = "org.freedesktop.Notifications";

class FreedesktopSignalBackend final : public NotificationBackend {
public:
    FreedesktopSignalBackend(QDBusConnection connection,
                             NotificationBackend *presentationBackend)
        : m_connection(std::move(connection))
        , m_presentationBackend(presentationBackend)
    {
    }

    void modelPublished(NotificationSnapshotPtr snapshot) override
    {
        if (m_presentationBackend != nullptr) {
            m_presentationBackend->modelPublished(std::move(snapshot));
        }
    }

    void notificationClosed(const NotificationCloseEvent &event) override
    {
        if (m_presentationBackend != nullptr) {
            m_presentationBackend->notificationClosed(event);
        }
        auto message = QDBusMessage::createSignal(QString::fromLatin1(ObjectPath),
                                                  QString::fromLatin1(InterfaceName),
                                                  QStringLiteral("NotificationClosed"));
        message.setArguments({event.id, quint32(event.reason)});
        const bool sent = m_connection.send(message);
        Q_UNUSED(sent)
    }

    void actionInvoked(const NotificationActionEvent &event) override
    {
        if (m_presentationBackend != nullptr) {
            m_presentationBackend->actionInvoked(event);
        }
        if (!event.activationToken.isEmpty()) {
            auto token = QDBusMessage::createSignal(QString::fromLatin1(ObjectPath),
                                                    QString::fromLatin1(InterfaceName),
                                                    QStringLiteral("ActivationToken"));
            token.setArguments({event.id, event.activationToken});
            const bool tokenSent = m_connection.send(token);
            Q_UNUSED(tokenSent)
        }

        auto message = QDBusMessage::createSignal(QString::fromLatin1(ObjectPath),
                                                  QString::fromLatin1(InterfaceName),
                                                  QStringLiteral("ActionInvoked"));
        message.setArguments({event.id, event.actionKey});
        const bool sent = m_connection.send(message);
        Q_UNUSED(sent)
    }

private:
    QDBusConnection m_connection;
    NotificationBackend *m_presentationBackend = nullptr;
};

} // namespace

class FreedesktopNotificationServer::Private final {
public:
    Private(QDBusConnection selectedConnection,
            NotificationClock &clock,
            NotificationPolicy policy,
            FreedesktopServerIdentity selectedIdentity,
            NotificationBackend *presentationBackend)
        : connection(std::move(selectedConnection))
        , identity(std::move(selectedIdentity))
        , backend(connection, presentationBackend)
        , service(clock, backend, policy)
        , object(service, identity)
    {
    }

    QDBusConnection connection;
    FreedesktopServerIdentity identity;
    FreedesktopSignalBackend backend;
    NotificationService service;
    QindaQt::Services::Notifications::Private::FreedesktopNotificationObject object;
    QString serviceName;
    QString lastError;
    bool objectRegistered = false;
};

FreedesktopNotificationServer::FreedesktopNotificationServer(
    QDBusConnection connection,
    NotificationClock &clock,
    NotificationPolicy policy,
    FreedesktopServerIdentity identity,
    NotificationBackend *presentationBackend)
    : d(std::make_unique<Private>(std::move(connection),
                                  clock,
                                  policy,
                                  std::move(identity),
                                  presentationBackend))
{
}

FreedesktopNotificationServer::~FreedesktopNotificationServer()
{
    stop();
}

bool FreedesktopNotificationServer::start(const QString &serviceName)
{
    if (isRunning()) {
        return d->serviceName == serviceName;
    }
    d->lastError.clear();
    if (!d->connection.isConnected()) {
        d->lastError = QStringLiteral("notification D-Bus connection is not connected");
        return false;
    }
    if (!d->service.isReady()) {
        d->lastError = d->service.initializationError();
        return false;
    }

    d->objectRegistered = d->connection.registerObject(
        QString::fromLatin1(ObjectPath),
        &d->object,
        QDBusConnection::ExportAllSlots);
    if (!d->objectRegistered) {
        d->lastError = d->connection.lastError().message();
        return false;
    }
    if (!d->connection.registerService(serviceName)) {
        d->lastError = d->connection.lastError().message();
        d->connection.unregisterObject(QString::fromLatin1(ObjectPath));
        d->objectRegistered = false;
        return false;
    }
    d->serviceName = serviceName;
    return true;
}

void FreedesktopNotificationServer::stop() noexcept
{
    if (!d->serviceName.isEmpty()) {
        const bool unregistered = d->connection.unregisterService(d->serviceName);
        Q_UNUSED(unregistered)
        d->serviceName.clear();
    }
    if (d->objectRegistered) {
        d->connection.unregisterObject(QString::fromLatin1(ObjectPath));
        d->objectRegistered = false;
    }
}

bool FreedesktopNotificationServer::isRunning() const noexcept
{
    return !d->serviceName.isEmpty();
}

const QString &FreedesktopNotificationServer::lastError() const noexcept
{
    return d->lastError;
}

NotificationService &FreedesktopNotificationServer::service() noexcept
{
    return d->service;
}

const NotificationService &FreedesktopNotificationServer::service() const noexcept
{
    return d->service;
}

} // namespace QindaQt::Services::Notifications
